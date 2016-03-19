#include "stdafx.h"
#include "service.h"

// client main function
int __cdecl cli(int argc, TCHAR *argv[]);

VOID __stdcall DisplayUsage()
{
    printf("Description:\n");
    printf("\tCommand-line tool that controls an application.\n\n");
    printf("Usage:\n");
    printf("\tservice [command]\n\n");
    printf("\t[command]\n");
    printf("\t  install\n");
    printf("\t  start\n");
    printf("\t  stop\n");
    printf("\t  remove\n");
    printf("\t  cli (client-side)\n");
}

//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None
//
void __cdecl _tmain(int argc, TCHAR *argv[])
{
    if (argc > 1) {
        TCHAR command[10];

        StringCchCopy(command, 10, argv[1]);
        if (lstrcmpi(command, TEXT("install")) == 0) {
            printf("\nhandle install\n");
            SvcInstall();
        }
        else if (lstrcmpi(command, TEXT("start")) == 0) {
            printf("\nhandle start\n");
            SvcStart();
        }
        else if (lstrcmpi(command, TEXT("stop")) == 0) {
            printf("\nhandle stop\n");
            SvcStop();
        }
        else if (lstrcmpi(command, TEXT("remove")) == 0) {
            printf("\nhandle remove\n");
            SvcRemove();
        }
        else if (lstrcmpi(command, TEXT("cli")) == 0) {
            // the default service is 'srv' app (Stage II), i.e. without arguments
            cli(argc, argv);
        }
        else
        {
            _tprintf(TEXT("Unknown command (%s)\n\n"), command);
            DisplayUsage();
        }
        return;
    }

    // Otherwise, the service is probably being started by the SCM (Service Control Manager)

    // in this process we can cause many services (but in our case there is a single service)
    // SERVICE_TABLE_ENTRY: each element of the structure specifies the service name and the entry point for the service (this is used by SCManager)
    // the entry point of SVCNAME service is 'SvcMain' function
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }      // The members of the last entry in the table must have NULL values to designate the end of the table
    };

    // This call returns when the service (all services in DispatchTable) has stopped. 
    // The process should simply terminate when the call returns.
    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
    }
}
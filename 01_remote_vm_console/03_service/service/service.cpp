
#include "stdafx.h"

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <aclapi.h>
#include "messages.h"

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("RemConsSrv")     // a name of the service

SERVICE_STATUS          gSvcStatus;     // —труктура SERVICE_STATUS используетс€ дл€ оповещени€ SCM текущего статуса сервиса.
                                        // https://msdn.microsoft.com/en-us/library/ms685996(VS.85).aspx
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR *);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR *);
VOID SvcReportEvent(LPTSTR);

//
VOID __stdcall SvcStart();
VOID __stdcall SvcRemove();
VOID __stdcall SvcStop();

VOID __stdcall DisplayUsage()
{
    printf("Description:\n");
    printf("\tCommand-line tool that controls a service.\n\n");
    printf("Usage:\n");
    printf("\tservice [command]\n\n");
    printf("\t[command]\n");
    printf("\t  install\n");
    printf("\t  start\n");
    printf("\t  stop\n");
    printf("\t  remove\n");
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
        printf("StartServiceCtrlDispatcher error: %d", GetLastError());
        SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
    }
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR servicePath[MAX_PATH];     // path to service's binary 

    // 1. get a path to our service

    //if (!GetModuleFileName("", szPath, MAX_PATH))  -- compilation error
    //If this parameter is NULL, GetModuleFileName retrieves the path of the executable file of the current process.
    if (!GetModuleFileName(NULL, servicePath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }
    
    // 2. Get a handle to the SCM database. 
    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // 3. Create the service
    schService = CreateService(
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        servicePath,               // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (NULL == schService)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else {
        printf("Service installed successfully\n");
        SvcReportEvent(TEXT("This is not error! Only for testing (Service installed successfully)"));
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
    // Register the handler function for the service
    // регистрируем функцию-обработчик, котора€ будет обрабатывать управл€ющие запросы от SCM, например, запрос на остановку
    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SVCNAME,
        SvcCtrlHandler);    // см. эту функцию ниже

    if (!gSvcStatusHandle)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here
    // другие пол€ тоже можно выставить по-своему, см. ссылку дл€ gSvcStatus выше
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;   // The service runs in its own process
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.
    SvcInit(dwArgc, lpszArgv);
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
// типа функци€ StartService
VOID SvcInit(DWORD dwArgc, LPTSTR *lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.
    //      (An event object is a synchronization object whose state can be explicitly set to signaled by use of the SetEvent function.)
    //      ( https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms682655(v=vs.85).aspx )
    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // the event object is created without a name.

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // Report running status when initialization is complete.
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.

    // TODO: «ƒ≈—№ должен быть главный код сервиса (того, что он делает), или в ÷» Ћ≈?
    
    while (1)
    {
        // ждать, пока указанный объект (объект останова сервиса) не получит сигнал
        //The WaitForSingleObject function checks the current state of the specified object.
        //If the object's state is nonsignaled, the calling thread ENTERS THE WAIT STATE
        //   until the object is signaled or the time-out interval elapses (но у нас INFINITE, так что только если получим сигнал)
        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

//
// Purpose: 
//   Starts the service if possible.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID __stdcall SvcStart()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    // Get a handle to the SCM database. 
    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the service.
    schService = OpenService(
        schSCManager,         // SCM database 
        SVCNAME,              // name of service 
        SERVICE_ALL_ACCESS);  // full access 

    if (NULL == schService)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    // Check the status in case the service is not stopped. 
    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // information level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // size needed if buffer is too small
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return;
    }

    // Check if the service is already running. It would be possible 
    // to stop the service here, but for simplicity this example just returns. 
    if (ssStatus.dwCurrentState != SERVICE_STOPPED && ssStatus.dwCurrentState != SERVICE_STOP_PENDING)
    {
        printf("Cannot start the service because it is already running\n");
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return;
    }

    // Save the tick count and initial checkpoint.
    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    // Wait for the service to stop before attempting to start it.
    while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the status until the service is no longer stop pending. 

        if (!QueryServiceStatusEx(
            schService,                     // handle to service 
            SC_STATUS_PROCESS_INFO,         // information level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // size needed if buffer is too small
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            CloseServiceHandle(schService);
            CloseServiceHandle(schSCManager);
            return;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                printf("Timeout waiting for service to stop\n");
                CloseServiceHandle(schService);
                CloseServiceHandle(schSCManager);
                return;
            }
        }
    }

    // Attempt to start the service.
    if (!StartService(
        schService,  // handle to service 
        0,           // number of arguments 
        NULL))      // no arguments 
    {
        printf("StartService failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service start pending...\n");

    // Check the status until the service is no longer start pending. 
    if (!QueryServiceStatusEx(
        schService,                     // handle to service 
        SC_STATUS_PROCESS_INFO,         // info level
        (LPBYTE)&ssStatus,             // address of structure
        sizeof(SERVICE_STATUS_PROCESS), // size of structure
        &dwBytesNeeded))              // if buffer too small
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return;
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth the wait hint, but no less than 1 second and no 
        // more than 10 seconds. 

        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        // Check the status again. 

        if (!QueryServiceStatusEx(
            schService,             // handle to service 
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&ssStatus,             // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded))              // if buffer too small
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            break;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // Continue to wait and check.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                // No progress made within the wait hint.
                break;
            }
        }
    }

    // Determine whether the service is running.

    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
    {
        printf("Service started successfully.\n");
    }
    else
    {
        printf("Service not started. \n");
        printf("  Current State: %d\n", ssStatus.dwCurrentState);
        printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
        printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
        printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Stops the service.
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID __stdcall SvcStop()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the service.

    schService = OpenService(
        schSCManager,         // SCM database 
        SVCNAME,              // name of service 
        SERVICE_STOP |
        SERVICE_QUERY_STATUS |
        SERVICE_ENUMERATE_DEPENDENTS);

    if (schService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    // Make sure the service is not already stopped.

    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
        goto stop_cleanup;
    }

    if (ssp.dwCurrentState == SERVICE_STOPPED)
    {
        printf("Service is already stopped.\n");
        goto stop_cleanup;
    }

    // If a stop is pending, wait for it.

    while (ssp.dwCurrentState == SERVICE_STOP_PENDING)
    {
        printf("Service stop pending...\n");

        // Do not wait longer than the wait hint. A good interval is 
        // one-tenth of the wait hint but not less than 1 second  
        // and not more than 10 seconds. 

        dwWaitTime = ssp.dwWaitHint / 10;

        if (dwWaitTime < 1000)
            dwWaitTime = 1000;
        else if (dwWaitTime > 10000)
            dwWaitTime = 10000;

        Sleep(dwWaitTime);

        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
        {
            printf("Service stopped successfully.\n");
            goto stop_cleanup;
        }

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            printf("Service stop timed out.\n");
            goto stop_cleanup;
        }
    }

    // If the service is running, dependencies must be stopped first.
    // StopDependentServices();

    // Send a stop code to the service.

    if (!ControlService(
        schService,
        SERVICE_CONTROL_STOP,
        (LPSERVICE_STATUS)&ssp))
    {
        printf("ControlService failed (%d)\n", GetLastError());
        goto stop_cleanup;
    }

    // Wait for the service to stop.

    while (ssp.dwCurrentState != SERVICE_STOPPED)
    {
        Sleep(ssp.dwWaitHint);
        if (!QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded))
        {
            printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
            goto stop_cleanup;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED)
            break;

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            printf("Wait timed out\n");
            goto stop_cleanup;
        }
    }
    printf("Service stopped successfully\n");

stop_cleanup:
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Deletes a service from the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID __stdcall SvcRemove()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS ssStatus;

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the service.

    schService = OpenService(
        schSCManager,       // SCM database 
        SVCNAME,            // name of service 
        DELETE);            // need delete access 

    if (NULL == schService)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    // Delete the service.

    if (!DeleteService(schService))
    {
        printf("DeleteService failed (%d)\n", GetLastError());
    }
    else printf("Service deleted successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}


//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((SERVICE_RUNNING == dwCurrentState) ||
        (SERVICE_STOPPED == dwCurrentState))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}


// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
//   (эта функци€ будет вызыватьс€ SCM)
//   (она будет обрабатывать управл€ющие запросы от SCM, например, запрос на остановку)
//
// Parameters:
//   requestedControlCode - control code (от SCM)
VOID WINAPI SvcCtrlHandler(DWORD requestedControlCode)
{
    // Handle the requested control code. 
    switch (requestedControlCode)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // у парн€ на хабре немного другие здесь действи€, например, есть така€ строка:
        // serviceStatus.dwCurrentState = SERVICE_STOPPED; 

        // сигнализируем сервис остановитьс€!
        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if (NULL != hEventSource)
    {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        //Writes an entry at the end of the specified event log.
        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}
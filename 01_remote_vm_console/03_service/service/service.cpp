
#include "stdafx.h"

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include "messages.h"

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("SvcName")     // a name of the service (TODO: надо будет назвать)

SERVICE_STATUS          gSvcStatus;     // Структура SERVICE_STATUS используется для оповещения SCM текущего статуса сервиса.
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
VOID SvcRemove();

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
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM (Service Control Manager)
    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return;
    }

    // Можно: Add any additional services for the process to this table.
    // SERVICE_TABLE_ENTRY это структура, которая описывает точку входа для сервис менеджера
    // типа для сервиса с названием SVCNAME (см. define выше) точкой входа будет SvcMain (см. ниже)
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.
    // Функция StartServiceCtrlDispatcher собственно связывает наш сервис с SCM (Service Control Manager)
    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
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

    // 1. Пытаемся узнать путь к сервису.
    // здесь используем функцию GetModuleFileName
    // у парня на хабре: servicePath = LPTSTR(argv[0]);

    //if (!GetModuleFileName("", szPath, MAX_PATH))  -- но на "" ругается компилятор
    //If this parameter is NULL, GetModuleFileName retrieves the path of the executable file of the current process.
    if (!GetModuleFileName(NULL, servicePath, MAX_PATH))         // https://msdn.microsoft.com/en-us/library/windows/desktop/ms683197(v=vs.85).aspx
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

    if (schService == NULL)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n");

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
    // регистрируем функцию-обработчик, которая будет обрабатывать управляющие запросы от SCM, например, запрос на остановку
    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SVCNAME,
        SvcCtrlHandler);    // см. эту функцию ниже

    if (!gSvcStatusHandle)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }

    // These SERVICE_STATUS members remain as set here
    // другие поля тоже можно выставить по-своему, см. ссылку для gSvcStatus выше
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
// типа функция StartService
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

    // TODO: ЗДЕСЬ должен быть главный код сервиса (того, что он делает), или в ЦИКЛЕ?

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
//   Remove the service
//
// Parameters:
//   None
// 
// Return value:
//   None
// (см. у парня на хабре)
VOID SvcRemove()
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        printf("Error: Can't open Service Control Manager");
        return;
    }

    // opens an existing service
    SC_HANDLE hService = OpenService(hSCManager, SVCNAME, SERVICE_STOP | DELETE);
    if (!hService) {
        printf("Error: Can't remove service");
        CloseServiceHandle(hSCManager);
        return;
    }

    // TODO: error handling
    DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    printf("Success remove service!");
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

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}


// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
//   (эта функция будет вызываться SCM)
//   (она будет обрабатывать управляющие запросы от SCM, например, запрос на остановку)
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

        // у парня на хабре немного другие здесь действия, например, есть такая строка:
        // serviceStatus.dwCurrentState = SERVICE_STOPPED; 

        // сигнализируем сервис остановиться!
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
#include "stdafx.h"
#include "rdrctd_in_out_childprocess.h"

//global variables should have been structures

//those are ends of pipes (we use 2 pipes)
HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;

static void CreateChildProcess(void);		// create Child process -- CMD.EXE (that will execute our commands written in the pipe)
static void ErrorExit(PTSTR);		// error handling

void init_child_process()
{
    SECURITY_ATTRIBUTES saAttr;

    // Set the bInheritHandle flag so pipe handles are inherited. 

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        ErrorExit(TEXT("StdoutRd CreatePipe"));

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdout SetHandleInformation"));

    // Create a pipe for the child process's STDIN. 
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
        ErrorExit(TEXT("Stdin CreatePipe"));

    // Ensure the write handle to the pipe for STDIN is not inherited. 
    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
        ErrorExit(TEXT("Stdin SetHandleInformation"));

    // Create the child process. 
    CreateChildProcess();

    // Write to the pipe that is the standard input for a child process. 
    // Data is written to the pipe's buffers, so it is not necessary to wait
    // until the child process is running before writing data.


    // Create the THREAD for WRITING to the pipe (thread in PARENT process)
    /*
    // TODO: Error Handling
    CreateThread(
    NULL,                   // default security attributes
    0,                      // use default stack size
    WriteToPipe,       // thread function name
    NULL,          // argument to thread function
    0,                      // use default creation flags
    NULL);   // returns the thread identifier
    */

}

// Create a child process that uses the previously created pipes for STDIN and STDOUT.
void CreateChildProcess()
{
    TCHAR cmdline[] = TEXT("cmd.exe");	// our child process is cmd.exe
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    // Set up members of the PROCESS_INFORMATION structure. 

    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process. 
    bSuccess = CreateProcess(NULL,	// No module name (use command line)
        cmdline,	   // call cmd.exe
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // no creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

                       // If an error occurs, exit the application. 
    if (!bSuccess)
        ErrorExit(TEXT("CreateProcess"));
    else
    {
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process, for example. 

        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);
    }
}


int WriteToPipeFromSocket(SOCKET &ClientSocket)
{
    DWORD dwRead, dwWritten;
    CHAR chBuf[BUFSIZE];
    BOOL bSuccess = FALSE;

    dwRead = recv(ClientSocket, chBuf, BUFSIZE, 0);
    if (dwRead > 0) {
        if (dwRead - 2 >= BUFSIZE)
            ErrorExit(TEXT("in writeToPipe function error! (message's lenght from client > bufsize"));
        chBuf[dwRead] = '\r';
        chBuf[dwRead + 1] = '\n';

        bSuccess = WriteFile(g_hChildStd_IN_Wr, chBuf, dwRead + 2, &dwWritten, NULL);
        if (!bSuccess)
            ErrorExit(TEXT("cannot write to the pipe"));
        if (dwRead + 2 != dwWritten)
            ErrorExit(TEXT("In writeToPipe function error! (number of written bytes != number of read bytes)"));
    }
    else if (dwRead == 0)
        printf("Connection closing...\n");
    else {
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return -1;
    }


    // TODO: create separate function to close handle!
    // Close the pipe handle so the child process stops reading. 
    //if (!CloseHandle(g_hChildStd_IN_Wr))
    //	ErrorExit(TEXT("StdInWr CloseHandle"));
    return 0;
}


int WriteToSocketFromPipe(SOCKET &socket)
{
    DWORD dwRead, dwWritten;
    CHAR buf[BUFSIZE];
    BOOL bSuccess = FALSE;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    int iSendResult;

    // 
    for (;;)
    {
        // 1. read from pipe an info that was written by Child process (CMD.EXE)
        // and put this info into chBuf
        bSuccess = ReadFile(g_hChildStd_OUT_Rd, buf, BUFSIZE, &dwRead, NULL);
        if (!bSuccess || dwRead == 0)
            return -1;

        // 2.
        //bSuccess = WriteFile((HANDLE)socket, chBuf, dwRead, &dwWritten, NULL);
        iSendResult = send(socket, buf, dwRead, 0);
        if (iSendResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(socket);
            WSACleanup();
            return -1;
        }

        break;
    }
    return 0;
}

// Format a readable error message, display a message box, 
// and exit from the application.
void ErrorExit(PTSTR lpszFunction)
{
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0, NULL);

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40)*sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
}
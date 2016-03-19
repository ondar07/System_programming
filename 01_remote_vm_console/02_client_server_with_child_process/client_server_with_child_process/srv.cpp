#undef UNICODE

#include "stdafx.h";
#include "redirected_input_ouput.h";

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

int __cdecl srv(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;	// this SOCKET is for listening (waiting connections from client)
	
	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); // the Windows Sockets specification is version 2.2
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

    for (;;) {
        SOCKET ClientSocket = INVALID_SOCKET;	// actual socket (this value will be returned by accept function, see link below)
        
        // Accept a client socket

        // The accept function extracts the first connection on the queue of pending connections on socket s.
        // It then creates and returns a handle to the NEW SOCKET.
        // The newly created socket is the socket that will handle the ACTUAL CONNECTION
        // The accept function can BLOCK the CALLER (this process) until a connection is present if no pending connections are present on the queue
        // more about accept function : https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms737526(v=vs.85).aspx
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (INVALID_SOCKET == ClientSocket) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            continue;
        }

        // connection with a client has been set up
        // so create CMD.EXE with redirected input/ouput
        PROCESS_INFORMATION childProcInfo;
        childProcInfo = InitChildProcess();

        // Receive until the peer shuts down the connection
        for (;;) {
            // TODO: in another thread (maybe)
            if (WriteToPipeFromSocket(ClientSocket) < 0)
                break;
            if (WriteToSocketFromPipe(ClientSocket) < 0)
                break;
        }

        // shutdown the connection since we're done
        iResult = shutdown(ClientSocket, SD_SEND);
        if (SOCKET_ERROR == iResult) {
            printf("shutdown failed with error: %d\n", WSAGetLastError());
            closesocket(ClientSocket);
        }

        // exit the child process (CMD.exe) after this connection is closed
        TerminateChildProcess(childProcInfo);
    }

	// cleanup
    closesocket(ListenSocket);
	WSACleanup();

	return 0;
}
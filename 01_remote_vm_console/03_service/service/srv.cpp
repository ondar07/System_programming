#undef UNICODE

#include "stdafx.h"
#include "rdrctd_in_out_childprocess.h"

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
    SOCKET ClientSocket = INVALID_SOCKET;	// actual socket (this value will be returned by accept function, see link below)

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
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

    // Accept a client socket

    // The accept function extracts the first connection on the queue of pending connections on socket s.
    // It then creates and returns a handle to the NEW SOCKET.
    // The newly created socket is the socket that will handle the ACTUAL CONNECTION
    // The accept function can BLOCK the CALLER (this process) until a connection is present if no pending connections are present on the queue
    // more about accept function : https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms737526(v=vs.85).aspx
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    // TODO: does it need?
    // No longer need server socket
    closesocket(ListenSocket);

    // get signal from client, so should create CMD.EXE with redirected input/ouput
    init_child_process();

    // Receive until the peer shuts down the connection
    do {
        // TODO: in another thread (maybe)
        iResult = WriteToPipeFromSocket(ClientSocket);
        if (iResult < 0)
            printf("WriteToPipeFromSocket returns -1");
        iResult = WriteToSocketFromPipe(ClientSocket);
        if (iResult < 0)
            printf("WriteToSocketFromPipe returns -1");

        /*
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
        printf_s("Server get a message: %s\n", recvbuf);
        WriteToPipe(ClientSocket);
        // TODO.
        // in recvbuf should be our command name => here we should writeToPipe
        //     => in child process (CMD.EXE) perform this command
        // after that readFromPipe by parent's process (client process is the parent process)

        // TODO: in recvbuf (maybe, create another buffer, for example sendbuf) will be info being got from readFromPipe (see previous TODO)
        iSendResult = send(ClientSocket, recvbuf, iResult, 0);
        if (iSendResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
        }
        printf("Bytes sent: %d\n", iSendResult);

        }

        else if (iResult == 0)
        printf("Connection closing...\n");
        else {
        printf("recv failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
        }
        */

    } while (iResult == 0);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}
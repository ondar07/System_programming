
#include "stdafx.h"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define RECV_BUFLEN 4096
#define SENDBUF_LEN 256
#define DEFAULT_PORT "27015"
#define IP_ADDRESS_OF_MACHINE "127.0.0.1"		// localhost ip ( ! should be VM IP)

int __cdecl cli(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	//char *sendbuf = "this is a test\n";
	char sendbuf[SENDBUF_LEN];
	char recvbuf[RECV_BUFLEN];
	int iResult;

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	// TODO: IP address of VM (in the STAGE IV)
	iResult = getaddrinfo(IP_ADDRESS_OF_MACHINE, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (INVALID_SOCKET == ConnectSocket) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (SOCKET_ERROR == iResult) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (INVALID_SOCKET == ConnectSocket) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}

	///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	// send and receive until the peer closes the connection
	do {
		// TODO: the following doesn't work for input including spaces (for example, "help dir")
		scanf_s("%s", sendbuf, SENDBUF_LEN);

		// Send our input to server (Virtual Machine)
		iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (SOCKET_ERROR == iResult) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}

		printf("Bytes Sent: %ld\n", iResult);

        iResult = recv(ConnectSocket, recvbuf, RECV_BUFLEN, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

        printf("CLIENT HAS GOT MESSAGE FROM SERVER:\n");
        // TODO: print output normally
        for (long i = 0; i < iResult; i++)		// iResult == number of read bytes
            printf("%c", recvbuf[i]);
	} while (iResult > 0);

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (SOCKET_ERROR == iResult) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();

	return 0;
}
#include "Header.h"
#include "Other.h"
#include "Receiver.h"
#include "Sender.h"

// dopln handling error stateov, vymysliet koniec fgets
std::mutex socketStateMutex;
SocketState socketState;

std::mutex errorCodeMutex;
ErrorCodes errorCode;

int main()
{
	const char *targetAdress = "192.168.0.220";//  "127.0.0.1";
	const char *port = "1050";
	struct addrinfo hints, *res;

	// Set the flags for getaddrinfo
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;

	// Initialize the socket
	WSAData wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	// Fill the res addrinfo struct
	int status = getaddrinfo(targetAdress, port, &hints, &res);
	if (status != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(status));
		freeaddrinfo(res);
		return 1;
	}

	// Create a socket
	SOCKET main_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (main_socket == INVALID_SOCKET)
	{
		printf("Socket creation failed!\n");
		freeaddrinfo(res);
		return 1;
	}

	// Connect to the socket
	int connect_code = connect(main_socket, res->ai_addr, res->ai_addrlen);
	if (connect_code == SOCKET_ERROR)
	{
		printf("Socket connection failed!\n");
		freeaddrinfo(res);
		return 1;
	}

	// Create thread responsible for receiving messaages
	std::thread receiverThread(&Receiver::receiver, Receiver(main_socket));

	// Create thread responsible for sending messaages
	std::thread senderThread(&Sender::sender, Sender(main_socket));

	receiverThread.join();
	senderThread.join();

	WSACleanup();

	return 0;
}
int32_t closeSocketWrapper(SOCKET socket, int8_t flag)
{
	int8_t symbol;
	constexpr int32_t size_buffer = 512;
	char buffer[size_buffer];

	std::unique_lock<std::mutex> socketStateLock(socketStateMutex);

	if (socketState == SocketState::Closed) {
		printf("INFO: closeSocket in thread (Id: %d) - socket already closed\n",
			GetCurrentThreadId());

		return 0;
	}

	switch (flag)
	{
	case 0:
		symbol = shutdown(socket, SD_SEND);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - shutdown failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			closesocket(socket);

			socketState = SocketState::Closed;

			return 1;
		}

	case 1:
		symbol = recv(socket, buffer, size_buffer, NULL);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - recvall failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			closesocket(socket);

			socketState = SocketState::Closed;

			return 1;
		}
		else if (symbol != 0)
		{
			printf(buffer);
		}

	case 2:
		printf("CLOSING\n");
		symbol = closesocket(socket);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - close failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			socketState = SocketState::Closed;

			return 1;
		}

		break;

	default:
		printf("ERROR: closeSocketWrap in thread (Id: %d) - incorrect flag for socket closing received\n",
			GetCurrentThreadId());

		return 1;
	}

	return 0;
}

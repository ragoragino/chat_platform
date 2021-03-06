#include "Header.h"
#include "Other.h"
#include "Receiver.h"
#include "Sender.h"

namespace Chat
{
	std::mutex errorCodeMutex;
	ErrorCodes errorCode = ErrorCodes::Correct;
}

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
	std::thread receiverThread(&Chat::Receiver::receiver, Chat::Receiver(main_socket));

	// Create thread responsible for sending messaages
	std::thread senderThread(&Chat::Sender::sender, Chat::Sender(main_socket));

	receiverThread.join();
	senderThread.join();

	WSACleanup();

	return 0;
}
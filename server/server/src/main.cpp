#include "Header.h"
#include "Other.h"
#include "ServerHandler.h"
#include "ClientHandler.h"

namespace Chat
{
	ServerHandler serverHandler;
	std::mutex serverMutex;
}

int main()
{
	addrinfo hints, *res;
	char ipstr[INET6_ADDRSTRLEN];
	const char *port = "1050";
	const char *serverIP = "192.168.0.220"; // "127.0.0.1";

	// Set the flags for getaddrinfo
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;

	// Initialize the socket library on Windows
	WSAData wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	// Fill the res addrinfo struct
	int status = getaddrinfo(serverIP, port, &hints, &res);
	if (status != 0)
	{
		printf("getaddrinfo: %s\n", gai_strerror(status));
		freeaddrinfo(res);
		return 1;
	}

	// Create a socket
	SOCKET mother_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (mother_socket == INVALID_SOCKET)
	{
		printf("Socket creation failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(res);
		return 1;
	}

	// Bind a socket to the port
	int bind_code = bind(mother_socket, res->ai_addr, res->ai_addrlen);
	if (bind_code == SOCKET_ERROR)
	{
		printf("Socket binding failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(res);
		return 1;
	}

	// Listen to clients on the socket
	int listen_code = listen(mother_socket, _MAXLOG);
	if (listen_code == SOCKET_ERROR)
	{
		printf("Socket listening failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(res);
		return 1;
	}

	// Initialize a thread that will be responsible for sending the messages
	std::thread threadSocketSender(&Chat::ServerHandler::sender, std::ref(Chat::serverHandler));

	// Initialize vector holding threads that will be responsible for receiving messages
	std::vector<std::thread> threadSocketReceivers;

	// Accept connections
	sockaddr guest_address;
	int guest_address_length = sizeof sockaddr;
	while (SOCKET child_socket = accept(mother_socket, &guest_address, &guest_address_length))
	{
		// Handle errors in the socket acceptance
		if (child_socket == INVALID_SOCKET)
		{
			printf("ERROR: Main Thread - setting child socket failed with error: %d\n",
				WSAGetLastError());
			continue;
		}

		// Print the family and IP of the child connection
		inet_ntop(guest_address.sa_family, guest_address.sa_data, ipstr, sizeof ipstr);
		const char *guess_family = guest_address.sa_family == AF_INET ? "IPv4" : "IPv6";
		printf("GUEST: %s: %s\n", guess_family, ipstr);
		guest_address_length = sizeof sockaddr;
		
		// Handle the accepted socket
		std::unique_lock<std::mutex> serverLock(Chat::serverMutex);
		Chat::serverHandler.addSocket(ADDR_SOCKET(child_socket, ipstr));
		serverLock.unlock();

		// Create a thread for receiving messages from this socket
		threadSocketReceivers.emplace_back(&Chat::ClientHandler::receiver,
			Chat::ClientHandler(ADDR_SOCKET(child_socket, ipstr)));
	}

	closesocket(mother_socket);
	WSACleanup();

	return 0;
}


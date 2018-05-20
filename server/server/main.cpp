#include "Header.h"
#include "Other.h"

# define _LENGTH_SIZE 2 // Size indicating the length of the data buffer
# define _MAX_MESSAGE_SIZE POW<2, _LENGTH_SIZE * 8>::value + 1
# define _MAXLOG 20
# define _ID_SIZE 2 // Size indicating the length of the ID buffer

static CONDITION_VARIABLE queueNotEmpty;
static CRITICAL_SECTION queueLock;
static std::queue<Message> messages;

static CRITICAL_SECTION socketsLock;
static std::vector<ADDR_SOCKET> sockets;

static CRITICAL_SECTION errorCodeLock;
static int8_t errorCode = 0x00;

inline int8_t sendall(SOCKET socket, char *message, int32_t message_size, int32_t flags)
{
	int8_t s_send;

	while (true)
	{
		s_send = send(socket, message, message_size, flags);

		if (s_send == message_size)
		{
			// Transfer was successful
			return 1;
		}
		else if (s_send == SOCKET_ERROR)
		{
			// Transfer ended in an error code
			return -1;
		}

		// Handle incomplete message transfer
		message += s_send;
		message_size -= s_send;
	}
}

inline int8_t recvall(SOCKET socket, char *message, int32_t message_size, int32_t flags)
{
	int8_t s_recv;

	while (true)
	{
		// Receive the packet
		s_recv = recv(socket, message, message_size, flags);

		// Handle transfer states
		if (s_recv == message_size)
		{
			// Transfer was successful
			return 1;
		}
		else if (s_recv == 0)
		{
			// Socket was closed
			return 0;
		}
		else if (s_recv == SOCKET_ERROR)
		{
			// Transfer was unsuccesful
			return -1;
		}

		// Handle incomplete message transfer
		message += s_recv;
		message_size -= s_recv;
	}
}

unsigned closeSocketWrap(SOCKET socket, int32_t flag)
{		
	int8_t symbol;
	constexpr int32_t size_buffer = 512;
	char buffer[size_buffer];

	switch (flag)
	{
	case 0:
		symbol = shutdown(socket, SD_SEND);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - shutdown failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			closesocket(socket);
			return 1;
		}

	case 1:
		symbol = recvall(socket, buffer, size_buffer, NULL);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - recvall failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			closesocket(socket);
			return 1;
		}
		printf(buffer);

	case 2:
		symbol = closesocket(socket);
		if (symbol == SOCKET_ERROR)
		{
			printf("ERROR: closeSocketWrap in thread (Id: %d) - close failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			return 1;
		}

	default:
		return 0;
	}	

	// The program should not access this part of the code
	assert(false);

	return 1;
}

unsigned __stdcall Sender(void *data)
{
	// TODO : Handle returning unallocated buffer
	Message message;
	char *buffer = (char*)malloc(_MAX_MESSAGE_SIZE + _LENGTH_SIZE + INET6_ADDRSTRLEN);
	if (buffer == NULL)
	{
		printf("ERROR: Sender thread (ID: %d) unable to allocate memory!\n",
			GetCurrentThreadId());

		EnterCriticalSection(&errorCodeLock);

		errorCode = 0x02;

		LeaveCriticalSection(&errorCodeLock);

		return 1;
	}

	while (true)
	{
		EnterCriticalSection(&queueLock);

		SleepConditionVariableCS(&queueNotEmpty, &queueLock, INFINITE);

		while (!messages.empty())
		{
			// Get the first message
			message = messages.front();
			messages.pop();

			// Leaving critical section so that the producer does not wait
			LeaveCriticalSection(&queueLock);

			// Copy length of the message, IP of the source and the message itself to the buffer
			uint16_t str_len = (uint16_t)(message.message_length() + 1);
			memcpy(buffer, &str_len, _LENGTH_SIZE);
			memcpy(buffer + _LENGTH_SIZE, message.get_ip(), INET6_ADDRSTRLEN);
			memcpy(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN, message.get_message(), str_len);

			// Write the message and the src IP address to the messages vector
			EnterCriticalSection(&socketsLock);

			// Send all of the message to all the servers
			int8_t symbol;
			for (const ADDR_SOCKET& addr_socket : sockets)
			{
				if (strcmp(addr_socket.clientip, message.get_ip()) == 0)
				{
					continue;
				}

				symbol = sendall(addr_socket.socket, buffer, 
					str_len + _LENGTH_SIZE + INET6_ADDRSTRLEN, NULL);

				// Check for errors while sending the message
				if (symbol == -1)
				{
					printf("ERROR: Sender thread (Id: %d) send failed with error: %d\n", 
						GetCurrentThreadId(), WSAGetLastError());
				}
				else if (symbol == 0)
				{
					printf("INFO: Sender thread (Id: %d) connection failed!\n",
						GetCurrentThreadId());
				}
			}

			LeaveCriticalSection(&socketsLock);

			EnterCriticalSection(&queueLock);
		}

		LeaveCriticalSection(&queueLock);
	}
}

unsigned __stdcall Receiver(void *data)
{
	int8_t symbol = 0;
	ADDR_SOCKET *socket_addr = (ADDR_SOCKET*)data;
	SOCKET socket = socket_addr->socket;
	char src_addres[INET6_ADDRSTRLEN];
	char buffer_size[_LENGTH_SIZE];
	char buffer_id[_ID_SIZE];
	char *buffer = (char*)malloc(_MAX_MESSAGE_SIZE + 1);
	Dictionary<uint16_t, 5> ids;
	if (buffer == NULL)
	{
		printf("ERROR: Receiver thread (ID: %d) unable to allocate memory!\n",
			GetCurrentThreadId());

		EnterCriticalSection(&errorCodeLock);

		errorCode = 0x02;

		LeaveCriticalSection(&errorCodeLock);

		closeSocketWrap(socket, 0);

		return 1;
	}

	while (true)
	{
		// Receive the size of the message
		symbol = recvall(socket, buffer_size, _LENGTH_SIZE, NULL);
		if (symbol != 1) {
			break; 
		}

		uint16_t *size_of_buffer16 = (uint16_t*)&buffer_size[0];
		int32_t size_of_buffer32 = (int32_t)*size_of_buffer16;

		// Receive the address of the source
		symbol = recvall(socket, src_addres, INET6_ADDRSTRLEN, NULL);
		if (symbol != 1) {
			break;
		}

		// Receive the size of the message
		symbol = recvall(socket, buffer_id, _ID_SIZE, NULL);
		if (symbol != 1) {
			break;
		}

		// Checking for message duplication
		uint16_t *id = (uint16_t*)&buffer_id[0];
		if (ids.find(*id) == 1) {
			break;
		}
		else {
			ids.add(*id);
		}

		// Receive and print the received message
		symbol = recvall(socket, buffer, size_of_buffer32, NULL);
		if (symbol != 1) { 
			break; 
		}

		// Safety null ending
		buffer[size_of_buffer32] = '\0';

		// Check whether the received message is not empty 
		// signalizes abrupt termination of the client
		if (size_of_buffer32 == 1) {
			symbol = 0;
			break;
		}
	
		printf(socket_addr->clientip);
		printf(": ");
		printf(buffer);

		// Write the message and the src IP address to the messages vector
		EnterCriticalSection(&queueLock);
		
		messages.emplace(buffer, socket_addr->clientip);

		LeaveCriticalSection(&queueLock);

		WakeConditionVariable(&queueNotEmpty);
	}

	free(buffer);
	
	int8_t return_symbol = 0;
	if (symbol == -1)
	{
		printf("ERROR: Receiver thread (Id: %d) send failed with error: %d\n",
			GetCurrentThreadId(), WSAGetLastError());

		closeSocketWrap(socket, 2);

		return_symbol = 1;
	}
	else if (symbol == 0)
	{
		printf("INFO: Receiver thread (Id: %d) connection failed!\n",
			GetCurrentThreadId());

		int8_t flag = closeSocketWrap(socket, 2);

		if (flag != 0) {
			return_symbol = flag;
		}

		return_symbol = 0;
	}

	// Erase this socket from the sockets vector
	EnterCriticalSection(&socketsLock);

	// Socket is exiting and therefore erase it from the sockets vector
	printf("INFO: Receiver thread (ID: %d) exiting!\n",
		GetCurrentThreadId());
	std::vector<ADDR_SOCKET>::iterator it = std::find_if(sockets.begin(), sockets.end(),
		[&socket](const ADDR_SOCKET &a) { return socket == a.socket; });
	sockets.erase(it);
	
	// Check the case of inability to release the mutex
	LeaveCriticalSection(&socketsLock);

	return return_symbol;
}

unsigned __stdcall ClientSession(void *data)
{
	unsigned threadIDReceiver;
	HANDLE receiverHandle = (HANDLE)_beginthreadex(NULL, 0, &Receiver, 
		(void*)data, 0, &threadIDReceiver);

	WaitForSingleObject(receiverHandle, INFINITE);

	free(data);

	return 0;
}

int main()
{
	addrinfo hints, *res;
	char ipstr[INET6_ADDRSTRLEN];
	const char *port = "1050";
	const char *server = "192.168.0.220"; // "127.0.0.1";

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
	int status = getaddrinfo(server, port, &hints, &res);
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

	// Initialize the condition variable for message queue
	InitializeConditionVariable(&queueNotEmpty);

	// Initialize the critical section for message, sockets and error queue
	InitializeCriticalSection(&queueLock);
	InitializeCriticalSection(&socketsLock);
	InitializeCriticalSection(&errorCodeLock);

	unsigned threadIDSender;
	HANDLE senderHandle = (HANDLE)_beginthreadex(NULL, 0, &Sender, NULL, 0, &threadIDSender);

	// Accept connections
	sockaddr guest_address;
	int guest_address_length = sizeof sockaddr;
	while (SOCKET child_socket = accept(mother_socket, &guest_address, &guest_address_length))
	{
		// Check the global errors state
		EnterCriticalSection(&errorCodeLock);

		if(errorCode != 0x00)
		{
			printf("ERROR: Main Thread - errorCode set to %d. Exiting application!", errorCode);
			exit(1);
		}

		LeaveCriticalSection(&errorCodeLock);

		// Handle errors in the socket acceptance
		if (child_socket == INVALID_SOCKET)
		{
			printf("ERROR: Main Thread - setting child socket failed with error: %d\n", 
				WSAGetLastError());
			continue;
		}

		// Handle the accepted socket
		EnterCriticalSection(&socketsLock);

		// Print the family and IP of the child connection
		inet_ntop(guest_address.sa_family, guest_address.sa_data, ipstr, sizeof ipstr);
		const char *guess_family = guest_address.sa_family == AF_INET ? "IPv4" : "IPv6";
		printf("GUEST: %s: %s\n", guess_family, ipstr);
		guest_address_length = sizeof sockaddr;
		
		ADDR_SOCKET *sock_address = (ADDR_SOCKET*)malloc(sizeof ADDR_SOCKET);
		memcpy(sock_address->clientip, ipstr, INET6_ADDRSTRLEN);
		sock_address->socket = child_socket;

		// Add new socket to the vector of sockets
		sockets.push_back(*sock_address);
		
		// Create a new thread for the accepted client (also pass in the accepted client socket)
		unsigned threadID;
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &ClientSession, 
			(void*)sock_address, 0, &threadID);

		LeaveCriticalSection(&socketsLock);
	}

	closesocket(mother_socket);
	WSACleanup();

	return 0;
}

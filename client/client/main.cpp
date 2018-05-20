#include "Header.h"
#include "Other.h"

# define _LENGTH_SIZE 2 // Size indicating the length of the information buffer in bytes -> uint16_t
# define _MAX_MESSAGE_SIZE POW<2, _LENGTH_SIZE * 8>::value + 1
# define _MAX_SHUTDOWN_MESSAGE_SIZE 512
# define _ID_LENGTH 2 

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
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(socket);
			return 1;
		}

	case 1:
		symbol = recvall(socket, buffer, size_buffer, NULL);
		if (symbol == SOCKET_ERROR)
		{
			printf("recvall failed with error: %d\n", WSAGetLastError());
			closesocket(socket);
			return 1;
		}
		printf(buffer);

	case 2:
		symbol = closesocket(socket);
		if (symbol == SOCKET_ERROR)
		{
			printf("close failed with error: %d\n", WSAGetLastError());
			return 1;
		}

	case 3:
		return 0;
	}

	// The program should not access this part of the code
	assert(false);

	return 1;
}

unsigned __stdcall Sender(void *data)
{
	uint16_t id = 0;
	int8_t symbol;
	SOCKET socket = (SOCKET)data;
	char *buffer = (char*)malloc(_MAX_MESSAGE_SIZE + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH);
	if (buffer == NULL)
	{
		printf("Buffer allocation failed!\n");

		return closeSocketWrap(socket, 0);
	}

	while (true)
	{
		// For cases of abrupt end of the program, there needs to be signal of the termination
		memset(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH, '\0', 1);

		// Get the message
		fgets(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH, _MAX_MESSAGE_SIZE, stdin);
		
		// Handle terminations by typed keywords
		if (strcmp(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH, "EXIT\n") == 0)
		{
			free(buffer);

			return closeSocketWrap(socket, 0);
		}

		// Set the length of the buffer 
		uint16_t str_len = (uint16_t)(strlen(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH) + 1);
		memcpy(buffer, &str_len, _LENGTH_SIZE);
		memset(buffer + _LENGTH_SIZE, 0, INET6_ADDRSTRLEN);
		memcpy(buffer + _LENGTH_SIZE + INET6_ADDRSTRLEN, (void*)&id, _ID_LENGTH);
		id++;

		// Send all of the message with address size unspecified
		symbol = sendall(socket, buffer, str_len + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_LENGTH, NULL);
		if (symbol == -1)
		{
			printf("Send failed with error: %d\n", WSAGetLastError());

			free(buffer);

			return closeSocketWrap(socket, 2);
		}
		else if (symbol == 0)
		{
			printf("Connection was closed!\n");

			free(buffer);

			return closeSocketWrap(socket, 2);
		}
	}

	return 0;
}

unsigned __stdcall Receiver(void *data)
{
	int8_t symbol;
	SOCKET socket = (SOCKET)data;
	char buffer_size[_LENGTH_SIZE]; // buffer used for saving the size of the message
	char src_address[INET6_ADDRSTRLEN]; // buffer used for saving the address of the source
	char *buffer = (char*)malloc(_MAX_MESSAGE_SIZE); // buffer used for saving the message
	if (buffer == NULL)
	{
		printf("Buffer allocation failed!\n");

		return closeSocketWrap(socket, 0);
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
		
		// Receive the source address
		symbol = recvall(socket, src_address, INET6_ADDRSTRLEN, NULL);
		if (symbol != 1) {
			break;
		}

		// Receive and print the received message
		symbol = recvall(socket, buffer, size_of_buffer32, NULL);
		if (symbol != 1) {
			break;
		}

		printf(src_address);
		printf(": ");
		printf(buffer);
	}

	if (symbol == 0)
	{
		// Socket is 0 when already closed by the other side
		// Close the socket
		free(buffer);
		return 0;
	}
	else if (symbol == -1)
	{
		printf("Receive failed with error: %d\n", WSAGetLastError());
		closesocket(socket);
		WSACleanup();
		free(buffer);
		return 1;
	}
	
	return 0;
}

unsigned __stdcall ClientSession(void *data)
{
	HANDLE handles[2];

	unsigned threadIDSender;
	handles[0] = (HANDLE)_beginthreadex(NULL, 0, &Sender, data, 0, &threadIDSender);

	unsigned threadIDReceiver;
	handles[1] = (HANDLE)_beginthreadex(NULL, 0, &Receiver, data, 0, &threadIDReceiver);

	WaitForMultipleObjects(2, handles, TRUE, INFINITE);

	return 0;
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

	// Create a new thread for the server
	unsigned threadID;
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &ClientSession, (void*)main_socket, 0, &threadID);

	// Let the main thread wait for the hThread indefinitely
	WaitForSingleObject(hThread, INFINITE);
	
	return 0;
}

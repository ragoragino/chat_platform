#include "Header.h"
#include "Other.h"
#include "ClientHandler.h"
#include "ServerHandler.h"

namespace Chat
{
	extern ServerHandler serverHandler;
	extern std::mutex serverMutex;

	inline int32_t ClientHandler::recvall(char *message,
		int32_t message_size,
		int32_t flags)
	{
		int32_t s_recv;
		int32_t counter = 0;

		while (counter++ < _MAX_TRY)
		{
			s_recv = recv(addr_socket.socket, message, message_size, NULL);

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
				return SOCKET_ERROR;
			}

			// Handle incomplete message transfer
			message += s_recv;
			message_size -= s_recv;
		}

		printf("INFO: ClientHandler thread (ID: %d) unable to send whole message!\n",
			GetCurrentThreadId());

		return message_size;
	}

	int32_t ClientHandler::receiver()
	{
		int8_t symbol = 0;
		char src_addres[INET6_ADDRSTRLEN];
		char buffer_size[_LENGTH_SIZE];
		char buffer_id[_ID_SIZE];
		std::unique_ptr<char[]> buffer(new char[_MAX_MESSAGE_SIZE + 1]);
		Dictionary<uint16_t, 5> ids;
		
		// Check for success of the allocation
		if (!buffer)
		{
			printf("ERROR: Receiver thread (ID: %d) unable to allocate memory!\n",
				GetCurrentThreadId());
			
			// Remove the socket from the serverHandler
			std::unique_lock<std::mutex> serverLock(serverMutex);
			serverHandler.removeSocket(addr_socket);
			serverLock.unlock();

			// Close the socket
			this->closeSocket(0);

			return 1;
		}

		while (true)
		{
			// Receive the size of the message
			symbol = this->recvall(buffer_size, _LENGTH_SIZE, NULL);
			if (symbol != 1) {
				break;
			}

			uint16_t *size_of_buffer16 = (uint16_t*)&buffer_size[0];
			int32_t size_of_buffer32 = (int32_t)*size_of_buffer16;

			// Receive the address of the source
			symbol = this->recvall(src_addres, INET6_ADDRSTRLEN, NULL);
			if (symbol != 1) {
				break;
			}

			// Receive the size of the message
			symbol = this->recvall(buffer_id, _ID_SIZE, NULL);
			if (symbol != 1) {
				break;
			}

			// Checking for message duplication
			uint16_t *id = (uint16_t*)&buffer_id[0];
			if (ids.find(*id) == 1) {
				continue;
			}
			else {
				ids.add(*id);
			}

			// Receive and print the received message
			symbol = this->recvall(buffer.get(), size_of_buffer32, NULL);
			if (symbol != 1) {
				break;
			}

			// Safety null ending
			buffer.get()[size_of_buffer32] = '\0';

			// Check whether the received message is not empty 
			// signalizes abrupt termination of the client
			if (size_of_buffer32 == 1)
			{
				symbol = 0;
				break;
			}

			printf(addr_socket.clientip);
			printf(": ");
			printf(buffer.get());

			// Send the message to the sender
			std::unique_lock<std::mutex> serverLock(serverMutex);
			serverHandler.sendMessage(std::make_unique<Message>(buffer.get(), addr_socket.clientip));
			serverLock.unlock();
		}

		int8_t return_symbol = 0;
		if (symbol == -1)
		{
			printf("ERROR: Receiver thread (Id: %d) send failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			this->closeSocket(2);

			return_symbol = 1;
		}
		else if (symbol == 0)
		{
			printf("INFO: Receiver thread (Id: %d) connection failed!\n",
				GetCurrentThreadId());

			int8_t flag = this->closeSocket(2);

			if (flag != 0) {
				return_symbol = flag;
			}

			return_symbol = 0;
		}

		printf("INFO: Receiver thread (ID: %d) exiting!\n",
			GetCurrentThreadId());

		std::unique_lock<std::mutex> serverLock(serverMutex);
		serverHandler.removeSocket(addr_socket);
		serverLock.unlock();

		return return_symbol;
	}

	int32_t ClientHandler::closeSocket(int8_t flag)
	{
		int8_t symbol;
		constexpr int32_t size_buffer = 512;
		char buffer[size_buffer];

		switch (flag)
		{
		case 0:
			printf("CLOSING\n");
			symbol = shutdown(addr_socket.socket, SD_SEND);
			if (symbol == SOCKET_ERROR)
			{
				printf("ERROR: closeSocketWrap in thread (Id: %d) - shutdown failed with error: %d\n",
					GetCurrentThreadId(), WSAGetLastError());

				closesocket(addr_socket.socket);

				return 1;
			}

		case 1:
			symbol = recv(addr_socket.socket, buffer, size_buffer, NULL);
			if (symbol == SOCKET_ERROR)
			{
				printf("ERROR: closeSocketWrap in thread (Id: %d) - recvall failed with error: %d\n",
					GetCurrentThreadId(), WSAGetLastError());

				closesocket(addr_socket.socket);

				return 1;
			}
			else if (symbol != 0)
			{
				printf(buffer);
			}

		case 2:
			printf("CLOSING\n");
			symbol = closesocket(addr_socket.socket);
			if (symbol == SOCKET_ERROR)
			{
				printf("ERROR: closeSocketWrap in thread (Id: %d) - close failed with error: %d\n",
					GetCurrentThreadId(), WSAGetLastError());

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
}
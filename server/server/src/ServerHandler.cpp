#include "Header.h"
#include "Other.h"
#include "ServerHandler.h"

namespace Chat
{
	// Function that handles sending the whole message
	inline int32_t ServerHandler::sendall(SOCKET socket,
		char *message,
		int32_t message_size,
		int32_t flags)
	{
		int32_t s_send;
		int32_t counter = 0;

		while (counter++ < _MAX_TRY)
		{
			s_send = send(socket, message, message_size, flags);

			if (s_send == message_size)
			{
				// Transfer was successful
				return 1;
			}
			else if (s_send == 0)
			{
				// Socket was closed
				return 0;
			}
			else if (s_send == SOCKET_ERROR)
			{
				// Transfer ended in an error code
				return SOCKET_ERROR;
			}

			// Handle incomplete message transfer
			message += s_send;
			message_size -= s_send;
		}

		printf("INFO: ServerHandler thread (ID: %d) unable to send whole message!\n",
			GetCurrentThreadId());

		return message_size;
	}


	int32_t ServerHandler::sender()
	{
		uint16_t id = 0;
		std::unique_ptr<IMessage> message;
		std::unique_ptr<char[]> buffer(new char[_MAX_MESSAGE_SIZE + _LENGTH_SIZE + INET6_ADDRSTRLEN]);
		std::vector<ADDR_SOCKET> sockets_for_delete;

		// Test success of the allocation
		if (!buffer)
		{
			printf("ERROR: Server thread (ID: %d) unable to allocate memory!\n",
				GetCurrentThreadId());

			exit(-1);

			return 1;
		}

		while (true)
		{
			std::unique_lock<std::mutex> messageLock(this->messageMutex);

			while (messages.empty()) {
				// Wait for the signalling of a new message
				this->messagesNotEmpty.wait(messageLock);
			}

			while (!messages.empty())
			{
				// Get the first message
				message = std::move(messages.front());
				messages.pop();

				// Leaving mutex section so that the producer does not have to wait
				messageLock.unlock();

				// Copy length of the message, IP of the source and the message itself to the buffer
				uint16_t str_len = (uint16_t)(message->messageLength() + 1);
				memcpy(buffer.get(), &str_len, _LENGTH_SIZE);
				memcpy(buffer.get() + _LENGTH_SIZE, message->getIp(), INET6_ADDRSTRLEN);
				memcpy(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN, (void*)&id, _ID_SIZE);
				memcpy(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, message->getMessage(), str_len);
				id++;

				// Send all of the message to all the servers
				std::unique_lock<std::mutex> socketsLock(this->socketsMutex);

				int8_t symbol;
				for (const ADDR_SOCKET& addr_socket : sockets)
				{
					// Don't send the message back to the sender!
					if (strcmp(addr_socket.clientip, message->getIp()) == 0)
					{
						continue;
					}

					symbol = sendall(addr_socket.socket, buffer.get(),
						str_len + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, NULL);

					// Check for errors while sending the message
					if (symbol == -1)
					{
						printf("ERROR: Server thread (Id: %d) send failed with error: %d\n for socket with IP %s!\n",
							GetCurrentThreadId(), WSAGetLastError(), addr_socket.clientip);

						sockets_for_delete.push_back(addr_socket);
					}
					else if (symbol == 0)
					{
						printf("INFO: Server thread (Id: %d) connection failed for socket with IP %s!\n",
							GetCurrentThreadId(), addr_socket.clientip);

						sockets_for_delete.push_back(addr_socket);
					}
				}

				// Delete sockets that turned out to be invalidated
				if (!sockets_for_delete.empty())
				{
					for (const ADDR_SOCKET& addr_socket : sockets_for_delete) {
						this->removeSocket(addr_socket);
					}

					sockets_for_delete.clear();
				}

				messageLock.lock();
			}
		}
	}

	void ServerHandler::addSocket(const ADDR_SOCKET& addr_socket)
	{
		std::unique_lock<std::mutex> socketsLock(this->socketsMutex);

		sockets.push_back(addr_socket);
	}


	void ServerHandler::addSocket(ADDR_SOCKET&& addr_socket)
	{
		std::unique_lock<std::mutex> socketsLock(this->socketsMutex);

		sockets.push_back(std::move(addr_socket));
	}

	void ServerHandler::removeSocket(const ADDR_SOCKET& addr_socket)
	{
		std::unique_lock<std::mutex> socketsLock(this->socketsMutex);

		std::vector<ADDR_SOCKET>::iterator it = std::find_if(sockets.begin(), sockets.end(),
			[&addr_socket](const ADDR_SOCKET &a) { return addr_socket.socket == a.socket; });
		sockets.erase(it);
	}

	void ServerHandler::sendMessage(std::unique_ptr<IMessage>&& message)
	{
		std::unique_lock<std::mutex> messagesLock(this->messageMutex);

		messages.emplace(std::move(message));

		this->messagesNotEmpty.notify_all();
	}
}
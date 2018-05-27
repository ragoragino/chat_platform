#include "Header.h"
#include "Other.h"
#include "ServerHandler.h"

static std::mutex errorCodeMutex;
static ErrorCodes errorCode = ErrorCodes::Correct;

inline int32_t ServerHandler::sendall(SOCKET socket,
	char *message, 
	int32_t message_size, 
	int32_t flags)
{
	int32_t s_send;

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
			return SOCKET_ERROR;
		}

		// Handle incomplete message transfer
		message += s_send;
		message_size -= s_send;
	}
}


int32_t ServerHandler::sender()
{
	std::unique_ptr<IMessage> message;
	std::unique_ptr<char[]> buffer(new char[_MAX_MESSAGE_SIZE + _LENGTH_SIZE + INET6_ADDRSTRLEN]);
	
	// Test the success of the allocation
	if (!buffer)
	{
		printf("ERROR: Server thread (ID: %d) unable to allocate memory!\n",
			GetCurrentThreadId());

		std::unique_lock<std::mutex> errorLock(errorCodeMutex);
		errorCode = ErrorCodes::MemoryAllocationError;
		errorLock.unlock();

		return 1;
	}

	while (true)
	{
		std::unique_lock<std::mutex> messageLock(this->messageMutex);

		while (messages.empty()) {
			// Check firstly whether the error code has not been signalled
			std::unique_lock<std::mutex> errorLock(errorCodeMutex);
			if (errorCode != ErrorCodes::Correct){
				return 0;
			}
			errorLock.unlock();

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
			memcpy(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN, message->getMessage(), str_len);

			// Send all of the message to all the servers
			std::unique_lock<std::mutex> socketsLock(this->socketsMutex);

			int8_t symbol;
			for (const ADDR_SOCKET& addr_socket : sockets)
			{
				/*if (strcmp(addr_socket.clientip, message->getIp()) == 0)
				{
					continue;
				}*/

				symbol = sendall(addr_socket.socket, buffer.get(),
					str_len + _LENGTH_SIZE + INET6_ADDRSTRLEN, NULL);

				// Check for errors while sending the message
				if (symbol == -1)
				{
					printf("ERROR: Server thread (Id: %d) send failed with error: %d\n for socket with IP %s!\n",
						GetCurrentThreadId(), WSAGetLastError(), addr_socket.clientip);
				}
				else if (symbol == 0)
				{
					printf("INFO: Server thread (Id: %d) connection failed for socket with IP %s!\n",
						GetCurrentThreadId(), addr_socket.clientip);
				}
			}

			socketsLock.unlock();

			messageLock.lock();
		}

		messageLock.unlock();
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
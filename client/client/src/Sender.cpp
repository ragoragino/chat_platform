#include "Header.h"
#include "Other.h"
#include "Sender.h"

static std::mutex errorCodeMutex;
static ErrorCodes errorCode = ErrorCodes::Correct;

int32_t closeSocketWrapper(SOCKET socket, int8_t flag);

inline int32_t Sender::sendall(SOCKET socket,
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


int32_t Sender::sender()
{
	uint16_t id = 0;
	int8_t symbol;
	std::unique_ptr<IMessage> message;
	std::unique_ptr<char[]> buffer(new char[_MAX_MESSAGE_SIZE + _LENGTH_SIZE + INET6_ADDRSTRLEN]);

	// Test the success of the allocation
	if (!buffer)
	{
		printf("ERROR: Sender thread (ID: %d) unable to allocate memory!\n",
			GetCurrentThreadId());

		std::unique_lock<std::mutex> errorLock(errorCodeMutex);
		errorCode = ErrorCodes::MemoryAllocationError;
		errorLock.unlock();

		return 1;
	}

	while (true)
	{
		// Check firstly whether the error code has not been signalled
		std::unique_lock<std::mutex> errorLock(errorCodeMutex);
		if (errorCode != ErrorCodes::Correct) {
			return 0;
		}
		errorLock.unlock();

		// For cases of abrupt end of the program, there needs to be signal of the termination
		memset(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, '\0', 1);

		fgets(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, _MAX_MESSAGE_SIZE, stdin);

		// Handle terminations by typed keywords
		if (strcmp(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, "EXIT\n") == 0)
		{
			return closeSocketWrapper(this->socket, 0);
		}

		// Set the length of the buffer 
		uint16_t str_len = (uint16_t)(strlen(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE) + 1);
		memcpy(buffer.get(), &str_len, _LENGTH_SIZE);
		memset(buffer.get() + _LENGTH_SIZE, 0, INET6_ADDRSTRLEN);
		memcpy(buffer.get() + _LENGTH_SIZE + INET6_ADDRSTRLEN, (void*)&id, _ID_SIZE);
		id++;

		// Send all of the message with address size unspecified
		symbol = sendall(socket, buffer.get(), str_len + _LENGTH_SIZE + INET6_ADDRSTRLEN + _ID_SIZE, NULL);
		// Check for errors while sending the message
		if (symbol == -1)
		{
			printf("ERROR: Sender thread (Id: %d) send failed with error: %d\n",
				GetCurrentThreadId(), WSAGetLastError());

			closeSocketWrapper(this->socket, 0);

			return 1;
		}
		else if (symbol == 0)
		{
			printf("INFO: Sender thread (Id: %d) connection failed!\n",
				GetCurrentThreadId());

			closeSocketWrapper(this->socket, 0);

			return 1;
		}
	}
}
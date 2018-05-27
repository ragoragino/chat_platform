#include "Header.h"
#include "Other.h"
#include "Receiver.h"
#include "Sender.h"

extern ErrorCodes errorCode;
extern std::mutex errorCodeMutex;

int32_t closeSocketWrapper(SOCKET socket, int8_t flag);

inline int32_t Receiver::recvall(char *message,
	int32_t message_size,
	int32_t flags)
{
	int32_t s_recv;

	while (true)
	{
		s_recv = recv(this->socket, message, message_size, NULL);

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
}

int32_t Receiver::receiver()
{
	int8_t symbol = 0;
	char src_address[INET6_ADDRSTRLEN];
	char buffer_size[_LENGTH_SIZE];
	char buffer_id[_ID_SIZE];
	std::unique_ptr<char[]> buffer(new char[_MAX_MESSAGE_SIZE + 1]);
	Dictionary<uint16_t, 5> ids;
	if (!buffer)
	{
		printf("ERROR: Receiver thread (ID: %d) unable to allocate memory!\n",
			GetCurrentThreadId());

		std::unique_lock<std::mutex> errorCodeLock(errorCodeMutex);
		errorCode = ErrorCodes::MemoryAllocationError;
		errorCodeLock.unlock();

		closeSocketWrapper(this->socket, 0);

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
		symbol = this->recvall(src_address, INET6_ADDRSTRLEN, NULL);
		if (symbol != 1) {
			break;
		}
		
		// Receive the size of the message
		/*symbol = this->recvall(buffer_id, _ID_SIZE, NULL);
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
		}*/
		
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

		printf(src_address);
		printf(": ");
		printf(buffer.get());
	}

	int8_t return_symbol = 0;
	if (symbol == -1)
	{
		printf("ERROR: Receiver thread (Id: %d) send failed with error: %d\n",
			GetCurrentThreadId(), WSAGetLastError());

		closeSocketWrapper(this->socket, 0);

		return_symbol = 1;
	}
	else if (symbol == 0)
	{
		printf("INFO: Receiver thread (Id: %d) connection failed!\n",
			GetCurrentThreadId());

		int8_t flag = closeSocketWrapper(this->socket, 0);

		if (flag != 0) {
			return_symbol = flag;
		}

		return_symbol = 0;
	}

	printf("INFO: Receiver thread (ID: %d) exiting!\n",
		GetCurrentThreadId());

	return return_symbol;
}

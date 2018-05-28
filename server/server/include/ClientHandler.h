#pragma once
#include "Header.h"
#include "Other.h"

namespace Chat
{
	class ClientHandler
	{
		public:
			ClientHandler(const ADDR_SOCKET& addr_socket_in) :
				addr_socket(addr_socket_in) {};

			// Function handling receiving of the messages
			int32_t receiver();

		private:
			// Helper function handling the completeness of transfer of a message
			int32_t recvall(char *message, int32_t messageSize, int32_t flags);

			// Function responsible for closing a socket
			int32_t closeSocket(int8_t flag);

			ADDR_SOCKET addr_socket;
	};
}
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

			int32_t receiver();

		private:
			int32_t recvall(char *message, int32_t messageSize, int32_t flags);

			int32_t closeSocket(int8_t flag);

			ADDR_SOCKET addr_socket;
	};
}
#pragma once
#include "Header.h"
#include "Other.h"

namespace Chat
{
	class Sender
	{
	public:
		Sender(const SOCKET& socket_in) : socket(socket_in) {};

		// Function handling sending the messages
		int32_t sender();

	private:
		// Helper function handling completeness of the transfer of a message
		int32_t sendall(SOCKET socket, char *message, int32_t messageSize, int32_t flags);

		int32_t closeSocket(int8_t flag);

		// The socket connection
		SOCKET socket;
	};
}
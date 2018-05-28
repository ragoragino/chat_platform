#pragma once
#include "Header.h"
#include "Other.h"

namespace Chat
{
	class Receiver
	{
	public:
		Receiver(const SOCKET& socket_in) :
			socket(socket_in) {};

		// Function handling receiving the messages	
		int32_t receiver();

	private:
		// Helper function handling completeness of the transfer of a message
		int32_t recvall(char *message, int32_t messageSize, int32_t flags);

		// The socket connection
		SOCKET socket;
	};
}
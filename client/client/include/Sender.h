#pragma once
#include "Header.h"
#include "Other.h"

class Sender
{
public:
	Sender(const SOCKET& socket_in) : socket(socket_in) {};

	// Function handling the sending of the messages
	int32_t sender();

protected:
	// Helper function handling the completeness of transfer of a message
	int32_t sendall(SOCKET socket, char *message, int32_t messageSize, int32_t flags);

private:
	// Vector holding all the sockets
	SOCKET socket;
};
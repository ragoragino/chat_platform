#pragma once
#include "Header.h"
#include "Other.h"

class Receiver
{
public:
	Receiver(const SOCKET& socket_in) :
		socket(socket_in) {};

	int32_t receiver();

protected:
	int32_t recvall(char *message, int32_t messageSize, int32_t flags);

private:
	SOCKET socket;
};
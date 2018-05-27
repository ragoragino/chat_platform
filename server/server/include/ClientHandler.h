#pragma once
#include "Header.h"
#include "Other.h"

class ClientHandler
{
public:
	ClientHandler(const ADDR_SOCKET& addr_socket_in) :
		addr_socket(addr_socket_in), 
		socketState(SocketState::Open) {};

	int32_t receiver();

protected:
	int32_t recvall(char *message, int32_t messageSize, int32_t flags);

	int32_t closeSocket(int8_t flag);

private:
	ADDR_SOCKET addr_socket;
	SocketState socketState;
};
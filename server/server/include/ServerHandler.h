#pragma once
#include "Header.h"
#include "Other.h"

class ServerHandler
{
public:
	// Function that sends messages to the clients
	void sendMessage(std::unique_ptr<IMessage>&& message);

	// Function that adds socket to the vector of sockets 
	void addSocket(const ADDR_SOCKET& socket);

	// Rvalue overload for the addSocket
	void addSocket(ADDR_SOCKET&& socket);

	// Function that removes socket from the vector of sockets
	void removeSocket(const ADDR_SOCKET& socket);

	// Function handling the sending of the messages
	int32_t sender();

protected:
	// Helper function handling the completeness of transfer of a message
	int32_t sendall(SOCKET socket, char *message, int32_t messageSize, int32_t flags);

private:
	// Queue holding messages and associated condition variable and mutex
	std::condition_variable messagesNotEmpty;
	std::mutex messageMutex;
	std::queue<std::unique_ptr<IMessage>> messages;

	// Vector holding all the sockets and associated mutex
	std::mutex socketsMutex;
	std::vector<ADDR_SOCKET> sockets;
};
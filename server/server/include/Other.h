#pragma once
#include "Header.h"

// Compile-time power expression
template<int32_t X, int32_t N>
struct POW
{
	static constexpr int32_t value = X * POW<X, N - 1>::value;
};

template<int32_t X>
struct POW<X, 1>
{
	static constexpr int32_t value = X;
};

// Struct containing client IP and the corresponding socket
struct ADDR_SOCKET
{
	ADDR_SOCKET(SOCKET sckt, char *client_ip) : socket(sckt) {
		memcpy(clientip, client_ip, INET6_ADDRSTRLEN);
	}

	ADDR_SOCKET(const ADDR_SOCKET& addr_socket) : socket(addr_socket.socket) {
		memcpy(clientip, addr_socket.clientip, INET6_ADDRSTRLEN);
	}

	char clientip[INET6_ADDRSTRLEN];
	SOCKET socket;
};

// Enum class for codes signalling application errors
enum class ErrorCodes : int8_t
{
	Correct = 0x01,
	MemoryAllocationError = 0x01,
	MutexReleaseFailure = 0x02,
	Undefined = 0x03
};

// Enum class for codes signalling socket state
enum class SocketState : int8_t
{
	Open = 0x00,
	Closed = 0x01
};

// Dictionary to check for duplicates of messages
template<typename T, size_t N>
class Dictionary
{
public:
	Dictionary() {};

	void add(T id);

	int8_t find(T id);

private:
	T dictionary[N];
	int32_t current = 0;
	bool full = false;
};

template<typename T, size_t N>
void Dictionary<T, N>::add(T id)
{
	if (!full && current == N)
	{
		full = true;
	}

	current %= N;

	dictionary[current++] = id;
}

template<typename T, size_t N>
int8_t Dictionary<T, N>::find(T id)
{
	int32_t end;
	if (full) {
		end = N;
	}
	else {
		end = current;
	}

	for (int i = 0; i != end; i++)
	{
		if (dictionary[i] == id)
		{
			return 1;
		}
	}

	return 0;
}

// Interface for messages
class IMessage
{
public:
	virtual int32_t messageLength() const = 0;

	virtual const char* getMessage() const = 0;

	virtual const char* getIp() const = 0;

	virtual ~IMessage() {};
};

// An implementation of the interface
class Message : public IMessage
{
public:
	Message() = default;

	Message(char *msg, char *src_ip) : message(msg) {
		memcpy(source_ip, src_ip, INET6_ADDRSTRLEN);
	}

	virtual int32_t messageLength() const override { return message.size(); };

	virtual const char* getMessage() const override { return message.data(); };

	virtual const char* getIp() const override { return source_ip; };

	virtual ~Message() {};

private:
	std::string message;
	char source_ip[INET6_ADDRSTRLEN];
};
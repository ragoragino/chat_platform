#pragma once
#include "Header.h"

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

// Struct containing client IP and corresponding socket
struct ADDR_SOCKET
{
	ADDR_SOCKET(char * client_ip, SOCKET sckt)
	{
		memcpy(clientip, client_ip, INET6_ADDRSTRLEN);

		socket = sckt;
	}

	char clientip[INET6_ADDRSTRLEN];
	SOCKET socket;
};

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

class Message
{
public:
	Message() {};

	Message(char *msg, char *src_ip) : message(msg)
	{
		memcpy(source_ip, src_ip, INET6_ADDRSTRLEN);
	}

	int32_t message_length() const { return message.size(); };

	// Return null-terminated array of chars
	const char* get_message() const { return message.data(); };

	const char* get_ip() const { return source_ip; };

private:
	std::string message;
	char source_ip[INET6_ADDRSTRLEN];
};

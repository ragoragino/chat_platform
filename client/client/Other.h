#pragma once

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

struct WRAPPER
{
	SOCKET socket;
	HANDLE handle;
};
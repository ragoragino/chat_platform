#pragma once

#pragma comment(lib,  "ws2_32.lib") 

#include <process.h>
#include <cstdint>
#include <stdio.h>
#include <cassert>
#include <cmath>
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <queue>
#include <string>
#include <algorithm>
#include <vector>
#include <thread>
#include <condition_variable>
#include <utility>
#include <functional>
#include <memory>

#define _LENGTH_SIZE 2 // Size indicating the length of the information buffer in bytes -> uint16_t
#define _MAX_MESSAGE_SIZE POW<2, _LENGTH_SIZE * 8>::value + 1
#define _MAX_SHUTDOWN_MESSAGE_SIZE 512
#define _ID_SIZE 2 
#define _MAX_TRY 10 // How many trials the sender or receiver might try before giving up
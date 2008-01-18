#pragma once

#include "config.h"

#ifndef Types_H
#define Types_H

typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char octet;
typedef unsigned char uchar;

#ifdef WIN32
#include <windows.h> // HANDLE
#ifdef IPV_DUAL_STACK
	#include <mstcpip.h>
#endif
#include <ws2tcpip.h> // SOCKET, socklen_t
#include <winsock2.h>
typedef uint SOCKET;
const HANDLE InvalidHandle = 0;
const SOCKET InvalidSocket = static_cast<SOCKET>(-1);
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h> // sockaddr_in
#include <unistd.h> // close()
#include <cerrno> // errno
typedef int HANDLE;
typedef int SOCKET;
const HANDLE InvalidHandle = -1;
const SOCKET InvalidSocket = -1;
#endif

#endif // Types_H

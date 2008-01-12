#pragma once

#ifndef SocketTypes_GUARD
#define SocketTypes_GUARD

#include "config.h"

#ifdef WIN32
	#include <ws2tcpip.h> // SOCKET, socklen_t
	#include <winsock2.h>
typedef SOCKET fd_t; // uint
#else // POSIX
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h> // sockaddr_in
	#include <unistd.h> // close()
	#include <cerrno> // errno
typedef int fd_t;
#endif

#endif // SocketTypes_GUARD

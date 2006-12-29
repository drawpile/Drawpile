/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef Sockets_INCLUDED
#define Sockets_INCLUDED

#include "../../config.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include <stdexcept>

#include <stdint.h>

#ifdef WIN32
	#if defined( HAVE_WSA )
	#include <winsock2.h>
	#else
	#error Windows socket API was not detected.
	#endif // WSA
	#define MSG_NOSIGNAL 0 // the flag isn't used in win32
	
	// Because MS decided to break BSD socket compatibility.
	#define EINPROGRESS WSAEINPROGRESS
	#define ENOTSOCK WSAENOTSOCK
	#define EISCONN WSAEISCONN
	#define EADDRINUSE WSAEADDRINUSE
	#define EAFNOSUPPORT WSAEAFNOSUPPORT
	//#define EACCES WSAEACCES
	#define ECONNREFUSED WSAECONNREFUSED
	#define ETIMEDOUT WSAETIMEDOUT
	#define ENETUNREACH WSAENETUNREACH
	#define EALREADY WSAEALREADY
	#define ENOTCONN WSAENOTCONN
	#define ECONNRESET WSAECONNRESET
	#define EOPNOTSUPP WSAEOPNOTSUPP
	#define ECONNABORTED WSAECONNABORTED
	#define ENOBUFS WSAENOBUFS
	//#define EPROTO WSAEPROTO
	#define EDESTADDRREQ WSAEDESTADDRREQ
	#define EMSGSIZE WSAEMSGSIZE
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <unistd.h> // close()
	#include <cerrno> // errno
	
	// not defined in non-win32 systems
	#define INVALID_SOCKET -1
	#define SOCKET_ERROR -1
#endif

#ifndef EAGAIN
	#define EAGAIN EWOULDBLOCK
#endif

inline
bool netInit() throw()
{
	#ifndef NDEBUG
	std::cout << "netInit()" << std::endl;
	#endif
	
	#if defined( WIN32 ) and defined( HAVE_WSA )
	WSADATA info;
	if (WSAStartup(MAKEWORD(2,0), &info))
		return false;
	#endif
	
	return true;
}

inline
bool netStop() throw()
{
	#ifndef NDEBUG
	std::cout << "netStop()" << std::endl;
	#endif
	
	#if defined( WIN32 ) and defined( HAVE_WSA )
	WSACleanup();
	#endif
	
	return true;
}

//! Socket abstraction
class Socket
{
protected:
	//! Assigned file descriptor
	int sock;
	
	//! Local address
	sockaddr_in addr;
	
	//! Remote address
	sockaddr_in raddr;
	
	//! Last error number
	int error;
public:
	//! ctor
	Socket() throw()
		: sock(INVALID_SOCKET),
		error(0)
	{
		#ifndef NDEBUG
		std::cout << "Socket::Socket()" << std::endl;
		#endif
	}
	
	//! ctor
	Socket(int nsock) throw()
		: sock(nsock),
		error(0)
	{
		#ifndef NDEBUG
		std::cout << "Socket::Socket(" << nsock << ")" << std::endl;
		#endif
	}
	
	//! dtor
	virtual ~Socket() throw()
	{
		#ifndef NDEBUG
		std::cout << "Socket::~Socket()" << std::endl;
		#endif
		
		close();
	}
	
	//! Close socket
	/**
	 * Closes the socket, and therefore, closes the connection associated with it.
	 */
	void close() throw();
	
	//! Set file descriptor
	/**
	 * @param nsock is the new file descriptor.
	 *
	 * @return file descriptor associated with the class.
	 */
	int fd(int nsock) throw()
	{
		if (sock >= 0) close();
		return sock = nsock;
	}
	
	//! Get file descriptor
	/**
	 * @return file descriptor associated with the class.
	 */
	int fd() const throw() { return sock; }
	
	//! Accept new connection.
	/**
	 * @return NULL if no new connection was made.
	 * @return Socket* if new connection was accepted.
	 */
	Socket* accept() throw(std::bad_alloc);
	
	//! Set blocking
	/**
	 * @bug You can't enable blocking on non-Win32 systems.
	 *
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise.
	 */
	int block(bool x) throw();
	
	//! Bind socket to port and address
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise
	 */
	int bindTo(uint32_t address, uint16_t port) throw();
	
	//! Connect to remote address
	int connect(sockaddr_in* rhost) throw();
	
	//! Set listening
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise.
	 */
	int listen() throw();
	
	//! Send data.
	/**
	 * @param buffer contains data to be send.
	 * @param buflen declares the number of bytes to be sent from buffer.
	 *
	 * @return number of bytes actually sent.
	 * @return (SOCKET_ERROR - 1) if the operation would block.
	 * @return SOCKET_ERROR otherwise.
	 */
	int send(char* buffer, size_t buflen) throw();
	
	//! Receive data
	/**
	 * @param buffer will be written to with max buflen bytes from network.
	 * @param buflen declares the number of bytes to read from network.
	 *
	 * @return number of bytes read.
	 * @return 0 if connection was closed on the other end.
	 * @return (SOCKET_ERROR - 1) if the operation would block.
	 * @return SOCKET_ERROR otherwise.
	 */
	int recv(char* buffer, size_t buflen) throw();
	
	//! Get last error number
	/**
	 * @return last errno.
	 */
	int getError() const throw() { return error; }
	
	//! Get address structure
	/**
	 * @return associated address structure.
	 */
	sockaddr_in* getAddr() throw() { return &addr; }
	
	//! Get local IP address
	/**
	 * @return IP address.
	 */
	int address() const throw() { return ntohl(addr.sin_addr.s_addr); }
	
	//! Get local port
	/**
	 * @return port number.
	 */
	int port() const throw() { return ntohs(addr.sin_port); }
};

#endif // Sockets_INCLUDED

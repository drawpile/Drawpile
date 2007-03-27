/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

#include "../shared/templates.h"

#include <iostream>
#include <stdexcept>
#include <stdint.h>

#ifdef WIN32
	#if defined( HAVE_WSA )
		#include <ws2tcpip.h>
		#include <winsock2.h>
		#if defined( HAVE_XPWSA )
			#include <mswsock.h>
		#endif
	#else
		#error Windows Socket API 2.x required!
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
	#define EWOULDBLOCK WSAEWOULDBLOCK
	//#define EPROTO WSAEPROTO
	#define EDESTADDRREQ WSAEDESTADDRREQ
	#define EMSGSIZE WSAEMSGSIZE
	#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
	//#define ENETDOWN WSAENETDOWN
	typedef SOCKET fd_t;
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <unistd.h> // close()
	#include <cerrno> // errno
	
	// not defined in non-win32 systems
	#define INVALID_SOCKET -1
	#define SOCKET_ERROR -1
	typedef int fd_t;
#endif

#ifndef EAGAIN
	#define EAGAIN EWOULDBLOCK
#endif

#ifdef WIN32
	#define NEED_NET
#endif

//! Net automaton
/**
 * Initializes WSA (ctor) and cleans up after it (dtor).
 *
 * Throws std::exception if it couldn't initialize WSA.
 */
struct Net
{
	Net() throw(std::exception)
	{
		#ifndef NDEBUG
		std::cout << "Net::Net()" << std::endl;
		#endif
		
		#if defined( HAVE_WSA )
		WSADATA info;
		if (WSAStartup(MAKEWORD(2,0), &info))
		{
			throw std::exception();
		}
		
		if (LOBYTE(info.wVersion) != 2
			or HIBYTE(info.wVersion) != 0)
		{
			std::cerr << "Invalid WSA version." << std::endl;
			WSACleanup( );
			return; 
		}
		
		#endif
	}
	
	~Net() throw()
	{
		#ifndef NDEBUG
		std::cout << "Net::~Net()" << std::endl;
		#endif
		
		#if defined( HAVE_WSA )
		WSACleanup();
		#endif
	}
};

//! Socket abstraction
class Socket
{
protected:
	//! Assigned file descriptor
	fd_t sock;
	
	#ifdef IPV6_SUPPORT
	sockaddr_in6
	#else
	sockaddr_in
	#endif
		//! Address
		addr;
	
	//! Last error number (from errno or equivalent)
	int error;
	//! Socket state
	bool active;
public:
	//! ctor
	Socket() throw()
		: sock(INVALID_SOCKET),
		error(0)
	{
		#ifdef DEBUG_SOCKETS
		#ifndef NDEBUG
		std::cout << "Socket::Socket()" << std::endl;
		#endif
		#endif
	}
	
	#ifdef IPV6_SUPPORT
	Socket(fd_t& nsock, const sockaddr_in6 saddr) throw()
	#else
	Socket(fd_t& nsock, const sockaddr_in saddr) throw()
	#endif
		: sock(nsock),
		error(0)
	{
		#ifdef DEBUG_SOCKETS
		#ifndef NDEBUG
		std::cout << "Socket::Socket(fd_t)" << std::endl;
		#endif
		#endif
		
		memcpy(&addr, &saddr, sizeof(addr));
	}
	
	//! ctor
	Socket(fd_t nsock) throw()
		: sock(nsock),
		error(0)
	{
		#ifdef DEBUG_SOCKETS
		#ifndef NDEBUG
		std::cout << "Socket::Socket(" << nsock << ")" << std::endl;
		#endif
		#endif
	}
	
	//! dtor
	virtual ~Socket() throw()
	{
		#ifdef DEBUG_SOCKETS
		#ifndef NDEBUG
		std::cout << "Socket::~Socket()" << std::endl;
		#endif
		#endif
		
		close();
	}
	
	//! Create new socket
	fd_t create() throw();
	
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
	fd_t fd(fd_t nsock) throw()
	{
		if (sock != INVALID_SOCKET) close();
		if (nsock != INVALID_SOCKET) active = true;
		return sock = nsock;
	}
	
	//! Get file descriptor
	/**
	 * @return reference to file descriptor associated with the class.
	 */
	fd_t fd() const throw() { return sock; }
	
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
	bool block(const bool x) throw();
	
	//! Re-use socket address
	/**
	 * Sets SO_REUSEPORT for the socket.
	 */
	bool reuse(const bool x) throw();
	
	//! Set/unset lingering
	/**
	 * Sets SO_LINGER for the socket.
	 *
	 * "Lingers on close if unsent data is present."
	 */
	bool linger(const bool x, const uint16_t delay) throw();
	
	//! Bind socket to port and address
	/**
	 * @return 0 on success.
	 * @return SOCKET_ERROR otherwise
	 */
	int bindTo(const std::string address, const uint16_t port) throw();
	
	//! Connect to remote address
	#ifdef IPV6_SUPPORT
	int connect(const sockaddr_in6* rhost) throw();
	#else
	int connect(const sockaddr_in* rhost) throw();
	#endif
	
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
	int send(char* buffer, const size_t buflen) throw();
	
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
	int recv(char* buffer, const size_t buflen) throw();
	
	#if defined(WITH_SENDFILE) or defined(HAVE_XPWSA)
	//! Sendfile interface
	/**
	 * Note for Windows!
	 * - Blocks and sends full data in one go.
	 * - Offset is also ignored.
	 *
	 * @param fd FD of the file to be sent.
	 * @param offset is the starting offset in the file for sending.
	 * @param nbytes is the number of bytes to be sent.
	 * @param sbytes is the sent bytes.
	 *
	 * @return -1 on error, 0 otherwise.
	 */
	int sendfile(fd_t fd, off_t offset, size_t nbytes, off_t& sbytes) throw();
	#endif // WITH_SENDFILE or HAVE_XPWSA
	
	//! Get last error number
	/**
	 * @return last errno.
	 */
	int getError() const throw() { return error; }
	
	//! Get address structure
	/**
	 * @return associated address structure.
	 */
	#ifdef IPV6_SUPPORT
	sockaddr_in6* getAddr() throw()
	#else // IPv4
	sockaddr_in* getAddr() throw()
	#endif // IPV6_SUPPORT
	{
		return &addr;
	}
	
	//! Get IP address
	/**
	 * @return IP address string.
	 * 	e.g. ::1:30000 or 127.0.0.1:300000
	 */
	std::string address() const throw();
	
	//! Get port
	/**
	 * @return port number.
	 */
	uint16_t port() const throw();
	
	//! Check if the address matches
	bool matchAddress(Socket* tsock) throw();
	
	//! Check if the port matches
	bool matchPort(const Socket* tsock) const throw();
	
	/* Operator overloads */
	
	//! operator== overload (Socket*)
	bool operator== (const Socket* tsock) const throw() { return (sock == tsock->fd()); }
	
	//! operator== overload (fd_t)
	bool operator== (const fd_t& _fd) const throw() { return (sock == _fd); }
	
	//! operator= overload (Socket*)
	Socket* operator= (Socket* tsock) throw() { fd(tsock->fd()); return this; }
	
	//! operator= overload (fd_t)
	fd_t operator= (fd_t& _fd) throw() { fd(_fd); return sock; }
};

#ifdef SOCKET_OSTREAM
std::ostream& operator<< (std::ostream& os, const Socket& sock)
{
	os << sock.address();
}
#endif

#endif // Sockets_INCLUDED

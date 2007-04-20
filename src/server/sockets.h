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

#include "config.h"

#include "../shared/templates.h"

#include <iostream>
#include <stdexcept>
#include <stdint.h>

#ifdef IPV6_SUPPORT
#define IPV6_SUPPORT_INCOMPLETE 1
#endif

#ifdef WIN32
	#include <ws2tcpip.h>
	#include <winsock2.h>
	#if defined( HAVE_XPWSA )
		#include <mswsock.h>
	#endif
	
	#define EWOULDBLOCK WSAEWOULDBLOCK
	
	#define EINPROGRESS WSAEINPROGRESS
	#define ENETDOWN WSAENETDOWN
	//#define EMFILE WSAEMFILE
	#define ENOBUFS WSAENOBUFS
	//#define EINTR WSAEINTR
	#define EWOULDBLOCK WSAEWOULDBLOCK
	#define ECONNABORTED WSAECONNABORTED
	#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
	#define EADDRINUSE WSAEADDRINUSE
	//#define EACCES WSAEACCES
	#define ECONNREFUSED WSAECONNREFUSED
	#define ETIMEDOUT WSAETIMEDOUT
	#define ENETUNREACH WSAENETUNREACH
	#define ECONNRESET WSAECONNRESET
	#define ESHUTDOWN WSAESHUTDOWN
	#define EDISCON WSAEDISCON
	#define ENETRESET WSAENETRESET
	#define EAFNOSUPPORT WSAEAFNOSUPPORT
	#define ENOTCONN WSAENOTCONN
	#define EISCONN WSAEISCONN
	#define EALREADY WSAEALREADY
	
	// winows only
	//#define WSA_IO_PENDING
	//#define WSA_OPERATION_ABORTED
	
	// programming errors
	//#define EFAULT WSAEFAULT
	//#define EBADF WSAEBADF
	#define ENOTSOCK WSAENOTSOCK
	#define ENOPROTOOPT WSAENOPROTOOPT
	#define EPROTOTYPE WSAEPROTOTYPE
	#define ESOCKTNOSUPPORT WSAESOCKTNOSUPPORT
	#define EOPNOTSUPP WSAEOPNOTSUPP
	#define EPROTONOSUPPORT WSAEPROTONOSUPPORT
	
	#define MSG_NOSIGNAL 0 // the flag isn't used in win32
	typedef SOCKET fd_t;
	
	#define NEED_NET
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

//! Net automaton
/**
 * Initializes WSA (ctor) and cleans up after it (dtor).
 *
 * Throws std::exception if it couldn't initialize WSA.
 */
struct Net
{
	Net() throw(std::exception);
	~Net() throw();
};

//! Socket abstraction
class Socket
{
protected:
	//! Assigned file descriptor
	fd_t sock;
	
	//! Address
	#ifdef IPV6_SUPPORT
	sockaddr_in6 addr, r_addr;
	#else // IPv4
	sockaddr_in addr, r_addr;
	#endif
	
	//! Last error number (from errno or equivalent)
	int s_error;
public:
	//! ctor
	Socket() throw()
		: sock(INVALID_SOCKET),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket::Socket()" << std::endl;
		#endif
	}
	
	#ifdef IPV6_SUPPORT
	Socket(fd_t& nsock, const sockaddr_in6 saddr) throw()
		: sock(nsock),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket(FD: "<<nsock<<", address: " << AddrToString(saddr) << ") constructed" << std::endl;
		#endif
		
		memcpy(&addr, &saddr, sizeof(saddr));
	}
	#endif
	
	Socket(fd_t& nsock, const sockaddr_in saddr) throw()
		: sock(nsock),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket(FD: "<<nsock<<", address: " << AddrToString(saddr) << ") constructed" << std::endl;
		#endif
		
		memcpy(&addr, &saddr, sizeof(saddr));
	}
	
	//! ctor
	Socket(fd_t nsock) throw()
		: sock(nsock),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket(FD: " << nsock << ") constructed" << std::endl;
		#endif
	}
	
	//! dtor
	~Socket() throw()
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "~Socket(FD: " << sock << ") destructed" << std::endl;
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
	
	//! releases FD association from this
	fd_t release() throw()
	{
		fd_t t_sock = sock;
		sock = INVALID_SOCKET;
		return t_sock;
	}
	
	//! Set file descriptor
	/**
	 * @param nsock is the new file descriptor.
	 *
	 * @return file descriptor associated with the class.
	 */
	fd_t fd(fd_t nsock) throw()
	{
		if (sock != INVALID_SOCKET) close();
		return sock = nsock;
	}
	
	//! Get file descriptor
	/**
	 * @return reference to file descriptor associated with the class.
	 */
	fd_t fd() const throw() { return sock; }
	
	//! Accept new connection.
	/**
	 * @return Socket* if new connection was accepted, NULL otherwise.
	 */
	Socket accept() throw();
	
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
	 * @return 0 on success, SOCKET_ERROR otherwise.
	 */
	int bindTo(const std::string& address, const uint16_t port) throw();
	
	//! Connect to remote address
	#ifdef IPV6_SUPPORT
	int connect(const sockaddr_in6& rhost) throw();
	#endif
	int connect(const sockaddr_in& rhost) throw();
	
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
	int getError() const throw() { return s_error; }
	
	//! Get address structure
	/**
	 * @return associated address structure.
	 */
	#ifdef IPV6_SUPPORT
	sockaddr_in6& getAddr() throw()
	#else // IPv4
	sockaddr_in& getAddr() throw()
	#endif
	{
		return addr;
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
	bool matchAddress(const Socket& tsock) const throw();
	
	//! Check if the port matches
	bool matchPort(const Socket& tsock) const throw();
	
	#ifdef IPV6_SUPPORT
	static std::string AddrToString(const sockaddr_in6& raddr) throw();
	#endif
	static std::string AddrToString(const sockaddr_in& raddr) throw();
	
	#ifdef IPV6_SUPPORT
	static sockaddr_in6 StringToAddr(std::string const& address) throw();
	#endif
	static sockaddr_in StringToAddr(std::string const& address) throw();
	
	/* Operator overloads */
	
	//! operator== overload (Socket*)
	bool operator== (const Socket& tsock) const throw()
	{
		return (sock == tsock.sock);
	}
	
	//! operator== overload (fd_t)
	bool operator== (const fd_t& _fd) const throw()
	{
		return (sock == _fd);
	}
	
	//! operator= overload (Socket*)
	Socket& operator= (Socket& tsock) throw()
	{
		sock = tsock.sock;
		return *this;
	}
	
	//! operator= overload (fd_t)
	fd_t operator= (fd_t& _fd) throw()
	{
		sock = _fd;
		return sock;
	}
};

#ifdef SOCKET_OSTREAM
std::ostream& operator<< (std::ostream& os, const Socket& sock)
{
	os << sock.address();
}
#endif

#endif // Sockets_INCLUDED

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

#include "common.h"

#include <iostream>
#include <stdexcept>

//! Network constants
namespace Network {

//! IPv6 related constants
namespace IPv6 {

//! Localhost address
/**
 * Equivalent of IPv4 \b 127.0.0.1
 */
const char Localhost[] = "::1";

//! Unspecified address
/**
 * Equivalent of IPv4 \b 0.0.0.0
 */
const char Unspecified[] = "::";

//! Maximum length of IPv6 address
/**
 * ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
 */
const uint AddrLength = 39;

}

//! IPv4 related constants
namespace IPv4 {

//! Localhost address
const char Localhost[] = "127.0.0.1";

//! Unspecified address
const char Unspecified[] = "0.0.0.0";

//! Maximum length of IPv4 address
/**
 * 123.456.789.012
 */
const uint AddrLength = 16;

}

//! Super user port upper bound
const uint SuperUser_Port = 1023;

//! Maximum length of either IPv4 or IPv6 address
const uint AddrLength = IPv6::AddrLength;

//! Maximum length of port number as string
const uint PortLength = 5;

//! Highest port number
const uint PortUpperBound = 65535;

//! Lowest port number
const uint PortLowerBound = 0;

}

#ifdef WIN32
	#include <ws2tcpip.h>
	#include <winsock2.h>
	#if defined( HAVE_MSWSOCK_H )
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
	
	#define SHUT_RD SD_RECEIVE
	#define SHUT_WR SD_SEND
	#define SHUT_RDWR SD_BOTH
	
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

#ifdef NEED_NET
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
#endif

//! Address
struct Address {
	Address();
	
	enum Family {
		IPV4,
		IPV6
	} type;
	
	union {
		//! base address
		sockaddr addr;
		//! IPv4 address
		sockaddr_in IPv4;
		#ifdef IPV6_SUPPORT
		//! IPv6 address
		sockaddr_in6 IPv6;
		#endif
	};
	
	socklen_t size() const throw();
	
	int family() const throw();
	
	ushort port() const throw();
	
	void port(ushort _port) throw();
	
	//! Assign operator
	Address& operator= (const Address& naddr) throw();
	
	//! Is-equal operator
	/**
	 * @bug Comparing IPv6 addresses likely doesn't work.
	 */
	bool operator== (const Address& naddr) const throw();
};

void initAddress(Address& addr) throw();

//! Socket abstraction
class Socket
{
protected:
	//! Assigned file descriptor
	fd_t sock;
	
	//! Address
	Address addr;
	
	/*
	#ifdef SC_SOCKETS
	std::list<msghdr*> msgs;
	#endif
	*/
	
	//! Last error number (from errno or equivalent)
	int s_error;
public:
	//! Default constructor
	Socket() throw()
		: sock(INVALID_SOCKET),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket::Socket()" << std::endl;
		#endif
	}
	
	//! ctor
	/**
	 * @param[in] nsock FD to associate with this Socket
	 * @param[in] saddr Address to associate with this Socket
	 */
	Socket(fd_t& nsock, const Address saddr) throw()
		: sock(nsock),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket(FD: "<<nsock<<", address: " << AddrToString(saddr) << ") constructed" << std::endl;
		#endif
		
		addr = saddr;
	}
	
	//! ctor
	/**
	 * @param[in] nsock FD to associate with this Socket
	 */
	Socket(fd_t nsock) throw()
		: sock(nsock),
		s_error(0)
	{
		#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
		std::cout << "Socket(FD: " << nsock << ") constructed" << std::endl;
		#endif
	}
	
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
	 * @param[in] nsock is the new file descriptor.
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
	 * @return Socket if new connection was accepted
	 * @note (Socket.getFD() == INVALID_SOCKET) if no new connection was accepted
	 */
	Socket accept() throw();
	
	//! Set blocking
	/**
	 * @param[in] x enable/disable blocking
	 *
	 * @note You can't re-enable blocking on non-Win32 systems (API limitation?).
	 */
	bool block(const bool x) throw();
	
	//! Re-use socket port
	/**
	 * Sets SO_REUSEPORT for the socket. Allows multiple apps to bind to same port and address.
	 * Primarily used for multi-casting.
	 *
	 * @param[in] x enable/disable port re-use
	 *
	 * @note In Win32, this causes behaviour similar to reuse_addr() does in all other systems.
	 */
	bool reuse_port(const bool x) throw();
	
	//! Re-use socket address
	/**
	 * Sets SO_REUSEADDR for the socket. Allows socket port to be re-used as soon as it's
	 * freed, avoiding the delay caused from waiting TIME_WAIT to expire.
	 *
	 * @param[in] x enable/disable address re-use
	 *
	 * @note In Win32, this does nothing as TIME_WAIT is ignored completely there.
	 */
	bool reuse_addr(const bool x) throw();
	
	//! Set/unset lingering
	/**
	 * Sets SO_LINGER for the socket.
	 *
	 * "Lingers on close if unsent data is present."
	 *
	 * @param[in] x enable/disable lingering
	 * @param[in] delay linger time if enabled
	 */
	bool linger(const bool x, const ushort delay) throw();
	
	//! Bind socket to port and address
	/**
	 * @param[in] address address string to bind to
	 * @param[in] port port number to bind to
	 *
	 * @note This constructs Address from the string and port and passes it to bindTo(const Address&)
	 *
	 * @retval 0 on success
	 * @retval SOCKET_ERROR on error
	 */
	int bindTo(const std::string& address, const ushort port) throw();
	
	//! Bind socket to port and address
	/**
	 * @param[in] address Address to bind to
	 *
	 * @retval 0 on success
	 * @retval SOCKET_ERROR on error
	 */
	int bindTo(const Address& address) throw();
	
	//! Connect to address
	/**
	 * @param[in] rhost host to connect to
	 */
	int connect(const Address& rhost) throw();
	
	//! Set listening
	/**
	 * @retval 0 on success
	 * @retval SOCKET_ERROR on error
	 */
	int listen() throw();
	
	//! Send data.
	/**
	 * @param[in] buffer Data to be send.
	 * @param[in] buflen Number of bytes to send from buffer
	 *
	 * @return number of bytes actually sent.
	 * @retval SOCKET_ERROR on error
	 */
	int send(char* buffer, const size_t buflen) throw();
	
	/*
	#ifdef HAVE_SENDMSG
	//! Scatter-Gather variant of send()
	int sc_send(std::list<Array<char*,size_t>*> buffers) throw();
	#endif
	*/
	
	//! Receive data
	/**
	 * @param[out] buffer Target buffer
	 * @param[in] buflen Number of bytes to read from network.
	 *
	 * @return number of bytes read.
	 * @retval 0 if connection was closed on the other end.
	 * @retval SOCKET_ERROR on error.
	 */
	int recv(char* buffer, const size_t buflen) throw();
	
	/*
	#ifdef HAVE_RECVMSG
	//! Scatter-Gather variant of recv()
	int sc_recv(std::list<Array<char*,size_t>*> buffers) throw();
	#endif
	*/
	
	#if defined(WITH_SENDFILE) or defined(HAVE_XPWSA)
	//! Sendfile interface
	/**
	 * Note for Windows!
	 * - Blocks and sends full data in one go.
	 * - Offset is also ignored.
	 *
	 * @param[in] fd FD of the file to be sent.
	 * @param[in] offset is the starting offset in the file for sending.
	 * @param[in] nbytes is the number of bytes to be sent.
	 * @param[out] sbytes is the sent bytes.
	 *
	 * @retval -1 on error
	 */
	int sendfile(fd_t fd, off_t offset, size_t nbytes, off_t& sbytes) throw();
	#endif // WITH_SENDFILE or HAVE_XPWSA
	
	//! Shutdown socket
	/**
	 * @param[in] how SHUT_RD, SHUT_WR, SHUT_RDWR
	 */
	int shutdown(int how) throw()
	{
		return ::shutdown(sock, how);
	}
	
	//! Get last error number
	/**
	 * @return Last error number (errno).
	 */
	int getError() const throw() { return s_error; }
	
	//! Get address structure
	/**
	 * @return Associated address structure.
	 */
	Address& getAddr() throw()
	{
		return addr;
	}
	
	//! Get IP address
	/**
	 * @return IP address string.
	 * (e.g. [::1]:30000 or 127.0.0.1:30000)
	 */
	std::string address() const throw();
	
	//! Get port
	/**
	 * @return Local port number
	 */
	ushort port() const throw();
	
	//! Convert address to string representation of it
	/**
	 * @param[in] raddr address to translate
	 */
	static std::string AddrToString(const Address& raddr) throw();
	
	//! Convert string to address
	/**
	 * @param[in] address string to convert
	 */
	static Address StringToAddr(std::string const& address) throw();
	
	/* Operator overloads */
	
	#ifdef SOCKET_OPS
	//! operator== overload (Socket&)
	bool operator== (const Socket& tsock) const throw()
	{
		return (sock == tsock.sock);
	}
	
	//! operator= overload (Socket&)
	Socket& operator= (Socket& tsock) throw()
	{
		if (sock != INVALID_SOCKET)
			close(sock);
		sock = tsock.sock;
		return *this;
	}
	#endif
};

#endif // Sockets_INCLUDED

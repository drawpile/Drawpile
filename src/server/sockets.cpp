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

#include "sockets.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include <fcntl.h>
#include <cassert>

Net::Net() throw(std::exception)
{
	#ifndef NDEBUG
	std::cout << "Net::Net()" << std::endl;
	#endif
	
	#if defined( HAVE_WSA )
	WSADATA info;
	if (WSAStartup(MAKEWORD(2,0), &info))
		throw std::exception();
	
	if (LOBYTE(info.wVersion) != 2
		or HIBYTE(info.wVersion) != 0)
	{
		std::cerr << "Invalid WSA version." << std::endl;
		WSACleanup( );
		exit(1); 
	}
	#endif
}

Net::~Net() throw()
{
	#ifndef NDEBUG
	std::cout << "Net::~Net()" << std::endl;
	#endif
	
	#if defined( HAVE_WSA )
	WSACleanup();
	#endif
}

/* *** Socket class *** */

fd_t Socket::create() throw()
{
	#ifdef WSA_SOCKETS
	
	#ifdef IPV6_SUPPORT
	sock = WSASocket(AF_INET6, SOCK_STREAM, 0, 0, 0, /*WSA_FLAG_OVERLAPPED*/);
	#else // IPv4
	sock = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, /*WSA_FLAG_OVERLAPPED*/);
	#endif
	
	#else // POSIX
	
	#ifdef IPV6_SUPPORT
	sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	#else // IPv4
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	#endif
	
	#endif
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef WSA_SOCKETS
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != WSAEAFNOSUPPORT);
		assert(s_error != WSAEPROTONOSUPPORT);
		assert(s_error != WSAEPROTOTYPE);
		assert(s_error != WSAESOCKTNOSUPPORT);
		assert(s_error != WSAEINVAL);
		
		switch (s_error)
		{
		#ifdef WSA_SOCKETS
		case WSAEINPROGRESS:
			break;
		case WSAENETDOWN:
			std::cerr << "socket: network sub-system failure" << std::endl;
			break;
		case WSAEMFILE:
			std::cerr << "socket: socket limit reached" << std::endl;
			break;
		case WSAENOBUFS:
			std::cerr << "socket: out of buffers" << std::endl;
			break;
		#else // POSIX
		// TODO
		#endif
		default:
			std::cerr << "Socket::create() - unknown error: " << s_error << std::endl;
			assert(s_error);
			break;
		}
	}
	
	return sock;
}

void Socket::close() throw()
{
	#if defined(HAVE_XPWSA)
	::DisconnectEx(sock, 0, TF_REUSE_SOCKET, 0);
	#elif defined(WSA_SOCKETS)
	::closesocket(sock);
	#else // POSIX
	::close(sock);
	#endif
	
	sock = INVALID_SOCKET;
}

Socket* Socket::accept() throw(std::bad_alloc)
{
	assert(sock != INVALID_SOCKET);
	
	// temporary address struct
	#ifdef IPV6_SUPPORT
	sockaddr_in6 sa;
	#else
	sockaddr_in sa;
	#endif
	
	#ifdef WSA_SOCKETS
	int addrlen
	#else
	socklen_t addrlen
	#endif
		= sizeof(sa);
	
	#ifdef WSA_SOCKETS
	fd_t n_fd = ::WSAAccept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen, 0, 0);
	#else // POSIX
	fd_t n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &addrlen);
	#endif
	
	if (n_fd != INVALID_SOCKET)
	{
		Socket *s = new Socket(n_fd, sa);
		memcpy(s->getAddr(), &sa, sizeof(addr));
		
		return s;
	}
	else
	{
		#ifdef WSA_SOCKETS
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		#ifdef EPROTO
		assert(s_error != EPROTO);
		#endif
		
		#ifdef LINUX
		// no idea what these are
		assert(s_error != ENOSR); // ?
		assert(s_error != ESOCKTNOSUPPORT); // ?
		assert(s_error != EPROTONOSUPPORT); // Protocol not supported
		assert(s_error != ETIMEDOUT); // Timed out
		assert(s_error != ERESTARTSYS); // ?
		#endif
		
		switch (s_error)
		{
		#ifdef WSA_SOCKETS
		case WSAEINTR: // interrupted
		case WSAEWOULDBLOCK: // would block
			break;
		case WSAEMFILE:
			std::cerr << "socket: process FD limit reached" << std::endl;
			break;
		case WSAENFILE:
			std::cerr << "socket: system FD limit reached" << std::endl;
			break;
		case WSAENOMEM:
			std::cerr << "socket: out of memory" << std::endl;
			break;
		case WSAENOBUFS:
			std::cerr << "socket: out of network buffers" << std::endl;
			break;
		case WSAEPERM:
			std::cerr << "socket: firewall blocked incoming connection" << std::endl;
			break;
		case WSAECONNABORTED:
			std::cerr << "socket: incoming connection aborted" << std::endl;
			break;
		#else // POSIX
		case EINTR:
		case EAGAIN:
			// retry
			break;
		case EMFILE:
			std::cerr << "socket: process FD limit reached" << std::endl;
			break;
		case ENFILE:
			std::cerr << "socket: system FD limit reached" << std::endl;
			break;
		case ENOMEM:
			std::cerr << "socket: out of memory" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "socket: out of network buffers" << std::endl;
			break;
		case EPERM:
			std::cerr << "socket: firewall blocked incoming connection" << std::endl;
			break;
		case ECONNABORTED:
			std::cerr << "socket: incoming connection aborted" << std::endl;
			break;
		#endif
		default:
			std::cerr << "Socket::accept() - unknown error: " << s_error << std::endl;
			exit(1);
		}
		
		return 0;
	}
}

bool Socket::block(const bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::block(fd: " << sock << ", " << (x?"true":"false") << ")" << std::endl;
	#endif // NDEBUG
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WSA_SOCKETS
	uint32_t arg = (x ? 1 : 0);
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else // POSIX
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif
}

bool Socket::reuse(const bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::reuse(fd: " << sock << ", " << (x?"true":"false") << ")" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifndef SO_REUSEPORT
	// Windows (for example) does not have it
	return (x==true);
	#else // POSIX
	int val = (x ? 1 : 0);
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
	
	if (r == SOCKET_ERROR)
	{
		s_error = errno;
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		
		std::cerr << "Socket::reuse() - unknown error: " << s_error << std::endl;
		exit(1);
	}
	
	return (r == 0);
	#endif
}

bool Socket::linger(const bool x, const uint16_t delay) throw()
{
	::linger lval;
	lval.l_onoff = (x ? 1 : 0);
	lval.l_linger = delay;
	
	const int r = setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lval), sizeof(lval));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif
		
		#ifdef WSA_SOCKETS
		assert(s_error != WSAEBADF);
		assert(s_error != WSAENOTSOCK);
		assert(s_error != WSAENOPROTOOPT);
		assert(s_error != WSAEFAULT);
		#else // POSIX
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != ENOPROTOOPT);
		assert(s_error != EFAULT);
		#endif
		
		std::cerr << "Socket::linger() - unknown error: " << s_error << std::endl;
		exit(1);
	}
	else
		return (r == 0);
}

/** \todo move string-to-address to its own function */
int Socket::bindTo(const std::string address, const uint16_t port) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::bindTo([" << address << "], " << port << ")" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef HAVE_WSA
	#ifdef IPV6_SUPPORT
	char straddr[INET6_ADDRSTRLEN+1];
	#else // IPv4
	char straddr[14+1];
	#endif
	
	memcpy(straddr, address.c_str(), address.length());
	
	int size = sizeof(addr);
	#ifdef IPV6_SUPPORT
	WSAStringToAddress(straddr, AF_INET6, 0, reinterpret_cast<sockaddr*>(&addr), &size);
	#else // IPv4
	WSAStringToAddress(straddr, AF_INET, 0, reinterpret_cast<sockaddr*>(&addr), &size);
	#endif
	
	#else // POSIX
	
	#ifdef IPV6_SUPPORT
	inet_pton(AF_INET6, address.c_str(), &(addr.sin6_addr));
	#else // IPv4
	inet_pton(AF_INET, address.c_str(), &(addr.sin_addr));
	#endif
	
	#endif
	
	#ifdef IPV6_SUPPORT
	addr.sin6_family = AF_INET6;
	bswap(addr.sin6_port = port);
	#else // IPv4
	addr.sin_family = AF_INET;
	bswap(addr.sin_port = port);
	#endif // IPV6_SUPPORT
	
	const int r = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif // HAVE_WSA
		
		// programming errors
		
		assert(s_error != EBADF);
		// According to docs, this may change in the future.
		assert(s_error != EINVAL);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EISCONN);
		
		switch (s_error)
		{
		#ifdef WSA_SOCKETS
		case WSAEADDRINUSE:
			std::cerr << "socket: address already in use" << std::endl;
			break;
		case WSAEADDRNOTAVAIL:
			std::cerr << "socket: address not available" << std::endl;
			break;
		case WSAENOBUFS:
			std::cerr << "socket: out of network buffers" << std::endl;
			break;
		case WSAEACCES:
			std::cerr << "socket: can't bind to super-user sockets" << std::endl;
			break;
		#else // POSIX
		case EADDRINUSE:
			std::cerr << "socket: address already in use" << std::endl;
			break;
		case EADDRNOTAVAIL:
			std::cerr << "socket: address not available" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "socket: out of network buffers" << std::endl;
			break;
		case EACCES:
			std::cerr << "socket: can't bind to super-user sockets" << std::endl;
			break;
		#endif
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::bindTo() - unknown error: " << s_error << std::endl;
			#endif // NDEBUG
			exit(1);
		}
	}
	
	return r;
}

#ifdef IPV6_SUPPORT
int Socket::connect(const sockaddr_in6* rhost) throw()
{
	// TODO
	assert(1);
	return 0;
}
#endif

int Socket::connect(const sockaddr_in* rhost) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::connect()" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WSA_SOCKETS
	const int r = WSAConnect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost), 0, 0, 0, 0);
	#else // POSIX
	const int r = ::connect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost));
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != ENOTSOCK);
		assert(s_error != EISCONN);
		assert(s_error != EADDRINUSE);
		assert(s_error != EAFNOSUPPORT);
		assert(s_error != EALREADY);
		
		switch (s_error)
		{
		#ifdef WSA_SOCKETS
		case WSAEWOULDBLOCK:
			return 2;
		case WSAEINPROGRESS:
			break;
		case WSAEACCES:
		case WSAEPERM:
			// firewall rule blocked operation
			break;
		case WSAECONNREFUSED:
		case WSAETIMEDOUT:
		case WSAENETUNREACH:
			break;
		#else // POSIX
		case EINPROGRESS:
			break;
		case EACCES:
		case EPERM:
			std::cerr << "socket: firewall denied connection" << std::endl;
			break;
		case ECONNREFUSED:
		case ETIMEDOUT:
		case ENETUNREACH:
			break;
		case EAGAIN:
			// retry
			return 2;
		#endif
		}
	}
	
	return r;
}

int Socket::listen() throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::listen()" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	const int r = ::listen(sock, 4);
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif
		
		assert(s_error != EBADF);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		
		#ifndef NDEBUG
		std::cerr << "Socket::listen() - unknown error: " << s_error << std::endl;
		#endif // NDEBUG
		exit(1);
	}
	else
		return r;
}

int Socket::send(char* buffer, const size_t len) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::send(*buffer, " << len << ")" << std::endl;
	#endif
	
	assert(buffer != 0);
	assert(len > 0);
	assert(sock != INVALID_SOCKET);
	
	#ifdef WSA_SOCKETS
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD sb;
	const int r = ::WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	#else // POSIX
	const int r = ::send(sock, buffer, len, MSG_NOSIGNAL);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		s_error = ::WSAGetLastError();
		#else // POSIX
		s_error = ::errno;
		#endif
		
		// programming errors
		#ifdef WSA_SOCKETS
		// invalid address for: lpBuffers, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine
		assert(s_error != WSAEFAULT);
		assert(s_error != WSAEINVAL);
		assert(s_error != WSANOTINITIALISED);
		#else
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		assert(s_error != EOPNOTSUPP);
		#endif
		
		switch (s_error)
		{
		#ifdef WSA_SOCKETS
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		case WSAEINTR: // iterrupted
		case WSAEWOULDBLOCK: // would block
			break;
		case WSAEPIPE: // pipe broken
		case WSAECONNRESET: // connection reset
			break;
		case WSAENOMEM: // out of memory
		case WSAENOBUFS: // out of network buffers
			break;
		#else // POSIX
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			break;
		case ENOMEM:
		case ENOBUFS:
			break;
		#endif
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::send() - unknown error: " << s_error << std::endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WSA_SOCKETS
		return r;
		#endif
	}
	
	#ifdef WSA_SOCKETS
	return sb;
	#else
	return r;
	#endif
}

int Socket::recv(char* buffer, const size_t len) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::recv(*buffer, " << len << ")" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	assert(buffer != 0);
	assert(len > 0);
	
	// WSA causes WSAEFAULT error to occur for some reason
	
	#ifdef WSA_SOCKETS
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD flags=0;
	DWORD rb;
	const int r = WSARecv(sock, &wbuf, 1, &rb, &flags, 0, 0);
	#else // POSIX
	const int r = ::recv(sock, buffer, len, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		s_error = WSAGetLastError();
		#else // POSIX
		s_error = errno;
		#endif
		
		// programming errors
		#ifdef WSA_SOCKETS
		assert(s_error != WSANOTINITIALISED);
		assert(s_error != WSAEFAULT);
		#else // POSIX
		assert(s_error != EBADF);
		assert(s_error != EFAULT);
		assert(s_error != EINVAL);
		assert(s_error != ENOTCONN);
		assert(s_error != ENOTSOCK);
		#endif
		
		switch (s_error)
		{
		case EAGAIN:
		case EINTR:
			break;
		#ifdef WSA_SOCKETS
		case WSAEDISCON:
		case WSAESHUTDOWN:
		case WSAENOBUFS: // Out of buffers
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSA_IO_PENDING: // Operation will be completed later
			break;
		case WSAEWOULDBLOCK:
			// Would block, or can't complete the request currently.
			break;
		case WSAECONNRESET:
		case WSAECONNREFUSED:
			break;
		case WSAENOMEM:
			break;
		#else // POSIX
		case ECONNRESET:
		case ECONNREFUSED:
			break;
		case ENOMEM:
			break;
		#endif
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::recv() - unknown error: " << s_error << std::endl;
			#endif // NDEBUG
			assert(s_error);
			break;
		}
		
		#ifdef WSA_SOCKETS
		return r;
		#endif
	}
	
	#ifdef WSA_SOCKETS
	return rb;
	#else
	return r;
	#endif
}

#if defined(WITH_SENDFILE) or defined(HAVE_XPWSA)
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes) throw()
{
	#if defined(DEBUG_SOCKETS) and !defined(NDEBUG)
	std::cout << "Socket::sendfile()" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	assert(offset >= 0);
	
	// call the real sendfile()
	#ifdef HAVE_XPWSA
	const int r = TransmitFile(sock, fd, nbytes, 0, 0, 0, TF_WRITE_BEHIND);
	#else // non-windows
	const int r = ::sendfile(fd, sock, offset, nbytes, 0, sbytes, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		s_error = WSAGetLastError();
		#else
		s_error = errno;
		#endif // HAVE_WSA
		
		// programming errors
		assert(s_error != ENOTSOCK);
		assert(s_error != EBADF);
		assert(s_error != EINVAL);
		assert(s_error != EFAULT);
		assert(s_error != ENOTCONN);
		
		switch (s_error)
		{
		case EAGAIN:
			// retry
			break;
		case EPIPE:
		case EIO: // should be handled by the caller
			break;
		default:
			std::cerr << "Socket::sendfile() - unknown error: " << s_error << std::endl;
			exit(1);
		}
	}
	
	return 0;
}
#endif // WITH_SENDFILE

std::string Socket::address() const throw()
{
	// create temporary string array for address
	#ifdef IPV6_SUPPORT
	char straddr[INET6_ADDRSTRLEN+1];
	straddr[INET6_ADDRSTRLEN] = '\0';
	#else // IPv4
	char straddr[INET_ADDRSTRLEN+1];
	straddr[INET_ADDRSTRLEN] = '\0';
	#endif // IPV6_SUPPORT
	
	// convert address to string
	
	#ifdef HAVE_WSA
	
	#ifdef IPV6_SUPPORT
	DWORD len = INET6_ADDRSTRLEN;
	#else // IPv4
	DWORD len = INET_ADDRSTRLEN;
	#endif // IPV6_SUPPORT
	
	sockaddr sa;
	memcpy(&sa, &addr, sizeof(addr));
	WSAAddressToString(&sa, sizeof(addr), 0, straddr, &len);
	
	return std::string(straddr);
	
	#else // POSIX
	
	#ifdef IPV6_SUPPORT
	inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
	#else // IPv4
	inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
	#endif // IPV6_SUPPORT
	
	std::string str(straddr);
	
	char buf[7];
	sprintf(buf, ":%d", port());
	str.insert(str.length(), buf);
	
	return str;
	
	#endif
}

uint16_t Socket::port() const throw()
{
	#ifdef IPV6_SUPPORT
	uint16_t _port =  addr.sin6_port;
	#else // IPv4
	uint16_t _port =  addr.sin_port;
	#endif
	
	return bswap(_port);
}

bool Socket::matchAddress(Socket* tsock) throw()
{
	#ifdef IPV6_SUPPORT
	// TODO: Similar checking for IPv6 addresses
	return false;
	#else // IPv4
	return (addr.sin_addr.s_addr == tsock->getAddr()->sin_addr.s_addr);
	#endif
}

bool Socket::matchPort(const Socket* tsock) const throw()
{
	return (port() == tsock->port());
}

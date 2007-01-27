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

bool Socket::create() throw()
{
	#ifdef HAVE_WSA
	#ifdef IPV6_SUPPORT
	sock = WSASocket(AF_INET6, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
	#else // No IPv6
	sock = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);
	#endif // IPv6
	#else // No WSA
	#ifdef IPV6_SUPPORT
	sock = socket(AF_INET6, SOCK_STREAM, 0);
	#else
	sock = socket(AF_INET, SOCK_STREAM, 0);
	#endif // IPv6
	#endif // HAVE_WSA
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else // No WSA
		error = errno;
		#endif // HAVE_WSA
		
		switch (error)
		{
		#ifdef HAVE_WSA
		case WSAENETDOWN:
			std::cerr << "! network subsystem failed" << std::endl;
			break;
		#ifndef NDEBUG
		case WSAEAFNOSUPPORT:
			std::cerr << "? address family not supported" << std::endl;
			assert(!(error == WSAEAFNOSUPPORT));
			break;
		case WSAEPROTONOSUPPORT:
			std::cerr << "? protocol not supported" << std::endl;
			assert(!(error == WSAEPROTONOSUPPORT));
			break;
		case WSAEPROTOTYPE:
			std::cerr << "? Wrong protocol type for socket" << std::endl;
			assert(!(error == WSAEPROTOTYPE));
			break;
		case WSAESOCKTNOSUPPORT:
			std::cerr << "? socket type not support for this address family" << std::endl;
			assert(!(error == WSAESOCKTNOSUPPORT));
			break;
		case WSAEINVAL:
			std::cerr << "? invalid parameter" << std::endl;
			assert(!(error == WSAEINVAL));
			break;
		#endif // NDEBUG
		case WSAEINPROGRESS:
			#ifndef NDEBUG
			std::cerr << "& in progress" << std::endl;
			#endif
			break;
		case WSAEMFILE:
			std::cerr << "! Socket limit reached" << std::endl;
			break;
		case WSAENOBUFS:
			std::cerr << "! Out of buffers" << std::endl;
			break;
		#endif // HAVE_WSA
		// TODO: Non-WSA errors
		default:
			std::cerr << "! Socket::create() - unknown error: " << error << std::endl;
			break;
		}
	}
	
	return (sock != INVALID_SOCKET);
}

void Socket::close() throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::close()" << std::endl;
	#endif
	#endif
	
	#ifdef WIN32
	#ifdef HAVE_XPWSA
	DisconnectEx(sock, /* overlap */ 0, TF_REUSE_SOCKET, 0);
	#else // No XPWSA
	closesocket(sock);
	#endif // HAVE_XPWSA
	#else // No WIN32
	::close(sock);
	#endif // WIN32
	
	sock = INVALID_SOCKET;
}

Socket* Socket::accept() throw(std::bad_alloc)
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::accept()" << std::endl;
	#endif
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef IPV6_SUPPORT
	sockaddr_in6 sa; // temporary
	#else
	sockaddr_in sa; // temporary
	#endif
	
	#ifndef WIN32
	socklen_t tmp
	#else
	int tmp
	#endif
		= sizeof(sa);
	
	#ifdef HAVE_WSA
	fd_t n_fd = WSAAccept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp, 0, 0);
	#else // No WSA
	fd_t n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp);
	#endif
	
	if (n_fd != INVALID_SOCKET)
	{
		#ifdef DEBUG_SOCKETS
		#ifndef NDEBUG
		std::cout << "New connection" << std::endl;
		#endif
		#endif
		
		Socket *s = new Socket(n_fd, sa);
		memcpy(s->getAddr(), &sa, sizeof(addr));
		
		return s;
	}
	else
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else // No WSA
		error = errno;
		#endif // HAVE_WSA
		
		switch (error)
		{
		case EAGAIN:
			// retry
			break;
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(!(error == EBADF));
			break;
		case EINVAL:
			std::cerr << "listen() was not called for this socket." << std::endl;
			assert(!(error == EINVAL));
			break;
		case EFAULT:
			std::cerr << "Addr not writable" << std::endl;
			assert(!(error == EFAULT));
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(!(error == ENOTSOCK));
			break;
		case EOPNOTSUPP:
			std::cerr << "Not of type, SOCK_STREAM." << std::endl;
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif // NDEBUG
			break;
		case EMFILE:
			std::cerr << "Per-process open FD limit reached." << std::endl;
			break;
		case ENFILE:
			std::cerr << "System open FD limit reached." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory." << std::endl;
			break;
		case EPERM:
			#ifndef NDEBUG
			std::cerr << "Firewall denied connection" << std::endl;
			#endif // NDEBUG
			break;
		case ECONNABORTED:
			#ifndef NDEBUG
			std::cerr << "Connection aborted" << std::endl;
			#endif // NDEBUG
			break;
		case ENOBUFS:
			std::cerr << "Out of network buffers" << std::endl;
			break;
		#ifndef WIN32
		case EPROTO:
			// whatever this is?
			#ifndef NDEBUG
			std::cerr << "Protocol error." << std::endl;
			#endif // NDEBUG
			break;
		#endif // !WIN32
		#ifdef LINUX
		// no idea what these are mostly.
		case ENOSR:
			#ifndef NDEBUG
			std::cerr << "ENOSR" << std::endl;
			#endif // NDEBUG
			break;
		case ESOCKTNOSUPPORT:
			#ifndef NDEBUG
			std::cerr << "ESOCKTNOSUPPORT" << std::endl;
			#endif // NDEBUG
			break;
		case EPROTONOSUPPORT:
			#ifndef NDEBUG
			std::cerr << "EPROTONOSUPPORT" << std::endl;
			#endif // NDEBUG
			break;
		case ETIMEDOUT:
			#ifndef NDEBUG
			std::cerr << "ETIMEDOUT" << std::endl;
			#endif // NDEBUG
			break;
		case ERESTARTSYS:
			#ifndef NDEBUG
			std::cerr << "ERESTARTSYS" << std::endl;
			#endif // NDEBUG
			break;
		#endif // LINUX
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::accept() - unknown error: " << error << std::endl;
			#endif // NDEBUG
			break;
		}
		
		return 0;
	}
}

bool Socket::block(bool x) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::block(" << (x?"true":"false") << ")" << std::endl;
	#endif // NDEBUG
	#endif // DEBUG_SOCKETS
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	uint32_t arg = !x;
	#ifdef HAVE_WSA
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else
	return ioctlsocket(sock, FIONBIO, &arg);
	#endif // HAVE_WSA
	#else // Non-Win32
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif // WIN32
}

bool Socket::reuse(bool x) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::reuse(" << (x?"true":"false") << ")" << std::endl;
	#endif
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifndef SO_REUSEPORT
	// Windows (for example) does not have it
	return (x==true);
	#else
	int val = (x ? 1 : 0);
	
	int r = (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) == 0);
	
	if (r == SOCKET_ERROR)
	{
		error = errno;
		
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Bad FD" << std::endl;
			assert(!(error == EBADF));
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(!(error == ENOTSOCK));
			break;
		case ENOPROTOOPT:
			std::cerr << "The option is unknown at the level SOL_SOCKET." << std::endl;
			assert(!(error == ENOPROTOOPT));
			break;
		case EFAULT:
			std::cerr << "&val in invalid address space." << std::endl;
			assert(!(error == EFAULT));
			break;
		#endif // NDEBUG
		default:
			std::cerr << "unknown error from setsockopt() : " << error << std::endl;
			break;
		}
	}
	return (r == 0);
	#endif
}

int Socket::bindTo(std::string address, uint16_t port) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::bindTo([" << address << "], " << port << ")" << std::endl;
	#endif // NDEBUG
	#endif // DEBUG_SOCKETS
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef HAVE_WSA
	int size = sizeof(addr);
	#ifdef IPV6_SUPPORT
	char straddr[INET6_ADDRSTRLEN+1];
	#else // IPv6
	char straddr[14+1];
	#endif // IPV6_SUPPORT
	memcpy(straddr, address.c_str(), address.length());
	WSAStringToAddress(
		straddr,
		#ifdef IPV6_SUPPORT
		AF_INET6,
		#else // IPv4
		AF_INET,
		#endif // IPV6_SUPPORT
		0,
		reinterpret_cast<sockaddr*>(&addr),
		&size
	);
	#else // No WSA
	inet_pton(
		#ifdef IPV6_SUPPORT
		AF_INET6,
		#else // IPv4
		AF_INET,
		#endif // IPV6_SUPPORT
		address.c_str(),
		#ifdef IPV6_SUPPORT
		&(addr.sin6_addr)
		#else // IPv4
		&(addr.sin_addr)
		#endif // IPV6_SUPPORT
	);
	#endif // HAVE_WSA
	
	#ifdef IPV6_SUPPORT
	addr.sin6_family = AF_INET6;
	addr.sin6_port = bswap(port);
	#else // IPv4
	addr.sin_family = AF_INET;
	addr.sin_port = bswap(port);
	#endif // IPV6_SUPPORT
	
	int r = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		error = errno;
		#endif // HAVE_WSA
		
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(!(error == EBADF));
			break;
		case EINVAL:
			// According to docs, this may change in the future.
			std::cerr << "Socket already bound" << std::endl;
			assert(!(error == EINVAL));
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(!(error == ENOTSOCK));
			break;
		case EOPNOTSUPP:
			std::cerr << "does not support bind" << std::endl;
			assert(!(error == EOPNOTSUPP));
			break;
		case EAFNOSUPPORT:
			std::cerr << "invalid address family" << std::endl;
			assert(!(error == EAFNOSUPPORT));
			break;
		case EISCONN:
			std::cerr << "already connected" << std::endl;
			assert(!(error == EISCONN));
			break;
		#endif
		case EADDRINUSE:
			std::cerr << "address already in use" << std::endl;
			break;
		case EADDRNOTAVAIL:
			std::cerr << "address not available" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "insufficient resources" << std::endl;
			break;
		case EACCES:
			std::cerr << "can't bind to superuser sockets" << std::endl;
			break;
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::bindTo() - unknown error: " << error << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

#ifdef IPV6_SUPPORT
int Socket::connect(sockaddr_in6* rhost) throw()
#else
int Socket::connect(sockaddr_in* rhost) throw()
#endif
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::connect()" << std::endl;
	#endif
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef HAVE_WSA
	int r = WSAConnect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost), 0, 0, 0, 0);
	#else
	int r = ::connect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost));
	#endif
	
	if (r == -1)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		error = errno;
		#endif
		
		switch (error)
		{
		case EINPROGRESS:
			#ifndef NDEBUG
			std::cout << "Connection in progress." << std::endl;
			#endif
			return 0;
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
		case EFAULT:
			assert(!(error == EFAULT));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EISCONN:
			assert(!(error == EISCONN));
		case EADDRINUSE:
			assert(!(error == EADDRINUSE));
		case EAFNOSUPPORT:
			assert(!(error == EAFNOSUPPORT));
		case EALREADY: // already connected
			assert(!(error == EALREADY));
			break;
		#endif // NDEBUG
		case EACCES:
		case EPERM:
			std::cerr << "Firewall denied connection" << std::endl;
			return -1;
		case ECONNREFUSED:
			std::cerr << "Connection refused" << std::endl;
			return 1;
			break;
		case ETIMEDOUT:
			std::cerr << "Connection timed-out" << std::endl;
			return 2;
			break;
		case ENETUNREACH:
			std::cerr << "Network unreachable" << std::endl;
			return 2;
			break;
		case EAGAIN:
			// retry
			return 2;
		}
	}
	
	return 0;
}

int Socket::listen() throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::listen()" << std::endl;
	#endif
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	int r = ::listen(sock, 4);
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		error = errno;
		#endif
		
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
			break;
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
			break;
		case EOPNOTSUPP:
			std::cerr << "Does not support listen." << std::endl;
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::listen() - unknown error: " << error << std::endl;
			#endif // NDEBUG
			break;
		}
	}
	
	return r;
}

int Socket::send(char* buffer, size_t len) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::send(*buffer, " << len << ")" << std::endl;
	#endif
	#endif
	
	assert(buffer != 0);
	assert(len > 0);
	assert(sock != INVALID_SOCKET);
	
	#ifdef HAVE_WSA
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD sb;
	int r = WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	if (r != SOCKET_ERROR) r = sb;
	#else
	int r = ::send(sock, buffer, len, MSG_NOSIGNAL);
	#endif // HAVE_WSA
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		error = errno;
		#endif
		
		switch (error)
		{
		#ifdef HAVE_WSA
		#ifndef NDEBUG
		case WSAEFAULT:
			// invalid address for
			// lpBuffers, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine
			std::cerr << "Invalid address given" << std::endl;
			assert(!(error == WSAEFAULT));
			break;
		case WSAEINVAL:
			std::cerr << "Socket not overlapped" << std::endl;
			assert(!(error == WSAEINVAL));
			break;
		case WSANOTINITIALISED:
			assert(!(error == WSANOTINITIALISED));
			break;
		#endif // NDEBUG
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSAENETDOWN: // Network sub-system failure
			break;
		case WSAEWOULDBLOCK:
			// Would block, or can't complete the request currently.
			return SOCKET_ERROR - 1;
		#endif
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
			break;
		case EFAULT:
			std::cerr << "invalid buffer address" << std::endl;
			assert(!(error == EFAULT));
			break;
		case EINVAL:
			std::cerr << "Invalid argument" << std::endl;
			assert(!(error == EINVAL));
			break;
		case ENOTCONN:
			assert(!(error == ENOTCONN));
			break;
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
			break;
		case EOPNOTSUPP:
			std::cerr << "Invalid flags" << std::endl;
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		case EAGAIN:
		case EINTR:
		case ENOMEM:
			return SOCKET_ERROR - 1;
			break;
		case EPIPE:
		case ECONNRESET:
			close();
			break;
		case ENOBUFS:
			#ifndef NDEBUG
			std::cerr << "Out of network buffers" << std::endl;
			#endif // NDEBUG
			return SOCKET_ERROR - 1;
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::send() - unknown error: " << error << std::endl;
			#endif // NDEBUG
			break;
		}
	}
	
	return r;
}

int Socket::recv(char* buffer, size_t len) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::recv(*buffer, " << len << ")" << std::endl;
	#endif
	#endif
	
	assert(sock != INVALID_SOCKET);
	assert(buffer != 0);
	assert(len > 0);
	
	// WSA causes WSAEFAULT error to occur for some reason
	
	/*
	#ifdef HAVE_WSA
	WSABUF wbuf;
	wbuf.buf = buffer;
	wbuf.len = len;
	DWORD rb;
	int r = WSARecv(sock, &wbuf, 1, &rb, 0, 0, 0);
	if (r != SOCKET_ERROR) r = rb;
	#else
	*/
	int r = ::recv(sock, buffer, len, 0);
	/*
	#endif // HAVE_WSA
	*/
	
	if (r == SOCKET_ERROR)
	{
		/*
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		*/
		error = errno;
		//#endif // HAVE_WSA
		
		switch (error)
		{
		#ifdef HAVE_WSA
		#ifndef NDEBUG
		case WSANOTINITIALISED:
			assert(!(error == WSANOTINITIALISED));
			break;
		#endif // NDEBUG
		case WSAEFAULT:
			std::cerr << "WSABUF in invalid address space" << std::endl;
			assert(!(error == WSAEFAULT));
			break;
		case WSAEDISCON:
			close();
		case WSAESHUTDOWN:
		case WSAENOBUFS: // Out of buffers
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		case WSA_IO_PENDING: // Operation will be completed later
			error = 0;
			break;
		#endif
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
			break;
		case EFAULT:
			std::cerr << "Buffer points to invalid address" << std::endl;
			assert(!(error == EFAULT));
			break;
		case EINVAL:
			std::cerr << "Invalid argument." << std::endl;
			assert(!(error == EINVAL));
			break;
		case ENOTCONN:
			assert(!(error == ENOTCONN));
			break;
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
			break;
		#endif // NDEBUG
		case ECONNRESET:
			close();
			break;
		case ECONNREFUSED:
			#ifndef NDEBUG
			std::cerr << "Connection refused" << std::endl;
			#endif
			break;
		case EAGAIN:
		case EINTR:
		case ENOMEM:
			return SOCKET_ERROR - 1;
		default:
			// Should not happen
			#ifndef NDEBUG
			std::cerr << "Socket::recv() - unknown error: " << error << std::endl;
			#endif // NDEBUG
			break;
		}
	}
	
	return r;
}

#if defined(WITH_SENDFILE) || defined(HAVE_XPWSA)
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes) throw()
{
	#ifdef DEBUG_SOCKETS
	#ifndef NDEBUG
	std::cout << "Socket::sendfile()" << std::endl;
	#endif // NDEBUG
	#endif
	
	assert(fd != INVALID_SOCKET);
	assert(offset >= 0);
	
	// call the real sendfile()
	#ifdef HAVE_XPWSA
	int r = TransmitFile(sock, fd, nbytes, 0, 0, 0, TF_WRITE_BEHIND);
	#else // non-windows
	int r = ::sendfile(fd, sock, offset, nbytes, 0, sbytes, 0);
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef HAVE_WSA
		error = WSAGetLastError();
		#else
		error = errno;
		#endif // HAVE_WSA
		
		switch (error)
		{
		#ifndef NDEBUG
		case ENOTSOCK:
			std::cerr << "Socket::sendfile() - FD is not a socket" << std::endl;
			assert(1);
			break;
		case EBADF:
		case EINVAL:
			std::cerr << "Socket::sendfile() - invalid FD" << std::endl;
			assert(1);
			break;
		case ENOTCONN:
			std::cerr << "Socket::sendfile() - not connected" << std::endl;
			assert(1);
			break;
		#endif // NDEBUG
		case EPIPE:
			#ifndef NDEBUG
			std::cerr << "Socket::sendfile() - connection closed" << std::endl;
			#endif // NDEBUG
			return -1;
		case EIO:
			#ifndef NDEBUG
			std::cerr << "Socket::sendfile() - error while reading FD" << std::endl;
			#endif // NDEBUG
			return -1;
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "Socket::sendfile() - invalid sbytes address" << std::endl;
			assert(1);
			#endif // NDEBUG
			break;
		case EAGAIN:
			// retry
			return 0;
			break;
		default:
			return -1;
		}
	}
	
	return 0;
}
#endif // WITH_SENDFILE

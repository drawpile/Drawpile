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

fd_t Socket::create() throw()
{
	#ifdef WSA_SOCKETS
	
	sock = WSASocket(
	#ifdef IPV6_SUPPORT
		AF_INET6,
	#else // No IPv6
		AF_INET,
	#endif // IPv6
		SOCK_STREAM,
		0,
		0,
		0,
		WSA_FLAG_OVERLAPPED
	);
	
	#else // No WSA
	
	sock = socket(
	#ifdef IPV6_SUPPORT
		AF_INET6,
	#else
		AF_INET,
	#endif // IPv6
		SOCK_STREAM,
		IPPROTO_TCP
	);
	
	#endif // WSA_SOCKETS
	
	if (sock == INVALID_SOCKET)
	{
		#ifdef WSA_SOCKETS
		error = WSAGetLastError();
		#else // No WSA
		error = errno;
		#endif // WSA_SOCKETS
		
		switch (error)
		{
		#ifdef WSA_SOCKETS
		case WSAEINPROGRESS:
			break;
		case WSAENETDOWN:
			std::cerr << "! Network sub-system failure" << std::endl;
			break;
		#ifndef NDEBUG
		case WSAEAFNOSUPPORT:
			assert(!(error == WSAEAFNOSUPPORT));
		case WSAEPROTONOSUPPORT:
			assert(!(error == WSAEPROTONOSUPPORT));
		case WSAEPROTOTYPE:
			assert(!(error == WSAEPROTOTYPE));
		case WSAESOCKTNOSUPPORT:
			assert(!(error == WSAESOCKTNOSUPPORT));
		case WSAEINVAL:
			assert(!(error == WSAEINVAL));
			break;
		#endif // NDEBUG
		case WSAEMFILE:
			std::cerr << "! Socket limit reached" << std::endl;
			break;
		case WSAENOBUFS:
			std::cerr << "! Out of buffers" << std::endl;
			break;
		#endif // WSA_SOCKETS
		// TODO: Non-WSA errors
		default:
			std::cerr << "! Socket::create() - unhandled error: " << error << std::endl;
			assert(error);
			break;
		}
	}
	
	return sock;
}

void Socket::close() throw()
{
	#if defined(HAVE_XPWSA)
	DisconnectEx(sock, 0, TF_REUSE_SOCKET, 0);
	#elif defined(HAVE_WSA)
	closesocket(sock);
	#else // Not Win32
	::close(sock);
	#endif
	
	sock = INVALID_SOCKET;
}

Socket* Socket::accept() throw(std::bad_alloc)
{
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
	
	#ifdef WSA_SOCKETS
	fd_t n_fd = WSAAccept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp, 0, 0);
	#else // No WSA
	fd_t n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp);
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
		error = WSAGetLastError();
		#else // No WSA
		error = errno;
		#endif // WSA_SOCKETS
		
		switch (error)
		{
		case EINTR:
		case EAGAIN:
			// retry
			break;
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
		case EINVAL:
			assert(!(error == EINVAL));
		case EFAULT:
			assert(!(error == EFAULT));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EOPNOTSUPP:
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		case EMFILE:
			std::cerr << "! Process FD limit reached" << std::endl;
			break;
		case ENFILE:
			std::cerr << "! System FD limit reached" << std::endl;
			break;
		case ENOMEM:
			std::cerr << "! Out of memory" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "! Out of network buffers" << std::endl;
			break;
		case EPERM:
			std::cerr << "! Firewall blocked incoming connection" << std::endl;
			break;
		case ECONNABORTED:
			std::cerr << "! Incoming connection aborted" << std::endl;
			break;
		#ifndef WIN32
		case EPROTO:
			assert(error);
			break;
		#endif // !WIN32
		#ifdef LINUX
		// no idea what these are mostly.
		case ENOSR:
		case ESOCKTNOSUPPORT:
		case EPROTONOSUPPORT:
		case ETIMEDOUT:
		case ERESTARTSYS:
			assert(error);
			break;
		#endif // LINUX
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::accept() - unhandled error: " << error << std::endl;
			#endif // NDEBUG
			assert(error);
			break;
		}
		
		return 0;
	}
}

bool Socket::block(bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::block(fd: " << sock << ", " << (x?"true":"false") << ")" << std::endl;
	#endif // NDEBUG
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WIN32
	#ifdef WSA_SOCKETS
	uint32_t arg = (x ? 1 : 0);
	return (WSAIoctl(sock, FIONBIO, &arg, sizeof(arg), 0, 0, 0, 0, 0) == 0);
	#else
	u_long arg = (x ? 1 : 0);
	return ioctlsocket(sock, FIONBIO, &arg);
	#endif // WSA_SOCKETS
	#else // Not Win32
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif // WIN32
}

bool Socket::reuse(bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::reuse(fd: " << sock << ", " << (x?"true":"false") << ")" << std::endl;
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
			assert(!(error == EBADF));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case ENOPROTOOPT:
			assert(!(error == ENOPROTOOPT));
		case EFAULT:
			assert(!(error == EFAULT));
			break;
		#endif // NDEBUG
		default:
			std::cerr << "unhandled error from setsockopt() : " << error << std::endl;
			assert(error);
			break;
		}
	}
	return (r == 0);
	#endif
}

int Socket::bindTo(std::string address, uint16_t port) throw()
{
	#ifdef DEBUG_SOCKETS
	std::cout << "Socket::bindTo([" << address << "], " << port << ")" << std::endl;
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
			assert(!(error == EBADF));
			break;
		case EINVAL:
			// According to docs, this may change in the future.
			assert(!(error == EINVAL));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EOPNOTSUPP:
			assert(!(error == EOPNOTSUPP));
		case EAFNOSUPPORT:
			assert(!(error == EAFNOSUPPORT));
		case EISCONN:
			assert(!(error == EISCONN));
			break;
		#endif
		case EADDRINUSE:
			std::cerr << "! Address already in use" << std::endl;
			break;
		case EADDRNOTAVAIL:
			std::cerr << "! Address not available" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "! Out of network buffers" << std::endl;
			break;
		case EACCES:
			std::cerr << "! Can't bind to super-user sockets" << std::endl;
			break;
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::bindTo() - unhandled error: " << error << std::endl;
			#endif // NDEBUG
			assert(error);
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
	std::cout << "Socket::connect()" << std::endl;
	#endif
	
	assert(sock != INVALID_SOCKET);
	
	#ifdef WSA_SOCKETS
	int r = WSAConnect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost), 0, 0, 0, 0);
	#else
	int r = ::connect(sock, reinterpret_cast<sockaddr*>(&rhost), sizeof(rhost));
	#endif
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		error = WSAGetLastError();
		#else
		error = errno;
		#endif
		
		switch (error)
		{
		case EINPROGRESS:
			break;
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
			std::cerr << "! Firewall denied connection" << std::endl;
			break;
		case ECONNREFUSED:
			std::cerr << "! Connection refused" << std::endl;
			break;
		case ETIMEDOUT:
			std::cerr << "! Connection timed-out" << std::endl;
			break;
		case ENETUNREACH:
			std::cerr << "! Network unreachable" << std::endl;
			break;
		case EAGAIN:
			// retry
			return 2;
		}
	}
	
	return r;
}

int Socket::listen() throw()
{
	#ifdef DEBUG_SOCKETS
	std::cout << "Socket::listen()" << std::endl;
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
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EOPNOTSUPP:
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::listen() - unhandled error: " << error << std::endl;
			#endif // NDEBUG
			assert(error);
			break;
		}
	}
	
	return r;
}

int Socket::send(char* buffer, size_t len) throw()
{
	#ifdef DEBUG_SOCKETS
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
	int r = WSASend(sock, &wbuf, 1, &sb, 0, 0, 0);
	if (r != SOCKET_ERROR) r = sb;
	#else
	int r = ::send(sock, buffer, len, MSG_NOSIGNAL);
	#endif // WSA_SOCKETS
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		error = WSAGetLastError();
		#else
		error = errno;
		#endif
		
		switch (error)
		{
		#ifdef WSA_SOCKETS
		#ifndef NDEBUG
		case WSAEFAULT:
			// invalid address for
			// lpBuffers, lpNumberOfBytesSent, lpOverlapped, lpCompletionRoutine
			assert(!(error == WSAEFAULT));
		case WSAEINVAL:
			assert(!(error == WSAEINVAL));
		case WSANOTINITIALISED:
			assert(!(error == WSANOTINITIALISED));
			break;
		#endif // NDEBUG
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_IO_PENDING: // Operation will be completed later
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
			break;
		case WSAEWOULDBLOCK:
			// Would block, or can't complete the request currently.
			break;
		#endif // WSA_SOCKETS
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
		case EFAULT:
			assert(!(error == EFAULT));
		case EINVAL:
			assert(!(error == EINVAL));
		case ENOTCONN:
			assert(!(error == ENOTCONN));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EOPNOTSUPP:
			assert(!(error == EOPNOTSUPP));
			break;
		#endif // NDEBUG
		case EAGAIN:
		case EINTR:
			break;
		case EPIPE:
		case ECONNRESET:
			break;
		case ENOMEM:
			std::cerr << "! Out of memory" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "! Out of network buffers" << std::endl;
			break;
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::send() - unhandled error: " << error << std::endl;
			#endif // NDEBUG
			assert(error);
			break;
		}
	}
	
	return r;
}

int Socket::recv(char* buffer, size_t len) throw()
{
	#ifdef DEBUG_SOCKETS
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
	int r = WSARecv(sock, &wbuf, 1, &rb, &flags, 0, 0);
	if (r != SOCKET_ERROR) r = rb;
	#else
	int r = ::recv(sock, buffer, len, 0);
	#endif // WSA_SOCKETS
	
	if (r == SOCKET_ERROR)
	{
		#ifdef WSA_SOCKETS
		error = WSAGetLastError();
		#else
		error = errno;
		#endif // WSA_SOCKETS
		
		switch (error)
		{
		case EAGAIN:
		case EINTR:
			break;
		#ifdef WSA_SOCKETS
		#ifndef NDEBUG
		case WSANOTINITIALISED:
			assert(!(error == WSANOTINITIALISED));
		case WSAEFAULT:
			assert(!(error == WSAEFAULT));
			break;
		#endif // NDEBUG
		case WSAEDISCON:
		case WSAESHUTDOWN:
		case WSAENOBUFS: // Out of buffers
		case WSAENETDOWN: // Network sub-system failure
		case WSAENETRESET: // Keep-alive reset
		case WSAECONNABORTED: // Connection timed-out
		case WSA_OPERATION_ABORTED: // Overlapped operation aborted
		case WSA_IO_PENDING: // Operation will be completed later
			break;
		#endif // WSA_SOCKETS
		#ifndef NDEBUG
		case EBADF:
			assert(!(error == EBADF));
		case EFAULT:
			assert(!(error == EFAULT));
		case EINVAL:
			assert(!(error == EINVAL));
		case ENOTCONN:
			assert(!(error == ENOTCONN));
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
			break;
		#endif // NDEBUG
		case ECONNRESET:
		case ECONNREFUSED:
			break;
		case ENOMEM:
			std::cerr << "! Out of memory" << std::endl;
			break;
		default:
			#ifndef NDEBUG
			std::cerr << "Socket::recv() - unhandled error: " << error << std::endl;
			#endif // NDEBUG
			assert(error);
			break;
		}
	}
	
	return r;
}

#if defined(WITH_SENDFILE) || defined(HAVE_XPWSA)
int Socket::sendfile(fd_t fd, off_t offset, size_t nbytes, off_t *sbytes) throw()
{
	#ifdef DEBUG_SOCKETS
	std::cout << "Socket::sendfile()" << std::endl;
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
		case EAGAIN:
			// retry
			break;
		#ifndef NDEBUG
		case ENOTSOCK:
			assert(!(error == ENOTSOCK));
		case EBADF:
			assert(!(error == EBADF));
		case EINVAL:
			assert(!(error == EINVAL));
		case EFAULT:
			assert(!(error == EFAULT))
		case ENOTCONN:
			assert(!(error == ENOTCONN));
			break;
		#endif // NDEBUG
		case EPIPE:
			// broken pipe
			break;
		case EIO:
			std::cerr << "Socket::sendfile() - error while reading file." << std::endl;
			break;
		default:
			std::cerr << "Socket::sendfile() - unhadled error: " << error << std::endl;
			break;
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
	
	inet_ntop(
	#ifdef IPV6_SUPPORT
		AF_INET6,
		&addr.sin6_addr,
	#else // IPv4
		AF_INET,
		&addr.sin_addr,
	#endif // IPV6_SUPPORT
		straddr,
		sizeof(straddr)
	);
	
	std::string str(straddr);
	
	char buf[7];
	sprintf(buf, ":%d", port());
	str.insert(str.length(), buf);
	
	return str;
	
	#endif // WSA/POSIX
}

uint16_t Socket::port() const throw()
{
	uint16_t _port = 
	#ifdef IPV6_SUPPORT
		addr.sin6_port;
	#else // IPv4
		addr.sin_port;
	#endif // IPV6_SUPPORT
	
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

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

#include "sockets.h"

#ifndef NDEBUG
#include <iostream>
#endif

#include <fcntl.h>
#include <cassert>

void Socket::close() throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::close()" << std::endl;
	#endif

	#ifdef WIN32
	closesocket(sock);
	#else
	::close(sock);
	#endif
	
	sock = INVALID_SOCKET;
}

Socket* Socket::accept() throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Socket::accept()" << std::endl;
	#endif
	
	sockaddr_in sa; // temporary
	#ifndef WIN32
	socklen_t tmp = sizeof(sockaddr);
	#else
	int tmp = sizeof(sockaddr);
	#endif
	int n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp);
	error = errno;
	
	if (n_fd != -1)
	{
		#ifndef NDEBUG
		std::cout << "New connection" << std::endl;
		#endif
		
		Socket *s = new Socket();
		memcpy(reinterpret_cast<sockaddr*>(s->getAddr()), &sa, sizeof(sockaddr));
		
		s->fd(n_fd);
		
		return s;
	}
	else
	{
		switch (error)
		{
		#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
		#else
		case EAGAIN:
		#endif
			#ifndef NDEBUG
			std::cerr << "Would block, try again." << std::endl;
			#endif
			break;
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Invalid FD" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			break;
		#if defined( TRAP_CODER_ERROR )
		case EINVAL:
			#ifndef NDEBUG
			std::cerr << "Not listening." << std::endl;
			#endif
			break;
		#endif // TRAP_CODER_ERROR
		case EMFILE:
			#ifndef NDEBUG
			std::cerr << "Per-process open FD limit reached." << std::endl;
			#endif
			break;
		case ENFILE:
			#ifndef NDEBUG
			std::cerr << "System open FD limit reached." << std::endl;
			#endif
			break;
		#if defined( TRAP_CODER_ERROR )
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "Addr not writable" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case ENOMEM:
			#ifndef NDEBUG
			std::cerr << "Out of memory." << std::endl;
			#endif
			break;
		case EPERM:
			#ifndef NDEBUG
			std::cerr << "Firewall forbids." << std::endl;
			#endif
			break;
		#ifndef WIN32
		#if defined( TRAP_CODER_ERROR )
		case ENOTSOCK:
			#ifndef NDEBUG
			std::cerr << "Not a socket." << std::endl;
			#endif
			assert(1);
			break;
		case EOPNOTSUPP:
			#ifndef NDEBUG
			std::cerr << "Not of type, SOCK_STREAM." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case ECONNABORTED:
			#ifndef NDEBUG
			std::cerr << "Connection aborted" << std::endl;
			#endif
			break;
		case ENOBUFS:
			#ifndef NDEBUG
			std::cerr << "Out of network buffers" << std::endl;
			#endif
			break;
		case EPROTO:
			// whatever this is?
			#ifndef NDEBUG
			std::cerr << "Protocol error." << std::endl;
			#endif
			break;
		#endif // !WIN32
		#ifdef LINUX
		// no idea what these are mostly.
		case ENOSR:
			#ifndef NDEBUG
			std::cerr << "ENOSR" << std::endl;
			#endif
			break;
		case ESOCKTNOSUPPORT:
			#ifndef NDEBUG
			std::cerr << "ESOCKTNOSUPPORT" << std::endl;
			#endif
			break;
		case EPROTONOSUPPORT:
			#ifndef NDEBUG
			std::cerr << "EPROTONOSUPPORT" << std::endl;
			#endif
			break;
		case ETIMEDOUT:
			#ifndef NDEBUG
			std::cerr << "ETIMEDOUT" << std::endl;
			#endif
			break;
		case ERESTARTSYS:
			#ifndef NDEBUG
			std::cerr << "ERESTARTSYS" << std::endl;
			#endif
			break;
		#endif // LINUX
		default:
			#ifndef NDEBUG
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
		
		return 0;
	}
}

int Socket::block(bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::block(" << (x?"true":"false") << ")" << std::endl;
	#endif
	
	#ifdef WIN32
	unsigned long arg = !x;
	return ioctlsocket(sock, FIONBIO, &arg);
	#else
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? SOCKET_ERROR : 0;
	#endif
}

int Socket::bindTo(uint32_t address, uint16_t port) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::bindTo(" << address << ", " << port << ")" << std::endl;
	#endif

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = htons(port);
	
	int r = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	error = errno;
	
	if (r == -1)
	{
		switch (error)
		{
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Invalid FD" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EINVAL:
			// According to docks, this may change in the future.
			#ifndef NDEBUG
			std::cerr << "Socket already bound" << std::endl;
			#endif
			assert(1);
			break;
		case EACCES:
			#ifndef NDEBUG
			std::cerr << "Can't bind to super-user port." << std::endl;
			#endif
			assert(1);
			break;
		#ifndef WIN32
		#if defined( TRAP_CODER_ERROR )
		case ENOTSOCK:
			#ifndef NDEBUG
			std::cerr << "Not a socket." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		#endif // !WIN32
		default:
			#ifndef NDEBUG
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

int Socket::listen() throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::listen()" << std::endl;
	#endif

	int r = ::listen(sock, 4);
	error = errno;
	
	if (r == -1)
	{
		switch (error)
		{
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Invalid FD" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		#ifndef WIN32
		#if defined( TRAP_CODER_ERROR )
		case ENOTSOCK:
			#ifndef NDEBUG
			std::cerr << "Not a socket." << std::endl;
			#endif
			assert(1);
			break;
		case EOPNOTSUPP:
			#ifndef NDEBUG
			std::cerr << "Does not support listen." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		#endif // !WIN32
		default:
			#ifndef NDEBUG
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

int Socket::send(char* buffer, size_t buflen) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::send(*buffer, " << buflen << ")" << std::endl;
	#endif
	
	assert(buffer != 0);
	assert(buflen != 0);
	
	int r = ::send(sock, buffer, buflen, MSG_NOSIGNAL);
	error = errno;
	
	if (r == -1)
	{
		switch (error)
		{
		#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
		#else
		case EAGAIN:
		#endif
			#ifndef NDEBUG
			std::cerr << "Would block, try again" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Invalid fD" << std::endl;
			#endif
			assert(1);
			break;
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "Invalid parameter address." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#if defined( TRAP_CODER_ERROR )
		case EINVAL:
			#ifndef NDEBUG
			std::cerr << "Invalid argument" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case ENOMEM:
			#ifndef NDEBUG
			std::cerr << "Out of memory" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		case EPIPE:
			#ifndef NDEBUG
			std::cerr << "Pipe broken, connection closed on local end." << std::endl;
			#endif
			break;
		#ifndef WIN32
		case ECONNRESET:
			#ifndef NDEBUG
			std::cerr << "Connection reset by peer" << std::endl;
			#endif
			break;
		#if defined( TRAP_CODER_ERROR )
		case EDESTADDRREQ: // likely result of sendmsg()
			#ifndef NDEBUG
			std::cerr << "Not connceted, and no peer defined." << std::endl;
			#endif
			assert(1);
			break;
		case ENOTCONN:
			#ifndef NDEBUG
			std::cerr << "Not connected" << std::endl;
			#endif
			assert(1);
			break;
		case ENOTSOCK:
			#ifndef NDEBUG
			std::cerr << "Not a socket" << std::endl;
			#endif
			assert(1);
			break;
		case EOPNOTSUPP:
			#ifndef NDEBUG
			std::cerr << "Invalid flags" << std::endl;
			#endif
			assert(1);
			break;
		case EISCONN: // likely result of sendmsg()
			#ifndef NDEBUG
			std::cerr << "Already connected, but recipient was specified." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EMSGSIZE:
			#ifndef NDEBUG
			std::cerr << "Socket requires atomic sending." << std::endl;
			#endif
			assert(1);
			break;
		case ENOBUFS:
			#ifndef NDEBUG
			std::cerr << "Out of buffers" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#endif // !WIN32
		default:
			#ifndef NDEBUG
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

int Socket::recv(char* buffer, size_t buflen) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::recv(*buffer, " << buflen << ")" << std::endl;
	#endif
	
	assert(buffer != 0);
	assert(buflen != 0);
	
	int r = ::recv(sock, buffer, buflen, 0);
	error = errno;
	
	if (r == -1)
	{
		switch (error)
		{
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Invalid FD" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
		#else
		case EAGAIN:
		#endif
			// Should retry
			#ifndef NDEBUG
			std::cerr << "Would block, try again." << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		case EINTR:
			// Should retry
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#if defined( TRAP_CODER_ERROR )
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "Buffer points to invalid address" << std::endl;
			#endif
			assert(1);
			break;
		case EINVAL:
			#ifndef NDEBUG
			std::cerr << "Invalid argument." << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case ENOMEM:
			#ifndef NDEBUG
			std::cerr << "Out of memory" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#ifndef WIN32
		#if defined( TRAP_CODER_ERROR )
		case ECONNREFUSED:
			#ifndef NDEBUG
			std::cerr << "Connection refused" << std::endl;
			#endif
			break;
		case ENOTCONN:
			#ifndef NDEBUG
			std::cerr << "Not connected" << std::endl;
			#endif
			assert(1);
			break;
		case ENOTSOCK:
			#ifndef NDEBUG
			std::cerr << "Not a socket" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		#endif // !WIN32
		default:
			// Should not happen
			#ifndef NDEBUG
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

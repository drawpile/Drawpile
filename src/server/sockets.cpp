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
		std::cout << "New connection" << std::endl;
		
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
			std::cerr << "Would block, try again." << std::endl;
			break;
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		case EINTR:
			std::cerr << "Interrupted by signal." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Not listening." << std::endl;
			break;
		case EMFILE:
			std::cerr << "Per-process open FD limit reached." << std::endl;
			break;
		case ENFILE:
			std::cerr << "System open FD limit reached." << std::endl;
			break;
		case EFAULT:
			std::cerr << "Addr not writable" << std::endl;
			assert(1);
			break;
		case ENOMEM:
			std::cerr << "Out of memory." << std::endl;
			break;
		case EPERM:
			std::cerr << "Firewall forbids." << std::endl;
			break;
		#ifndef WIN32
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "Not of type, SOCK_STREAM." << std::endl;
			assert(1);
			break;
		case ECONNABORTED:
			std::cerr << "Connection aborted" << std::endl;
			break;
		case ENOBUFS:
			std::cerr << "Out of network buffers" << std::endl;
			break;
		case EPROTO:
			std::cerr << "Protocol error." << std::endl;
			break;
		#endif // !WIN32
		#ifdef LINUX
		case ENOSR:
			std::cerr << "ENOSR" << std::endl;
			break;
		case ESOCKTNOSUPPORT:
			std::cerr << "ESOCKTNOSUPPORT" << std::endl;
			break;
		case EPROTONOSUPPORT:
			std::cerr << "EPROTONOSUPPORT" << std::endl;
			break;
		case ETIMEDOUT:
			std::cerr << "ETIMEDOUT" << std::endl;
			break;
		case ERESTARTSYS:
			std::cerr << "ERESTARTSYS" << std::endl;
			break;
		#endif // LINUX
		default:
			std::cerr << "Unknown error." << std::endl;
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
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		case EINVAL:
			// According to docks, this may change in the future.
			std::cerr << "Socket already bound" << std::endl;
			assert(1);
			break;
		case EACCES:
			std::cerr << "Can't bind to super-user port." << std::endl;
			assert(1);
			break;
		#ifndef WIN32
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		#endif // !WIN32
		default:
			std::cerr << "Unknown error." << std::endl;
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
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		#ifndef WIN32
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "Does not support listen." << std::endl;
			assert(1);
			break;
		#endif // !WIN32
		default:
			std::cerr << "Unknown error." << std::endl;
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
			std::cerr << "Would block, try again" << std::endl;
			break;
		case EBADF:
			std::cerr << "Invalid fD" << std::endl;
			assert(1);
			break;
		case EFAULT:
			std::cerr << "Invalid parameter address." << std::endl;
			assert(1);
			break;
		case EINTR:
			std::cerr << "Interrupted by signal." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Invalid argument" << std::endl;
			assert(1);
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			break;
		case EPIPE:
			std::cerr << "Pipe broken, connection closed on local end." << std::endl;
			break;
		#ifndef WIN32
		case ECONNRESET:
			std::cerr << "Connection reset by peer" << std::endl;
			break;
		case EDESTADDRREQ: // likely result of sendmsg()
			std::cerr << "Not connceted, and no peer defined." << std::endl;
			assert(1);
			break;
		case ENOTCONN:
			std::cerr << "Not connected" << std::endl;
			assert(1);
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket" << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "Invalid flags" << std::endl;
			assert(1);
			break;
		case EISCONN: // likely result of sendmsg()
			std::cerr << "Already connected, but recipient was specified." << std::endl;
			assert(1);
			break;
		case EMSGSIZE:
			std::cerr << "Socket requires atomic sending." << std::endl;
			assert(1);
			break;
		case ENOBUFS:
			std::cerr << "Out of buffers" << std::endl;
			break;
		#endif // !WIN32
		default:
			std::cerr << "Unknown error." << std::endl;
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
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		#ifdef EWOULDBLOCK
		case EWOULDBLOCK:
		#else
		case EAGAIN:
		#endif
			// Should retry
			std::cerr << "Would block, try again." << std::endl;
			break;
		case EINTR:
			// Should retry
			std::cerr << "Interrupted by signal." << std::endl;
			break;
		case EFAULT:
			std::cerr << "Buffer points to invalid address" << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Invalid argument." << std::endl;
			assert(1);
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			//throw std::bad_alloc();
			break;
		#ifndef WIN32
		case ECONNREFUSED:
			std::cerr << "Connection refused" << std::endl;
			break;
		case ENOTCONN:
			std::cerr << "Not connected" << std::endl;
			assert(1);
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket" << std::endl;
			assert(1);
			break;
		#endif // !WIN32
		default:
			// Should not happen
			std::cerr << "Unknown error." << std::endl;
			break;
		}
	}
	
	return r;
}

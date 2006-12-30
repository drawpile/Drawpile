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
	
	assert(sock >= 0);
	
	sockaddr_in sa; // temporary
	#ifndef WIN32
	socklen_t tmp = sizeof(sockaddr);
	#else
	int tmp = sizeof(sockaddr);
	#endif
	int n_fd = ::accept(sock, reinterpret_cast<sockaddr*>(&sa), &tmp);
	error = errno;
	
	if (n_fd >= 0)
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
		case EAGAIN:
			#ifndef NDEBUG
			std::cerr << "Would block, try again." << std::endl;
			#endif
			break;
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		#endif
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			break;
		#ifndef NDEBUG
		case EINVAL:
			std::cerr << "Not listening." << std::endl;
			break;
		#endif
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
		#ifndef NDEBUG
		case EFAULT:
			std::cerr << "Addr not writable" << std::endl;
			assert(1);
			break;
		#endif
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
		#ifndef NDEBUG
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "Not of type, SOCK_STREAM." << std::endl;
			assert(1);
			break;
		#endif
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
		#ifndef WIN32
		case EPROTO:
			// whatever this is?
			#ifndef NDEBUG
			std::cerr << "Protocol error." << std::endl;
			#endif
			break;
		#endif
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

bool Socket::block(bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::block(" << (x?"true":"false") << ")" << std::endl;
	#endif
	
	assert(sock >= 0);
	
	#ifdef WIN32
	unsigned long arg = !x;
	return ioctlsocket(sock, FIONBIO, &arg);
	#else
	assert(x == false);
	return fcntl(sock, F_SETFL, O_NONBLOCK) == SOCKET_ERROR ? false : true;
	#endif
}

bool Socket::reuse(bool x) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::reuse(" << (x?"true":"false") << ")" << std::endl;
	#endif
	
	assert(sock >= 0);
	
	char val = (x ? 1 : 0);
	return (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == 0);
}

int Socket::bindTo(uint32_t address, uint16_t port) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::bindTo(" << address << ", " << port << ")" << std::endl;
	#endif
	
	assert(sock >= 0);
	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = htons(port);
	
	int r = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	error = errno;
	
	if (r == SOCKET_ERROR)
	{
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		case EINVAL:
			// According to docs, this may change in the future.
			std::cerr << "Socket already bound" << std::endl;
			assert(1);
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "does not support bind" << std::endl;
			break;
		case EAFNOSUPPORT:
			std::cerr << "invalid address family" << std::endl;
			break;
		case EISCONN:
			std::cerr << "already connected" << std::endl;
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
			std::cerr << "Unknown error." << std::endl;
			#endif
			break;
		}
	}
	
	return r;
}

int Socket::connect(sockaddr_in* rhost) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::connect()" << std::endl;
	#endif
	
	assert(sock >= 0);
	
	int r = ::connect(sock, (sockaddr*) rhost, sizeof(rhost));
	error = errno;
	
	if (r == -1)
	{
		switch (error)
		{
		case EINPROGRESS:
			std::cout << "Connection in progress." << std::endl;
			return 0;
		#ifndef NDEBUG
		case EBADF:
		case EFAULT:
		case ENOTSOCK:
		case EISCONN:
		case EADDRINUSE:
		case EAFNOSUPPORT:
			assert(1);
			break;
		#endif
		case EACCES:
		case EPERM:
			std::cerr << "Firewall denied connection?" << std::endl;
			return -1;
		case ECONNREFUSED:
			std::cout << "Connection refused" << std::endl;
			return 1;
			break;
		case ETIMEDOUT:
			std::cout << "Connection timed-out" << std::endl;
			return 2;
			break;
		case ENETUNREACH:
			std::cout << "Network unreachable" << std::endl;
			return 2;
			break;
		case EALREADY:
			std::cout << "Already connected" << std::endl;
			return 3;
		case EAGAIN:
			std::cout << "No free local ports left, try again later." << std::endl;
			return 2;
		}
	}
	
	return 0;
}

int Socket::listen() throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::listen()" << std::endl;
	#endif
	
	assert(sock >= 0);
	
	int r = ::listen(sock, 4);
	error = errno;
	
	if (r == SOCKET_ERROR)
	{
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		#endif
		#ifndef NDEBUG
		case ENOTSOCK:
			std::cerr << "Not a socket." << std::endl;
			assert(1);
			break;
		case EOPNOTSUPP:
			std::cerr << "Does not support listen." << std::endl;
			assert(1);
			break;
		#endif
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
	assert(sock >= 0);
	
	int r = ::send(sock, buffer, buflen, MSG_NOSIGNAL);
	error = errno;
	
	if (r == SOCKET_ERROR)
	{
		switch (error)
		{
		case EAGAIN:
			#ifndef NDEBUG
			std::cerr << "Would block, try again" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid fD" << std::endl;
			assert(1);
			break;
		case EFAULT:
			std::cerr << "Invalid parameter address." << std::endl;
			assert(1);
			break;
		#endif
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#ifndef NDEBUG
		case EINVAL:
			std::cerr << "Invalid argument" << std::endl;
			assert(1);
			break;
		#endif
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
		case ECONNRESET:
			#ifndef NDEBUG
			std::cerr << "Connection reset by peer" << std::endl;
			#endif
			break;
		#ifndef NDEBUG
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
		#endif
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
	
	assert(sock >= 0);
	assert(buffer != 0);
	assert(buflen != 0);
	
	int r = ::recv(sock, buffer, buflen, 0);
	error = errno;
	
	if (r == SOCKET_ERROR)
	{
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Invalid FD" << std::endl;
			assert(1);
			break;
		#endif
		case EAGAIN:
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
		#ifndef NDEBUG
		case EFAULT:
			std::cerr << "Buffer points to invalid address" << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Invalid argument." << std::endl;
			assert(1);
			break;
		#endif
		case ENOMEM:
			#ifndef NDEBUG
			std::cerr << "Out of memory" << std::endl;
			#endif
			return SOCKET_ERROR - 1;
			break;
		#ifndef NDEBUG
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
		#endif
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

#if defined(WITH_SENDFILE)
int Socket::sendfile(int fd, off_t offset, size_t nbytes, sf_hdtr *hdtr, off_t *sbytes) throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::recv(*buffer, " << buflen << ")" << std::endl;
	#endif
	
	assert(fd >= 0);
	assert(offset >= 0);
	
	// call the real sendfile()
	int r = ::sendfile(fd, sock, offset, nbytes, hdtr, sbytes, 0);
	error = errno;
	if (r == -1)
	{
		switch (error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "fd is not a valid file descriptor." << std::endl;
			assert(1);
			break;
		case ENOTSOCK:
			std::cerr << "Not a socket" << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "FD is not a regular file or socket is not of type SOCK_STREAM" << std::endl;
			assert(1);
			break;
		case ENOTCONN:
			std::cerr << "Not connected." << std::endl;
			assert(1);
			break;
		#endif // NDEBUG
		case EPIPE:
			#ifndef NDEBUG
			std::cerr << "The socket peer has closed the connection." << std::endl;
			#endif
			return -1;
		case EIO:
			#ifndef NDEBUG
			std::cerr << "An error occurred while reading from fd." << std::endl;
			#endif
			return -1;
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "An invalid address was specified for a parameter." << std::endl;
			assert(1);
			#endif
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

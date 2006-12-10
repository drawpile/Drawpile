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

Socket* Socket::accept()
{
	#ifndef NDEBUG
	std::cout << "Socket::accept()" << std::endl;
	#endif

	Socket* s = new Socket();
	sockaddr_in* a = s->getAddr();
	int n_fd = ::accept(sock, (sockaddr*)a, NULL);
	error = errno;
	if (n_fd > 0)
	{
		std::cout << "New connection" << std::endl;
		s->fd(n_fd);
		return s;
	}
	else
	{
		std::cout << "Invalid socket" << std::endl;
		delete s;
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
	
	return r;
}

int Socket::listen() throw()
{
	#ifndef NDEBUG
	std::cout << "Socket::listen()" << std::endl;
	#endif

	int r = ::listen(sock, 4);
	error = errno;
	return r;
}

int Socket::send(char* buffer, size_t buflen) throw()
{
	assert(buffer != 0);
	assert(buflen != 0);
	
	#ifndef NDEBUG
	std::cout << "Socket::send(*buffer, " << buflen << ")" << std::endl;
	#endif
	
	int r = ::send(sock, buffer, buflen, MSG_NOSIGNAL);
	
	error = errno;
	return r;
}

int Socket::recv(char* buffer, size_t buflen) throw()
{
	assert(buffer != 0);
	assert(buflen != 0);
	
	#ifndef NDEBUG
	std::cout << "Socket::recv(*buffer, " << buflen << ")" << std::endl;
	#endif
	
	int r = ::recv(sock, buffer, buflen, 0);
	error = errno;
	return r;
}

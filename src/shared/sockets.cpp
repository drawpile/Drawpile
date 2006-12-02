/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#include "sockets.h"

#include <cassert>

void Socket::close() throw()
{
	#ifdef WIN32
	closesocket(sock);
	#else
	::close(sock);
	#endif
}

Socket* Socket::accept() throw()
{
	Socket* s = new Socket();
	sockaddr_in* a = s->getAddr();
	int n_fd = ::accept(sock, (sockaddr*)a, NULL);
	error = errno;
	if (n_fd > 0)
	{
		s->fd(n_fd);
		return s;
	}
	else
	{
		delete s;
		return 0;
	}
}

int Socket::bindTo(uint32_t address, uint16_t port) throw()
{
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = htons(port);
	
	int r = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	error = errno;
	
	return r;
}

int Socket::listen() throw()
{
	int r = ::listen(sock, 4);
	error = errno;
	return r;
}

int Socket::send(char* buffer, size_t buflen) throw()
{
	assert(buffer != 0);
	assert(buflen != 0);
	
	int r = ::send(sock, buffer, buflen, 0);
	error = errno;
	return r;
}

int Socket::recv(char* buffer, size_t buflen) throw()
{
	assert(buffer != 0);
	assert(buflen != 0);
	
	int r = ::recv(sock, buffer, buflen, 0);
	error = errno;
	return r;
}

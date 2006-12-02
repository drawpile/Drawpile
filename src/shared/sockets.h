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

#ifndef Sockets_INCLUDED
#define Sockets_INCLUDED

#include <stdint.h>

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	// TODO
#endif

class Socket
{
protected:
	uint32_t sock;
	
	sockaddr_in addr;
	
	int error;
public:
	Socket() throw() { }

	virtual ~Socket() throw() { close(); };
	
	void close() throw();
	
	uint32_t fd(uint32_t nsock) throw() { return sock = nsock; }
	uint32_t fd() throw() { return sock; }
	
	Socket* accept() throw();
	
	int bindTo(uint32_t address, uint16_t port) throw();
	int listen() throw();
	
	int send(char* buffer, size_t buflen) throw();
	int recv(char* buffer, size_t buflen) throw();
	
	int getError() throw() { return error; }
	
	sockaddr_in* getAddr() { return &addr; }
	
	int address() const throw() { return ntohl(addr.sin_addr.s_addr); }
	int port() const throw() { return ntohs(addr.sin_port); }
};

#endif // Sockets_INCLUDED

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

#include "../shared/protocol.h"
#include "../shared/sockets.h"

#include "event.h"

//#include <sys/time.h>
#include <getopt.h>

#include <vector>
#include <cstdlib>
#include <iostream>

std::vector<Socket*> sockets;

int cleanexit(int rc)
{
	std::vector<Socket*>::iterator s(sockets.begin());
	for (; s != sockets.end(); s++)
	{
		(*s)->close();
	}
	
	#ifdef WIN32
	WSACleanup();
	#endif
	
	exit(rc);
}

int main(int argc, char** argv)
{
	
	std::ios::sync_with_stdio(false);
	
	std::cout << "dpsrv v0.0a\n(C) 2006 M.K.A.\n" << std::endl;
	
	/* options and other stuff */
	
	uint16_t
		hi_port = 0,
		lo_port = 0;
	
	char* password = 0;
	size_t pw_len = 0,
		user_limit = 0;
	
	//bool localhost_admin = true;
	
	/* parse args */
	{ // limited scope
	int32_t opt = 0;
	
	while ((opt = getopt( argc, argv, "p:Vhc:")) != -1)
	{
		switch (opt)
		{
			case 'p': // port to listen on
				lo_port = atoi(optarg);
				{
					char* off = strchr(optarg, '-');
					hi_port = (off != 0 ? atoi(off+1) : lo_port);
				}
				
				if (lo_port <= 1024 or hi_port <= 1024)
				{
					std::cout << "Super-user ports not allowed!" << std::endl;
					exit(1);
				}
				
				break;
			/*
			case 'l': // localhost admin
				localhost_admin = true;
				break;
			*/
			case 'u': // user limit
				user_limit = atoi(optarg);
				break;
			case 'c': // password
				pw_len = strlen(optarg);
				password = new char[pw_len];
				memcpy(password, optarg, pw_len);
				break;
			case 'h': // help
				std::cout << "Syntax: dbsrv [options]\n\n"
					<< "Options...\n\n"
					<< "   -p [port]   listen on 'port'.\n"
					<< "   -h      This output (a.k.a. Help)" << std::endl;
				exit(1);
				break;
			case 'V': // version
				exit(0);
			default:
				std::cout << "What?" << std::endl;
				exit(0);
		}
	}
	
	} // end limited scope
	
	sockets.reserve(10);
	
	#ifdef WIN32
	std::cout << "initializing WSA" << std::endl;
	WSADATA info;
    if (WSAStartup(MAKEWORD(2,0), &info)) { exit(1); }
    #endif
	
	Socket s;
	sockets.push_back(&s);
	std::cout << "creating socket" << std::endl;
	s.fd(socket(AF_INET, SOCK_STREAM, 0));
	
	if (s.fd() == INVALID_SOCKET)
	{
		std::cout << "failed to create a socket." << std::endl;
		cleanexit(1);
	}
	
	std::cout << "binding socket address" << std::endl;
	{ // limited scope
		bool bound = false;
		for (int bport=lo_port; bport < hi_port+1; bport++)
		{
			if (s.bindTo(INADDR_ANY, bport) == SOCKET_ERROR)
			{
				if (s.getError() == EBADF)
					cleanexit(1);
				else if (s.getError() == EINVAL)
					cleanexit(1);
			}
			else
			{
				bound = true;
				break;
			}
		}
		
		if (!bound)
		{
			std::cout << "Failed to bind to any port." << std::endl;
			int e = s.getError();
			switch (e)
			{
				#ifndef WIN32
				case EADDRINUSE:
					std::cout << "address already in use" << std::endl;
					break;
				case EADDRNOTAVAIL:
					std::cout << "address not available" << std::endl;
					break;
				case EAFNOSUPPORT:
					std::cout << "invalid address family" << std::endl;
					break;
				case ENOTSOCK:
					std::cout << "not a socket" << std::endl;
					break;
				case EOPNOTSUPP:
					std::cout << "does not support bind" << std::endl;
					break;
				case EISCONN:
					std::cout << "already connected" << std::endl;
					break;
				case ENOBUFS:
					std::cout << "insufficient resources" << std::endl;
					break;
				#endif
				case EACCES:
					std::cout << "can't bind to superuser sockets" << std::endl;
					break;
				default:
					std::cout << "unknown error" << std::endl;
					break;
			}
			cleanexit(1);
		}
	}
	
	if (s.listen() == SOCKET_ERROR)
	{
		std::cout << "Failed to open listening port." << std::endl;
		cleanexit(0);
	}
	
	std::cout << "listening on: " << s.address() << ":" << s.port() << std::endl;
	
	{ // limited scope for ev.
	Event ev;
	
	ev.add(s.fd(), ev.read);
	
	int ec;
	while (1)
	{
		ec = ev.wait(0, 500000);
		
		if (ec == 0)
		{
			// continue, no fds or time exceeded.
		}
		else if (ec == -1)
		{
			// TODO (error)
		}
		else
		{
			// do something
		}
	}
	
	} // limited scope for ev.
	
	#ifdef WIN32
	WSACleanup();
	#endif
	
	return 0;
}

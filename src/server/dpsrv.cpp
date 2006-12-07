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

#include "user.h"
#include "event.h"

//#include <sys/time.h>
#include <getopt.h> // for command-line opts

#include <bitset>
#include <map>
#include <list>
#include <vector>
#include <cstdlib>
#include <iostream>

Event ev;

const int hard_limit = 255;

std::bitset<hard_limit> user_ids;
std::bitset<hard_limit> session_ids;

std::vector<uint32_t> sockets;
std::map<uint32_t, User*> users;
std::map<uint8_t, uint32_t> user_id_map;

// listening socket
Socket lsock;

/** */
uint8_t getUserID()
{
	for (int i=1; i != 256; i++)
	{
		if (!user_ids.test(i))
		{
			user_ids.set(i, true);
			return i;
		}
	}
	
	return 0;
}

/** */
uint8_t getSessionID()
{
	for (int i=1; i != 256; i++)
	{
		if (!session_ids.test(i))
		{
			session_ids.set(i, true);
			return i;
		}
	}
	
	return 0;
}

/** */
void restoreUserID(uint8_t id)
{
	user_ids.set(id, false);
}

/** */
void restoreSessionID(uint8_t id)
{
	session_ids.set(id, false);
}

/** */
int cleanexit(int rc)
{
	lsock.close();
	
	/*
	std::vector<Socket*>::iterator s(sockets.begin());
	for (; s != sockets.end(); s++)
	{
		(*s)->close();
	}
	*/
	
	#ifdef WIN32
	WSACleanup();
	#endif
	
	exit(rc);
}

char* password = 0;
size_t pw_len = 0,
	user_limit = 0,
	cur_users = 0;

/** */
void uWrite(uint32_t fd)
{
	//users[fd]
	User* u = users[fd];
	
	Buffer buf = u->buffers.front();
	
	int rb = u->s->send(
		buf.rpos,
		buf.canRead()
	);
	
	if (rb > 0)
	{
		buf.read(rb);
		
		// just to ensure we don't need to do anything for it.
		assert(buf.rpos == u->buffers.front().rpos);
		
		if (buf.left == 0)
		{
			delete [] buf.data;
			u->buffers.pop();
			if (u->buffers.empty())
				ev.remove(fd, ev.write);
		}
	}
}

/** */
void uRead(uint32_t fd)
{
	//users[fd]
	// TODO
	
	
}

/** */
void uExcept(uint32_t fd)
{
	//users[fd]
	// TODO: SigPipe is the only thing that goes here, right?
	// So, this isn't really needed.
}

/** */
int main(int argc, char** argv)
{
	
	std::ios::sync_with_stdio(false);
	
	std::cout << "dpsrv v0.0a\n(C) 2006 M.K.A.\n" << std::endl;
	
	/* options and other stuff */
	
	uint16_t
		hi_port = 0,
		lo_port = 0;
	
	//bool localhost_admin = true;
	
	/* parse args */
	{ // limited scope
	int32_t opt = 0;
	
	while ((opt = getopt( argc, argv, "p:Vhc:")) != -1)
	{
		switch (opt)
		{
			/*
			case 'a': // address to listen on
				
				break;
			*/
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
	
	#ifdef WIN32
	std::cout << "initializing WSA" << std::endl;
	WSADATA info;
    if (WSAStartup(MAKEWORD(2,0), &info)) { exit(1); }
    #endif
	
	std::cout << "creating socket" << std::endl;
	lsock.fd(socket(AF_INET, SOCK_STREAM, 0));
	
	if (lsock.fd() == INVALID_SOCKET)
	{
		std::cout << "failed to create a socket." << std::endl;
		cleanexit(1);
	}
	
	lsock.block(0); // nonblocking
	
	std::cout << "binding socket address" << std::endl;
	{ // limited scope
		bool bound = false;
		for (int bport=lo_port; bport < hi_port+1; bport++)
		{
			#ifndef NDEBUG
			std::cout << "trying: " << INADDR_ANY << ":" << bport << std::endl;
			#endif
			
			if (lsock.bindTo(INADDR_ANY, bport) == SOCKET_ERROR)
			{
				if (lsock.getError() == EBADF)
					cleanexit(1);
				else if (lsock.getError() == EINVAL)
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
			int e = lsock.getError();
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
	
	if (lsock.listen() == SOCKET_ERROR)
	{
		std::cout << "Failed to open listening port." << std::endl;
		cleanexit(0);
	}
	
	std::cout << "listening on: " << lsock.address() << ":" << lsock.port() << std::endl;
	
	
	sockets.reserve(10);
	
	ev.init();
	
	ev.add(lsock.fd(), ev.read);
	
	Socket *nu;
	
	std::vector<uint32_t>::iterator siter;
	
	int ec;
	while (1) // yay for infinite loops
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
			if (ev.isset(lsock.fd(), ev.read))
			{
				ec--;
				
				nu = lsock.accept();
				
				if (nu != 0)
				{
					uint8_t id = getUserID();
					if (id == 0)
					{
						delete nu;
					}
					else
					{
						User *ud = new User;
						ud->s = nu;
						ud->id = id;
						
						sockets.push_back(nu->fd());
						
						ev.add(nu->fd(), ev.read);
						ev.add(nu->fd(), ev.except);
						
						users[nu->fd()] = ud;
						user_id_map[id] = nu->fd();
					}
				}
				// new connection?
			}
			
			if (ec != 0)
			{
				for (siter = sockets.begin(); siter != sockets.end(); siter++)
				{
					if (ev.isset(*siter, ev.read))
					{
						ec--;
						uRead(*siter);
					}
					else if (ev.isset(*siter, ev.write))
					{
						ec--;
						uWrite(*siter);
					}
					else if (ev.isset(*siter, ev.except))
					{
						ec--;
						uExcept(*siter);
					}
					
					if (ec == 0) break;
				}
			}
			
			// do something
		}
		
		// do something generic?
	}
	
	ev.finish();
	
	cleanexit(0);
	return 0; // never reached
}

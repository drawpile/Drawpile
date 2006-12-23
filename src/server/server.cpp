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

   ---

   For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#include "server.h"

#include "../shared/templates.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.helper.h"
//#include "../shared/protocol.h"

#include <getopt.h> // for command-line opts
#include <cstdlib>
#include <iostream>

Server::Server() throw()
	: password(0),
	pw_len(0),
	user_limit(0),
	cur_users(0),
	hi_port(protocol::default_port),
	lo_port(protocol::default_port)
{
	#ifndef NDEBUG
	std::cout << "Server::Server()" << std::endl;
	#endif
}

Server::~Server() throw()
{
	#ifndef NDEBUG
	std::cout << "Server::~Server()" << std::endl;
	#endif
	
	cleanup();
}

uint8_t Server::getUserID() throw()
{
	#ifndef NDEBUG
	std::cout << "Server::getUserID()" << std::endl;
	#endif
	
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

uint8_t Server::getSessionID() throw()
{
	#ifndef NDEBUG
	std::cout << "Server::getSessionID()" << std::endl;
	#endif
	
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

void Server::freeUserID(uint8_t id) throw()
{
	assert(user_ids.test(id) == true);
	
	#ifndef NDEBUG
	std::cout << "Server::freeUserID(" << static_cast<int>(id) << ")" << std::endl;
	#endif
	
	user_ids.set(id, false);
}

void Server::freeSessionID(uint8_t id) throw()
{
	assert(session_ids.test(id) == true);
	
	#ifndef NDEBUG
	std::cout << "Server::freeSessionID(" << static_cast<int>(id) << ")" << std::endl;
	#endif
	
	session_ids.set(id, false);
}

void Server::cleanup() throw()
{
	#ifndef NDEBUG
	std::cout << "Server::cleanup()" << std::endl;
	#endif
	
	// finish event system
	ev.finish();
	
	// close listening socket
	lsock.close();
	
	// delete users
	for (std::map<int, User*>::iterator uiter(users.begin()); uiter != users.end(); uiter++)
	{
		#if defined(FULL_CLEANUP)
		delete uiter->second;
		#else
		uiter->second->sock->close();
		#endif
	}
	
	#if defined( FULL_CLEANUP )
	users.clear();
	#endif // FULL_CLEANUP
}

void Server::uWrite(User* usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uWrite(" << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	Buffer buf = usr->buffers.front();
	
	int sb = usr->sock->send(
		buf.rpos,
		buf.canRead()
	);
	
	std::cout << "Sent " << sb << " bytes.." << std::endl;
	
	if (sb > 0)
	{
		buf.read(sb);
		
		// just to ensure we don't need to do anything for it.
		assert(buf.rpos == usr->buffers.front().rpos);
		
		if (buf.left == 0)
		{
			// remove buffer
			delete [] buf.data;
			usr->buffers.pop();
			
			// remove fd from write list if no buffers left.
			if (usr->buffers.empty())
			{
				fClr(usr->events, ev.write);
				if (usr->events == 0)
					ev.remove(usr->sock->fd(), usr->events);
				else
					ev.modify(usr->sock->fd(), usr->events);
			}
		}
	}
	else if (sb == 0)
	{
		// nothing
	}
	else
	{
		std::cerr << "Error occured while sending to user: "
			<< static_cast<int>(usr->id) << std::endl;
		
		fClr(usr->events, ev.read);
		if (usr->events == 0)
			ev.remove(usr->sock->fd(), usr->events);
		else
			ev.modify(usr->sock->fd(), usr->events);
	}
}

void Server::uRead(User* usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uRead(" << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	std::cout << "From user: " << static_cast<int>(usr->id) << std::endl;
	
	if (usr->input.canWrite() == 0)
	{
		std::cerr << "User #" << static_cast<int>(usr->id) << " input buffer full!" << std::endl;
		return;
	}
	
	int rb = usr->sock->recv(
		usr->input.wpos,
		usr->input.canWrite()
	);
	
	if (rb > 0)
	{
		std::cout << "Received " << rb << " bytes.." << std::endl;
		
		usr->input.write(rb);
		
		if (usr->inMsg == 0)
		{
			try {
				usr->inMsg  = protocol::stack::get(usr->input.rpos[0]);
			}
			catch (std::exception &e) {
				std::cerr << "Invalid data from user: "
					<< static_cast<int>(usr->id) << std::endl;
				uRemove(usr);
				return;
			}
		}
		
		size_t len = usr->inMsg->reqDataLen(usr->input.rpos, usr->input.canRead());
		if (len > usr->input.canRead())
		{
			// still need more data
			return;
		}
		
		// unserialize message...
		usr->input.read(
			usr->inMsg->unserialize(usr->input.rpos, usr->input.canRead())
		);
		
		// TODO: Handle message
		uHandleMsg(usr, usr->inMsg);
		
		protocol::stack::free(usr->inMsg);
		usr->inMsg = 0;
	}
	else if (rb == 0)
	{
		std::cout << "User disconnected!" << std::endl;
		
		uRemove(usr);
	}
	else
	{
		std::cerr << "Error occured while reading from user: "
			<< static_cast<int>(usr->id) << std::endl;
		
		// TODO (EAGAIN and such)
		
		usr->events = fClr(usr->events, ev.read);
		ev.modify(usr->sock->fd(), usr->events);
	}
}

void Server::uHandleMsg(User* usr, protocol::Message* msg) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleMsg(" << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	switch (usr->state)
	{
	case uState::active:
		std::cout << "active" << std::endl;
		// TODO
		break;
	case uState::login:
		std::cout << "login" << std::endl;
		if (msg->type == protocol::type::Identifier)
		{
			protocol::Identifier *i = static_cast<protocol::Identifier*>(msg);
			bool str = (
				memcmp((char*)&i->identifier,
					&protocol::identifierString,
					protocol::identifier_size
				) == 0);
			bool rev = (i->revision == protocol::revision);
			
			if (!str)
			{
				std::cerr << "Protocol string mismatch" << std::endl;
				uRemove(usr);
				return;
			}
			if (!rev)
			{
				std::cerr << "Protocol revision mismatch" << std::endl;
				uRemove(usr);
				return;
			}
			
			usr->state = uState::active;
			
			// TODO: send Auth Request
			// TODO: send Host Info.
		}
		else
		{
			std::cerr << "Invalid data from user: "
				<< static_cast<int>(usr->id) << std::endl;
			
			uRemove(usr);
		}
		break;
	default:
		assert(!"user state was something strange");
		break;
	}
}

void Server::uAdd(Socket* sock) throw(std::bad_alloc)
{
	uint8_t id = getUserID();
	
	if (id == 0)
	{
		#ifndef NDEBUG
		std::cout << "... server is full." << std::endl;
		#endif
		
		delete sock;
		return;
	}
	else
	{
		#ifndef NDEBUG
		std::cout << "... assigned ID: " << static_cast<uint32_t>(id) << std::endl;
		#endif
		
		User *usr = new User(id, sock);
		
		fSet(usr->events, ev.read);
		ev.add(usr->sock->fd(), usr->events);
		
		users.insert( std::make_pair(sock->fd(), usr) );
		user_id_map.insert( std::make_pair(id, usr) );
		
		if (usr->input.data == 0)
		{
			#ifndef NDEBUG
			std::cout << "Assigning buffer to user." << std::endl;
			#endif
			
			size_t buf_size = 8196;
			usr->input.setBuffer(new char[buf_size], buf_size);
		}
		
		#ifndef NDEBUG
		std::cout << "Known users: " << users.size() << std::endl;
		#endif
	}
}

void Server::uRemove(User* usr) throw()
{
	ev.remove(usr->sock->fd(), ev.write|ev.read);
	
	freeUserID(usr->id);
	
	int id = usr->sock->fd();
	std::cout << "Deleting user.." << std::endl;
	delete usr;
	
	std::cout << "Removing from mappings" << std::endl;
	users.erase(id);
}

void Server::getArgs(int argc, char** argv) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::getArgs(" << argc << ", **argv)" << std::endl;
	#endif
	
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
				
				if (lo_port <= 1023 or hi_port <= 1023)
				{
					std::cerr << "Super-user ports not allowed!" << std::endl;
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
				if (pw_len > 0)
				{
					password = new char[pw_len];
					memcpy(password, optarg, pw_len);
				}
				break;
			case 'h': // help
				std::cout << "Syntax: dbsrv [options]\n\n"
					<< "Options...\n\n"
					<< "   -p [port]  listen on 'port' (1024 - 65535).\n"
					<< "   -h         This output (a.k.a. Help)" << std::endl;
				exit(1);
				break;
			case 'V': // version
				exit(0);
			default:
				std::cerr << "What?" << std::endl;
				exit(0);
		}
	}
}

int Server::init() throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::init()" << std::endl;
	#endif
	
	#ifndef NDEBUG
	std::cout << "creating socket" << std::endl;
	#endif
	
	lsock.fd(socket(AF_INET, SOCK_STREAM, 0));
	#ifndef NDEBUG
	std::cout << "New socket: " << lsock.fd() << std::endl;
	#endif
	
	if (lsock.fd() == -1)
	{
		std::cerr << "failed to create a socket." << std::endl;
		return -1;
	}
	
	lsock.block(0); // nonblocking
	
	#ifndef NDEBUG
	std::cout << "binding socket address" << std::endl;
	#endif
	{ // limited scope
		bool bound = false;
		for (int bport=lo_port; bport < hi_port+1; bport++)
		{
			#ifndef NDEBUG
			std::cout << "trying: " << INADDR_ANY << ":" << bport << std::endl;
			#endif
			
			if (lsock.bindTo(INADDR_ANY, bport) == SOCKET_ERROR)
			{
				if (lsock.getError() == EBADF || lsock.getError() == EINVAL)
					return -1;
				
				// continue
			}
			else
			{
				bound = true;
				break;
			}
		}
		
		if (!bound)
		{
			std::cerr << "Failed to bind to any port." << std::endl;
			int e = lsock.getError();
			switch (e)
			{
				#ifndef WIN32
				case EADDRINUSE:
					std::cerr << "address already in use" << std::endl;
					break;
				case EADDRNOTAVAIL:
					std::cerr << "address not available" << std::endl;
					break;
				case EAFNOSUPPORT:
					std::cerr << "invalid address family" << std::endl;
					break;
				case ENOTSOCK:
					std::cerr << "not a socket" << std::endl;
					break;
				case EOPNOTSUPP:
					std::cerr << "does not support bind" << std::endl;
					break;
				case EISCONN:
					std::cerr << "already connected" << std::endl;
					break;
				case ENOBUFS:
					std::cerr << "insufficient resources" << std::endl;
					break;
				#endif // WIN32
				case EACCES:
					std::cerr << "can't bind to superuser sockets" << std::endl;
					break;
				default:
					std::cerr << "unknown error" << std::endl;
					break;
			}
			
			return -1;
		}
	}
	
	if (lsock.listen() == -1)
	{
		std::cerr << "Failed to open listening port." << std::endl;
		return -1;
	}
	
	std::cout << "listening on: " << lsock.address() << ":" << lsock.port() << std::endl;
	
	if (!ev.init())
		return -1;
	
	ev.add(lsock.fd(), ev.read);
	
	return 0;
}

int Server::run() throw()
{
	#ifndef NDEBUG
	std::cout << "Server::run()" << std::endl;
	#endif
	
	// define temporary socket
	Socket *nu;
	
	// define iterators
	std::vector<uint32_t>::iterator si;
	std::map<int, User*>::iterator ui;
	
	#ifndef NDEBUG
	std::cout << "eternity" << std::endl;
	#endif
	
	//EvList evl;
	
	// event count
	int ec /*, evs */;
	
	// main loop
	while (1)
	{
		ec = ev.wait(5000);
		
		if (ec == 0)
		{
			// continue, no fds or time exceeded.
		}
		else if (ec == -1)
		{
			std::cout << "Error in event system." << std::endl;
			// TODO (error)
		}
		else
		{
			//evl = ev.getEvents( ec );
			#ifndef NDEBUG
			std::cout << "Events waiting: " << ec << std::endl;
			#endif
			
			if (ev.isset(lsock.fd(), ev.read))
			{
				#ifndef NDEBUG
				std::cout << "Server socket triggered" << std::endl;
				#endif
				
				ec--;
				
				nu = lsock.accept();
				
				if (nu != 0)
				{
					#ifndef NDEBUG
					std::cout << "New connection" << std::endl;
					#endif
					
					uAdd( nu );
				}
				else
				{
					#ifndef NDEBUG
					std::cout << "New connection but no socket?" << std::endl;
					#endif
				}
			}
			
			if (ec > 0)
			{
				#ifndef NDEBUG
				std::cout << "Triggered sockets left: " << ec << std::endl;
				#endif
				
				
				for (ui = users.begin(); ui != users.end(); ui++)
				{
					std::cout << "Testing: " << ui->first << std::endl;
					if (ev.isset(ui->first, ev.read))
					{
						#ifndef NDEBUG
						std::cout << "Reading from client" << std::endl;
						#endif
						
						ec--;
						uRead(ui->second);
					}
					else if (ev.isset(ui->first, ev.write))
					{
						#ifndef NDEBUG
						std::cout << "Writing to client" << std::endl;
						#endif
						
						ec--;
						uWrite(ui->second);
					}
					
					if (ec == 0) break;
				}
				
				#ifndef NDEBUG
				if (ec > 0)
					std::cout << "Not in clients..." << std::endl;
				#endif
			}
			
			// do something
		}
		
		// do something generic?
	}
	
	#ifndef NDEBUG
	std::cout << "done?" << std::endl;
	#endif
	
	return 0;
}

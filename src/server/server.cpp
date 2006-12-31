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
#include "../shared/protocol.admin.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.helper.h"
#include "../shared/protocol.h" // Message()

#include "../shared/SHA1.h"

#include <getopt.h> // for command-line opts
#include <cstdlib>
#include <iostream>

Server::Server() throw()
	: password(0),
	pw_len(0),
	user_limit(0),
	session_limit(1),
	max_subscriptions(1),
	name_len_limit(8),
	hi_port(protocol::default_port),
	lo_port(protocol::default_port),
	min_dimension(400),
	requirements(0),
	extensions(0),
	default_user_mode(protocol::user::None),
	localhost_admin(true)
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
	
	return protocol::Global;
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

protocol::Authentication* Server::msgAuth(User* usr, uint8_t session) throw(std::bad_alloc)
{
	protocol::Authentication* m = new protocol::Authentication;
	
	m->session_id = session;
	
	// FIXME
	memcpy(m->seed, "1234", protocol::password_seed_size);
	
	return m;
}

protocol::HostInfo* Server::msgHostInfo() throw(std::bad_alloc)
{
	protocol::HostInfo *m = new protocol::HostInfo;
	
	m->sessions = session_ids.count();
	m->sessionLimit = session_limit;
	m->users = user_ids.count();
	m->userLimit = user_limit;
	m->nameLenLimit = name_len_limit;
	m->maxSubscriptions = max_subscriptions;
	m->requirements = requirements;
	m->extensions = extensions;
	
	return m;
}

void Server::uWrite(User* usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uWrite(" << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	Buffer *buf = usr->buffers.front();
	
	int sb = usr->sock->send(
		buf->rpos,
		buf->canRead()
	);
	
	std::cout << "Sent " << sb << " bytes.." << std::endl;
	
	if (sb == -1)
	{
		std::cerr << "Error occured while sending to user: "
			<< static_cast<int>(usr->id) << std::endl;
		
		fClr(usr->events, ev.read);
		if (usr->events == 0)
			ev.remove(usr->sock->fd(), usr->events);
		else
			ev.modify(usr->sock->fd(), usr->events);
	}
	else if (sb == 0)
	{
		// no data was sent, and no error occured.
	}
	else
	{
		buf->read(sb);
		
		// just to ensure we don't need to do anything for it.
		assert(buf->rpos == usr->buffers.front()->rpos);
		
		if (buf->left == 0)
		{
			// remove buffer
			delete buf;
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
		
		{
			try {
				usr->inMsg  = protocol::getMessage(usr->input.rpos[0]);
			}
			catch (std::exception &e) {
				std::cerr << "Invalid data from user: "
					<< static_cast<int>(usr->id) << std::endl;
				uRemove(usr);
				return;
			}
		}
		
		std::cout << "Verify size" << std::endl;
		size_t len = usr->inMsg->reqDataLen(usr->input.rpos, usr->input.canRead());
		if (len > usr->input.canRead())
		{
			// still need more data
			return;
		}
		
		std::cout << "Unserialize message" << std::endl;
		
		usr->input.read(
			usr->inMsg->unserialize(usr->input.rpos, usr->input.canRead())
		);
		
		switch (usr->state)
		{
		case uState::active:
			uHandleMsg(usr);
			break;
		default:
			uHandleLogin(usr);
			break;
		}
		
		delete usr->inMsg;
		usr->inMsg = 0;
	}
	else if (rb == 0)
	{
		std::cout << "User disconnected!" << std::endl;
		
		uRemove(usr);
		return;
	}
	else
	{
		switch (usr->sock->getError())
		{
		case EAGAIN:
		case EINTR:
			// retry later
			return;
		default:
			std::cerr << "Unrecoverable error occured while reading from user: "
				<< static_cast<int>(usr->id) << std::endl;
			
			uRemove(usr);
			return;
		}
	}
}

void Server::uHandleMsg(User* usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleMsg(user id: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	
	assert(usr->state == uState::active);
	assert(usr->inMsg != 0);
	
	// TODO
	switch (usr->inMsg->type)
	{
	case protocol::type::Acknowledgement:
		std::cout << "Acknowledgement" << std::endl;
		// for what?
		break;
	case protocol::type::Unsubscribe:
		std::cout << "Unsubscribe" << std::endl;
		
		break;
	case protocol::type::Subscribe:
		std::cout << "Subscribe" << std::endl;
		
		break;
	case protocol::type::ListSessions:
		std::cout << "List Sessions" << std::endl;
		{
			protocol::SessionInfo *nfo = 0;
			std::map<uint8_t, Session*>::iterator si(session_id_map.begin());
			for (; si != session_id_map.end(); si++)
			{
				nfo = new protocol::SessionInfo;
				
				nfo->identifier = si->second->id;
				
				nfo->width = si->second->width;
				nfo->height = si->second->height;
				
				nfo->owner = si->second->owner;
				nfo->users = si->second->users.size();
				nfo->limit = si->second->limit;
				nfo->mode = si->second->mode;
				nfo->length = si->second->len;
				nfo->title = si->second->title;
				
				uSendMsg(usr, nfo);
			}
			
			protocol::Acknowledgement *ack = new protocol::Acknowledgement;
			
			ack->event = protocol::type::ListSessions;
			
			uSendMsg(usr, ack);
		}
		break;
	case protocol::type::Instruction:
		std::cout << "Instruction" << std::endl;
		uHandleInstruction(usr);
		break;
	default:
		std::cerr << "Unexpected or unknown message type" << std::endl;
		break;
	}
	
	uRemove(usr);
}

void Server::uHandleInstruction(User* usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleInstruction(user id: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::type::Instruction);
	
	protocol::Instruction* m = static_cast<protocol::Instruction*>(usr->inMsg);
	
	if (!fIsSet(usr->mode, protocol::user::Administrator))
	{
		std::cerr << "Non-admin tries to pass instructions" << std::endl;
		uRemove(usr);
		return;
	}
	
	switch (m->command)
	{
	case protocol::admin::command::Create:
		{
			uint8_t session_id = getSessionID();
			
			if (session_id == protocol::Global)
			{
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::SessionLimit;
				uSendMsg(usr, errmsg);
				return;
			}
			
			Session *s = new Session;
			s->id = session_id;
			s->limit = m->aux_data;
			s->mode = m->aux_data2;
			
			if (s->limit < 2)
			{
				std::cerr << "Attempted to create user session." << std::endl;
				delete s;
				return;
			}
			
			size_t crop = sizeof(s->width) + sizeof(s->height);
			if (m->length < crop)
			{
				std::cerr << "Less data than required" << std::endl;
				uRemove(usr);
				delete s;
				return;
			}
			
			memcpy_t(s->width, m->data);
			memcpy_t(s->height, m->data+sizeof(s->width));
			
			bswap(s->width);
			bswap(s->height);
			
			if (s->width < min_dimension or s->height < min_dimension)
			{
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::TooSmall;
				uSendMsg(usr, errmsg);
				delete s;
				return;
			}
			
			if (m->length > crop)
			{
				s->title = new char[m->length - crop];
				memcpy(s->title, m->data+crop, m->length-crop);
			}
			
			s->owner = usr->id;
			
			session_id_map.insert( std::make_pair(s->id, s) );
			
			std::cout << "Session created: " << s->id << std::endl
				<< "With dimensions: " << s->width << " x " << s->height << std::endl;
			
			// TODO
			//registerSession(s);
		}
		break;
	case protocol::admin::command::Destroy:
		// TODO
		break;
	case protocol::admin::command::Alter:
		// TODO
		break;
	case protocol::admin::command::Password:
		if (m->session == protocol::Global)
		{
			password = m->data;
			pw_len = m->length;
			
			m->data = 0;
			m->length = 0;
		}
		else
		{
			// TODO
		}
		break;
	default:
		std::cerr << "Unrecognized command: "
			<< static_cast<int>(m->command) << std::endl;
		break;
	}
}

void Server::uHandleLogin(User* usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleLogin(user id: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	
	switch (usr->state)
	{
	case uState::login:
		#ifndef NDEBUG
		std::cout << "login" << std::endl;
		#endif
		
		if (usr->inMsg->type == protocol::type::UserInfo)
		{
			protocol::UserInfo *m = static_cast<protocol::UserInfo*>(usr->inMsg);
			
			if (m->user_id != protocol::null_user)
			{
				std::cerr << "User made blasphemous assumption." << std::endl;
				uRemove(usr);
				return;
			}
			
			if (m->session_id != protocol::Global)
			{
				std::cerr << "Wrong session identifier." << std::endl;
				uRemove(usr);
				return;
			}
			
			if (m->event != protocol::user_event::Login)
			{
				std::cerr << "Wrong user event." << std::endl;
				uRemove(usr);
				return;
			}
			
			if (m->length > name_len_limit)
			{
				std::cerr << "Name too long." << std::endl;
				uRemove(usr);
				return;
			}
			
			// assign message the user's id
			m->user_id = usr->id;
			
			// assign user their own name
			usr->name = m->name;
			usr->nlen = m->length;
			
			// null the message's name information
			m->length = 0;
			m->name = 0;
			
			// set user mode
			usr->mode = m->mode = default_user_mode;
			
			// auto admin promotion
			if (localhost_admin && usr->sock->address() == INADDR_LOOPBACK)
			{
				fSet(usr->mode, protocol::user::Administrator);
				m->mode = usr->mode;
			}
			
			// reply
			uSendMsg(usr, m);
			
			usr->state = uState::active;
			
			usr->inMsg = 0;
			return;
		}
		else
		{
			// wrong message type
			uRemove(usr);
		}
		break;
	case uState::lobby_auth:
		#ifndef NDEBUG
		std::cout << "lobby_auth" << std::endl;
		#endif
		// TODO
		break;
	case uState::login_auth:
		#ifndef NDEBUG
		std::cout << "login_auth" << std::endl;
		#endif
		
		if (usr->inMsg->type == protocol::type::Password)
		{
			// TODO
			protocol::Password *m = static_cast<protocol::Password*>(usr->inMsg);
			
			CSHA1 h;
			h.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			h.Update(reinterpret_cast<const uint8_t*>("1234"), 4);
			h.Final();
			char digest[protocol::password_hash_size];
			h.GetHash(reinterpret_cast<uint8_t*>(digest));
			
			if (memcmp(digest, m->data, protocol::password_hash_size) != 0)
			{
				// mismatch, send error or disconnect.
				uRemove(usr);
				return;
			}
		}
		else
		{
			uRemove(usr);
			return;
		}
		
		usr->state = uState::login;
		uSendMsg(usr, msgHostInfo());
		
		break;
	case uState::init:
		#ifndef NDEBUG
		std::cout << "init" << std::endl;
		#endif
		
		if (usr->inMsg->type == protocol::type::Identifier)
		{
			protocol::Identifier *i = static_cast<protocol::Identifier*>(usr->inMsg);
			bool str = (
				memcmp(i->identifier,
					protocol::identifier_string,
					protocol::identifier_size
				) == 0);
			bool rev = (i->revision == protocol::revision);
			
			if (!str)
			{
				#ifndef NDEBUG
				std::cerr << "Protocol string mismatch" << std::endl;
				#endif
				uRemove(usr);
				return;
			}
			
			if (!rev)
			{
				#ifndef NDEBUG
				std::cerr << "Protocol revision mismatch" << std::endl;
				std::cerr << "Expected: " << protocol::revision << ", Got: " << i->revision << std::endl;
				#endif
				uRemove(usr);
				return;
			}
			
			if (password == 0)
			{
				std::cout << "Proceed to login" << std::endl;
				// no password set, 
				usr->state = uState::login;
				uSendMsg(usr, msgHostInfo());
			}
			else
			{
				std::cout << "Request password" << std::endl;
				usr->state = uState::login_auth;
				uSendMsg(usr, msgAuth(usr, protocol::Global));
			}
		}
		else
		{
			#ifndef NDEBUG
			std::cerr << "Invalid data from user: "
				<< static_cast<int>(usr->id) << std::endl;
			#endif
			
			uRemove(usr);
		}
		break;
	default:
		assert(!"user state was something strange");
		break;
	}
}

void Server::uSendMsg(User* usr, protocol::Message* msg) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uSendMsg()" << std::endl;
	#endif
	
	assert(usr != 0);
	assert(msg != 0);
	
	size_t len;
	char* buf = msg->serialize(len);
	
	usr->buffers.push( new Buffer(buf, len) );
	usr->buffers.back()->write(len);
	buf = 0;
	
	if (!fIsSet(usr->events, ev.write))
	{
		fSet(usr->events, ev.write);
		ev.modify(usr->sock->fd(), usr->events);
	}
	
	delete msg;
	msg = 0;
}

void Server::uAdd(Socket* sock) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uAdd()" << std::endl;
	#endif
	
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
	#ifndef NDEBUG
	std::cout << "Server::uRemove()" << std::endl;
	#endif
	
	ev.remove(usr->sock->fd(), ev.write|ev.read);
	
	uint8_t id = usr->id;
	freeUserID(usr->id);
	
	int fd = usr->sock->fd();
	#ifndef NDEBUG
	std::cout << "Deleting user.." << std::endl;
	#endif
	delete usr;
	
	#ifndef NDEBUG
	std::cout << "Removing from mappings" << std::endl;
	#endif
	users.erase(fd);
	user_id_map.erase(id);
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
	lsock.reuse(1); // reuse address
	
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
				case EADDRINUSE:
					std::cerr << "address already in use " << std::endl;
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

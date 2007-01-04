/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

#include <ctime>
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
	default_user_mode(protocol::user_mode::None),
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
	
	#if defined( FULL_CLEANUP )
	users.clear();
	#endif // FULL_CLEANUP
}

message_ref Server::msgAuth(user_ref& usr, uint8_t session) throw(std::bad_alloc)
{
	protocol::Authentication* auth = new protocol::Authentication;
	auth->session_id = session;
	
	usr->seed[0] = (rand() % 255) + 1;
	usr->seed[1] = (rand() % 255) + 1;
	usr->seed[2] = (rand() % 255) + 1;
	usr->seed[3] = (rand() % 255) + 1;
	
	memcpy(auth->seed, usr->seed, protocol::password_seed_size);
	return message_ref(auth);
}

message_ref Server::msgHostInfo() throw(std::bad_alloc)
{
	protocol::HostInfo *hostnfo = new protocol::HostInfo;
	
	hostnfo->sessions = session_ids.count();
	hostnfo->sessionLimit = session_limit;
	hostnfo->users = user_ids.count();
	hostnfo->userLimit = user_limit;
	hostnfo->nameLenLimit = name_len_limit;
	hostnfo->maxSubscriptions = max_subscriptions;
	hostnfo->requirements = requirements;
	hostnfo->extensions = extensions;
	
	return message_ref(hostnfo);
}

void Server::uWrite(user_ref& usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uWrite(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	if (!usr->output.data or usr->output.canRead() == 0)
	{
		size_t len=0;
		protocol::Message *msg = boost::get_pointer(usr->queue.front());
		
		uint8_t id = msg->user_id;
		
		while (msg->next != 0)
			msg = msg->next;
		
		msg->user_id = id;
		char* buf = msg->serialize(len);
		
		usr->output.setBuffer(buf, len);
		usr->output.write(len);
		
		usr->queue.pop();
	}
	
	int sb = usr->sock->send(
		usr->output.rpos,
		usr->output.canRead()
	);
	
	std::cout << "Sent " << sb << " bytes.." << std::endl;
	
	if (sb == -1)
	{
		std::cerr << "Error occured while sending to user: "
			<< static_cast<int>(usr->id) << std::endl;
		
		uRemove(usr);
	}
	else if (sb == 0)
	{
		// no data was sent, and no error occured.
	}
	else
	{
		usr->output.read(sb);
		
		// just to ensure we don't need to do anything for it.
		
		if (usr->output.left == 0)
		{
			// remove buffer
			usr->output.rewind();
			
			// remove fd from write list if no buffers left.
			if (usr->queue.empty())
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

void Server::uRead(user_ref usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uRead(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	if (usr->input.canWrite() == 0)
	{
		std::cerr << "Input buffer full, increasing size" << std::endl;
		usr->input.resize(usr->input.size + 8192);
	}
	
	int rb = usr->sock->recv(
		usr->input.wpos,
		usr->input.canWrite()
	);
	
	if (rb == -1)
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
	else if (rb == 0)
	{
		std::cout << "User disconnected!" << std::endl;
		
		uRemove(usr);
		return;
	}
	else
	{
		std::cout << "Received " << rb << " bytes.." << std::endl;
		
		usr->input.write(rb);
		
		uProcessData(usr);
	}
}

void Server::uProcessData(user_ref& usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uProcessData(user: "
		<< static_cast<int>(usr->id) << ", in buffer: "
		<< usr->input.left << " bytes)" << std::endl;
	#endif
	
	while (usr->input.canRead() != 0)
	{
		if (!usr->inMsg)
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
		else if (usr->inMsg->type != usr->input.rpos[0])
		{
			// Input buffer frelled
			std::cerr << "Buffer corruption, message type changed." << std::endl;
			uRemove(usr);
			return;
		}
		
		size_t have = usr->input.canRead();
		
		std::cout << "Verify size... ";
		size_t len = usr->inMsg->reqDataLen(usr->input.rpos, have);
		if (len > have)
		{
			std::cout << "Need " << (len - have) << " bytes more." << std::endl;
			// still need more data
			return;
		}
		std::cout << "complete." << std::endl;
		
		std::cout << "Unserialize message" << std::endl;
		usr->input.read(
			usr->inMsg->unserialize(usr->input.rpos, have)
		);
		
		// rewind circular buffer if there's no more data in it.
		if (usr->input.left == 0)
			usr->input.rewind();
		
		switch (usr->state)
		{
		case uState::active:
			uHandleMsg(usr);
			break;
		default:
			uHandleLogin(usr);
			break;
		}
		
		if (usr->inMsg)
		{
			delete usr->inMsg;
			usr->inMsg = 0;
		}
	}
}

message_ref Server::uCreateEvent(user_ref& usr, session_ref session, uint8_t event)
{
	#ifndef NDEBUG
	std::cout << "Server::uCreateEvent(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	protocol::UserInfo *uevent = new protocol::UserInfo;
	
	uevent->user_id = usr->id;
	
	uevent->session_id = session->id;
	
	uevent->event = event;
	uevent->mode = session->mode;
	
	uevent->length = usr->nlen;
	
	uevent->name = new char[usr->nlen];
	memcpy(uevent->name, usr->name, usr->nlen);
	
	return message_ref(uevent);
}

void Server::uHandleMsg(user_ref& usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	
	assert(usr->state == uState::active);
	assert(usr->inMsg != 0);
	
	// TODO
	switch (usr->inMsg->type)
	{
	case protocol::type::ToolInfo:
	case protocol::type::StrokeInfo:
	case protocol::type::StrokeEnd:
		// handle all message with 'selected' modifier the same
		if (usr->inMsg->user_id != protocol::null_user)
		{
			std::cerr << "Client attempts to impersonate someone." << std::endl;
			uRemove(usr);
			break;
		}
		
		{
			// make sure the user id is correct
			usr->inMsg->user_id = usr->id;
			
			Propagate(
				usr->session,
				message_ref(usr->inMsg)
			);
			
			usr->inMsg = 0;
		}
		break;
	case protocol::type::Unsubscribe:
		std::cout << "Unsubscribe" << std::endl;
		{
			protocol::Unsubscribe *m = static_cast<protocol::Unsubscribe*>(usr->inMsg);
			
			std::map<uint8_t, session_ref>::iterator i(session_id_map.find(m->session_id));
			
			if (i == session_id_map.end())
			{
				std::cerr << "No such session: "
					<< static_cast<int>(m->session_id) << std::endl;
				
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::UnknownSession;
				uSendMsg(usr, message_ref(errmsg));
			}
			else
			{
				protocol::Acknowledgement *ack = new protocol::Acknowledgement;
				ack->event = protocol::type::Unsubscribe;
				uSendMsg(usr, message_ref(ack));
				
				uLeaveSession(usr, i->second);
			}
		}
		break;
	case protocol::type::Subscribe:
		std::cout << "Subscribe" << std::endl;
		{
			protocol::Subscribe *m = static_cast<protocol::Subscribe*>(usr->inMsg);
			
			std::map<uint8_t, session_ref>::iterator i(session_id_map.find(m->session_id));
			if (i == session_id_map.end())
			{
				std::cerr << "No such session: "
					<< static_cast<int>(m->session_id) << std::endl;
				
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::UnknownSession;
				uSendMsg(usr, message_ref(errmsg));
			}
			else
			{
				// Ack to user
				protocol::Acknowledgement *ack = new protocol::Acknowledgement;
				ack->event = protocol::type::Subscribe;
				uSendMsg(usr, message_ref(ack));
				
				uJoinSession(usr, i->second);
			}
		}
		break;
	case protocol::type::ListSessions:
		std::cout << "List Sessions" << std::endl;
		{
			if (session_id_map.size() != 0)
			{
				protocol::SessionInfo *nfo = 0;
				std::map<uint8_t, session_ref>::iterator si(session_id_map.begin());
				for (; si != session_id_map.end(); si++)
				{
					nfo = new protocol::SessionInfo;
					
					nfo->session_id = si->first;
					
					nfo->width = si->second->width;
					nfo->height = si->second->height;
					
					nfo->owner = si->second->owner;
					nfo->users = si->second->users.size();
					nfo->limit = si->second->limit;
					nfo->mode = si->second->mode;
					nfo->length = si->second->len;
					
					nfo->title = new char[si->second->len];
					memcpy(nfo->title, si->second->title, si->second->len);
					
					uSendMsg(usr, message_ref(nfo));
				}
			}
			
			protocol::Acknowledgement *ack = new protocol::Acknowledgement;
			ack->event = protocol::type::ListSessions;
			uSendMsg(usr, message_ref(ack));
		}
		break;
	case protocol::type::Instruction:
		std::cout << "Instruction" << std::endl;
		uHandleInstruction(usr);
		break;
	default:
		std::cerr << "Unexpected or unknown message type" << std::endl;
		uRemove(usr);
		break;
	}
}

void Server::uHandleInstruction(user_ref& usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleInstruction()" << std::endl;
	#endif
	
	//assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::type::Instruction);
	
	if (!fIsSet(usr->mode, protocol::user_mode::Administrator))
	{
		std::cerr << "Non-admin tries to pass instructions" << std::endl;
		uRemove(usr);
		return;
	}
	
	protocol::Instruction* m = static_cast<protocol::Instruction*>(usr->inMsg);
	
	switch (m->command)
	{
	case protocol::admin::command::Create:
		{
			uint8_t session_id = getSessionID();
			
			if (session_id == protocol::Global)
			{
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::SessionLimit;
				uSendMsg(usr, message_ref(errmsg));
				return;
			}
			
			session_ref s(new Session);
			s->id = session_id;
			s->limit = m->aux_data;
			s->mode = m->aux_data2;
			
			if (fIsSet(s->mode, protocol::user_mode::Administrator))
			{
				std::cerr << "Administrator flag in default mode." << std::endl;
				uRemove(usr);
				return;
			}
			
			if (s->limit < 2)
			{
				std::cerr << "Attempted to create single user session." << std::endl;
				//delete s;
				//s.reset();
				protocol::Error* errmsg = new protocol::Error;
				errmsg->code = protocol::error::InvalidData;
				uSendMsg(usr, message_ref(errmsg));
				return;
			}
			
			size_t crop = sizeof(s->width) + sizeof(s->height);
			if (m->length < crop)
			{
				std::cerr << "Less data than required" << std::endl;
				uRemove(usr);
				//delete s;
				//s.reset();
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
				uSendMsg(usr, message_ref(errmsg));
				return;
			}
			
			if (m->length > crop)
			{
				s->len = (m->length - crop);
				s->title = new char[s->len];
				memcpy(s->title, m->data+crop, s->len);
			}
			else
			{
				#ifndef NDEBUG
				std::cout << "No title set for session."
				#endif
			}
			
			if (!validateSessionTitle(s))
			{
				std::cerr << "Title not unique." << std::endl;
				
				#if 0
				protocol::Error *err = new protocol::Error;
				err->code = protocol::error::NotUnique;
				uSendMsg(usr, message_ref(err));
				#endif // 0
				
				return;
			}
			
			s->owner = usr->id;
			
			// TODO
			//registerSession(s);
			session_id_map.insert( std::make_pair(s->id, s) );
			
			std::cout << "Session created: " << static_cast<int>(s->id) << std::endl
				<< "With dimensions: " << s->width << " x " << s->height << std::endl;
			
			protocol::Acknowledgement *ack = new protocol::Acknowledgement;
			ack->event = protocol::type::Instruction;
			uSendMsg(usr, message_ref(ack));
		}
		break;
	case protocol::admin::command::Destroy:
		// TODO
		break;
	case protocol::admin::command::Alter:
		// TODO
		break;
	case protocol::admin::command::Password:
		if (m->session_id == protocol::Global)
		{
			password = m->data;
			pw_len = m->length;
			
			m->data = 0;
			m->length = 0;
			
			protocol::Acknowledgement *ack = new protocol::Acknowledgement;
			ack->event = protocol::type::Instruction;
			uSendMsg(usr, message_ref(ack));
		}
		else
		{
			// TODO: Behaviour for setting session password
		}
		break;
	default:
		std::cerr << "Unrecognized command: "
			<< static_cast<int>(m->command) << std::endl;
		break;
	}
}

void Server::uHandleLogin(user_ref& usr) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uHandleLogin(user: " << static_cast<int>(usr->id)
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
				
				#if 0
				protocol::Error *err = new protocol::Error;
				err->code = protocol::error::TooLong;
				uSendMsg(usr, message_ref(err));
				#endif // 0
				
				uRemove(usr);
				return;
			}
			
			if (!validateUserName(usr))
			{
				std::cerr << "Name not unique." << std::endl;
				
				#if 0
				protocol::Error *err = new protocol::Error;
				err->code = protocol::error::NotUnique;
				uSendMsg(usr, message_ref(err));
				#endif // 0
				
				uRemove(usr);
				return;
			}
			
			// assign message the user's id
			m->user_id = usr->id;
			
			// assign user their own name
			usr->name = m->name;
			usr->nlen = m->length;
			
			// null the message's name information, so they don't get deleted
			m->length = 0;
			m->name = 0;
			
			if (localhost_admin && usr->sock->address() == INADDR_LOOPBACK)
			{
				// auto admin promotion.
				// also, don't put any other flags on the user.
				usr->mode = protocol::user_mode::Administrator;
			}
			else
			{
				// set user mode
				usr->mode = default_user_mode;
			}
			m->mode = usr->mode;
			
			// reply
			uSendMsg(usr, message_ref(m));
			
			usr->state = uState::active;
			
			usr->inMsg = 0;
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
			protocol::Password *m = static_cast<protocol::Password*>(usr->inMsg);
			
			CSHA1 h;
			h.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			h.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			h.Final();
			char digest[protocol::password_hash_size];
			h.GetHash(reinterpret_cast<uint8_t*>(digest));
			
			if (memcmp(digest, m->data, protocol::password_hash_size) != 0)
			{
				// mismatch, send error or disconnect.
				uRemove(usr);
				return;
			}
			
			protocol::Acknowledgement *ack = new protocol::Acknowledgement;
			ack->event = protocol::type::Password;
			uSendMsg(usr, message_ref(ack));
		}
		else
		{
			uRemove(usr);
			return;
		}
		
		usr->state = uState::login;
		uSendMsg(usr, message_ref(msgHostInfo()));
		
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
				uSendMsg(usr, message_ref(msgHostInfo()));
			}
			else
			{
				std::cout << "Request password" << std::endl;
				usr->state = uState::login_auth;
				uSendMsg(usr, message_ref(msgAuth(usr, protocol::Global)));
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

void Server::Propagate(uint8_t session_id, message_ref msg) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::Propagate(session: " << static_cast<int>(session_id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	#endif
	
	const std::map<uint8_t, session_ref>::iterator si(session_id_map.find(session_id));
	if (si == session_id_map.end())
	{
		std::cerr << "No such session!" << std::endl;
		return;
	}
	
	std::map<uint8_t, user_ref>::iterator ui( si->second->users.begin() );
	for (; ui != si->second->users.end(); ui++)
	{
		// TODO: Somehow prevent some messages from being propagated back to originator
		
		uSendMsg(ui->second, msg);
	}
}

void Server::uSendMsg(user_ref& usr, message_ref msg) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uSendMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	protocol::msgName(msg->type);
	#endif
	
	usr->queue.push( msg );
	
	if (!fIsSet(usr->events, ev.write))
	{
		fSet(usr->events, ev.write);
		ev.modify(usr->sock->fd(), usr->events);
	}
}

void Server::uSyncSession(user_ref& usr, session_ref& session) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uSyncSession(user: " << static_cast<int>(usr->id)
		<< ", session: " << static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	// TODO: Pick user for requesting raster from, and send raster request.
	
	user_ref src_usr;
	
	protocol::Synchronize *s = new protocol::Synchronize;
	s->session_id = session->id;
	uSendMsg(src_usr, message_ref(s));
	
	// create fake tunnel
	session->tunnel.insert( std::make_pair(usr->id, src_usr->id) );
	
	// This is WRONG
	protocol::Raster *raster = new protocol::Raster;
	raster->session_id = session->id;
	uSendMsg(usr, message_ref(raster));
}

void Server::uJoinSession(user_ref& usr, session_ref& session) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uJoinSession()" << std::endl;
	#endif
	
	if (session->users.size() > 0)
	{
		//uGetRaster(session);
	}
	
	session->users.insert( std::make_pair(usr->id, usr) );
	
	// Add session to users session list.
	usr->sessions.insert(
		std::make_pair( session->id, SessionData(usr->id, session) )
	);
	
	// Tell session members there's a new user.
	Propagate(session->id, message_ref(uCreateEvent(usr, session, protocol::user_event::Join)));
	
	// Tell the new user of the already existing users.
	std::map<uint8_t, user_ref>::iterator si(session->users.begin());
	for (; si != session->users.end(); si++)
	{
		uSendMsg(usr, uCreateEvent(si->second, session, protocol::user_event::Join));
	}
	
	// set user's active session
	usr->session = session->id;
	
	// Announce active session
	message_ref msg_ref(new protocol::SessionSelect);
	msg_ref->user_id = usr->id;
	msg_ref->session_id = session->id;
	Propagate(session->id, msg_ref);
	// TODO: Propagate to all users who see this user
	
	// Start client sync
	uSyncSession(usr, session);
}

void Server::uLeaveSession(user_ref& usr, session_ref& session) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uLeaveSession(user: "
		<< static_cast<int>(usr->id) << ", session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	// TODO: Cancel any pending messages related to this user.
	
	session->users.erase(usr->id);
	
	// remove
	usr->sessions.erase(session->id);
	
	// last user in session.. destruct it
	if (session->users.size() == 0)
	{
		freeSessionID(session->id);
		
		session_id_map.erase(session->id);
		
		return;
	}
	
	// Tell session members the user left
	Propagate(session->id, uCreateEvent(usr, session, protocol::user_event::Leave));
	
	if (session->owner == usr->id)
	{
		session->owner = protocol::null_user;
		
		// TODO: Announce owner disappearance.. or not
	}
}

void Server::uAdd(Socket* sock) throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::uAdd()" << std::endl;
	#endif
	
	if (sock == 0)
	{
		#ifndef NDEBUG
		std::cout << "Null socket, aborting." << std::endl;
		#endif
		
		return;
	}
	
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
		
		user_ref usr( new User(id, sock) );
		
		usr->session = protocol::Global;
		
		fSet(usr->events, ev.read);
		ev.add(usr->sock->fd(), usr->events);
		
		users.insert( std::make_pair(sock->fd(), usr) );
		//user_id_map.insert( std::make_pair(id, usr) );
		
		if (usr->input.data == 0)
		{
			#ifndef NDEBUG
			std::cout << "Assigning buffer to user." << std::endl;
			#endif
			
			usr->input.setBuffer(new char[8192], 8192);
		}
		
		#ifndef NDEBUG
		std::cout << "Known users: " << users.size() << std::endl;
		#endif
	}
}

void Server::uRemove(user_ref& usr) throw()
{
	#ifndef NDEBUG
	std::cout << "Server::uRemove()" << std::endl;
	#endif
	
	ev.remove(usr->sock->fd(), ev.write|ev.read|ev.error|ev.hangup);
	
	freeUserID(usr->id);
	
	std::map<uint8_t, SessionData>::iterator si( usr->sessions.begin() );
	for (; si != usr->sessions.end(); si++ )
	{
		//si->second.session->users.erase(usr->id);
		uLeaveSession(usr, si->second.session);
	}
	
	#ifndef NDEBUG
	std::cout << "Removing from mappings" << std::endl;
	#endif
	users.erase(usr->sock->fd());
	//user_id_map.erase(usr->id);
	
	#if 0
	#ifndef NDEBUG
	std::cout << "Still in use in " << usr.use_count() << " place/s." << std::endl;
	#endif
	
	usr.reset();
	#endif
}

int Server::init() throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::init()" << std::endl;
	#endif
	
	srand(time(0) - 513); // FIXME
	
	#ifndef NDEBUG
	std::cout << "creating socket" << std::endl;
	#endif
	
	lsock.fd(socket(AF_INET, SOCK_STREAM, 0));
	#ifndef NDEBUG
	std::cout << "New socket: " << lsock.fd() << std::endl;
	#endif
	
	if (lsock.fd() == INVALID_SOCKET)
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
	
	// user map iterator
	std::map<int, user_ref>::iterator ui;
	
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
			return -1;
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
				
				uAdd( lsock.accept() );
			}
			
			if (ec > 0)
			{
				for (ui = users.begin(); ui != users.end(); ui++)
				{
					#if 0
					std::cout << "Testing: " << ui->first << std::endl;
					#endif
					if (ev.isset(ui->first, ev.read))
					{
						#ifndef NDEBUG
						std::cout << "Reading from client" << std::endl;
						#endif
						
						uRead(ui->second);
						
						if (--ec == 0) break;
					}
					if (ec != 0 && ev.isset(ui->first, ev.write))
					{
						#ifndef NDEBUG
						std::cout << "Writing to client" << std::endl;
						#endif
						
						uWrite(ui->second);
						
						if (--ec == 0) break;
					}
					if (ec != 0 && ev.isset(ui->first, ev.error))
					{
						#ifndef NDEBUG
						std::cout << "Error with client" << std::endl;
						#endif
						
						uRemove(ui->second);
						
						if (--ec == 0) break;
					}
					#ifdef EV_HAS_HANGUP
					if (ec != 0 && ev.isset(ui->first, ev.hangup))
					{
						#ifndef NDEBUG
						std::cout << "Client hung up" << std::endl;
						#endif
						
						uRemove(ui->second);
						
						if (--ec == 0) break;
					}
					#endif
					
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

bool Server::validateUserName(user_ref& usr) const throw()
{
	return true;
}

bool Server::validateSessionTitle(session_ref& session) const throw()
{
	return true;
}

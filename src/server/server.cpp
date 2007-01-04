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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::Server()" << std::endl;
	#endif
	#endif
}

Server::~Server() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::~Server()" << std::endl;
	#endif
	#endif
	
	cleanup();
}

uint8_t Server::getUserID() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::getUserID()" << std::endl;
	#endif
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::getSessionID()" << std::endl;
	#endif
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
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::freeUserID(" << static_cast<int>(id) << ")" << std::endl;
	#endif
	#endif
	
	user_ids.set(id, false);
}

void Server::freeSessionID(uint8_t id) throw()
{
	assert(session_ids.test(id) == true);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::freeSessionID(" << static_cast<int>(id) << ")" << std::endl;
	#endif
	#endif
	
	session_ids.set(id, false);
}

void Server::cleanup() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::cleanup()" << std::endl;
	#endif
	#endif
	
	// finish event system
	ev.finish();
	
	// close listening socket
	lsock.close();
	
	#if defined( FULL_CLEANUP )
	users.clear();
	#endif // FULL_CLEANUP
}

message_ref Server::msgAuth(user_ref& usr, uint8_t session) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgAuth()" << std::endl;
	#endif
	#endif
	
	protocol::Authentication* auth = new protocol::Authentication;
	auth->session_id = session;
	
	usr->seed[0] = (rand() % 255) + 1;
	usr->seed[1] = (rand() % 255) + 1;
	usr->seed[2] = (rand() % 255) + 1;
	usr->seed[3] = (rand() % 255) + 1;
	
	memcpy(auth->seed, usr->seed, protocol::password_seed_size);
	return message_ref(auth);
}

message_ref Server::msgHostInfo() const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgHostInfo()" << std::endl;
	#endif
	#endif
	
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

message_ref Server::msgError(uint16_t code) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgError()" << std::endl;
	#endif
	#endif
	
	protocol::Error *err = new protocol::Error;
	err->code = code;
	return message_ref(err);
}

message_ref Server::msgAck(uint8_t type) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgAck()" << std::endl;
	#endif
	#endif
	
	protocol::Acknowledgement *ack = new protocol::Acknowledgement;
	ack->event = type;
	return message_ref(ack);
}

message_ref Server::msgSyncWait(session_ref session) const throw(std::bad_alloc)
{
	protocol::SyncWait *sync = new protocol::SyncWait;
	sync->session_id = session->id;
	return message_ref(sync);
}

void Server::uWrite(user_ref& usr) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uWrite(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
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
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Sent " << sb << " bytes.." << std::endl;
	#endif
	#endif
	
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uRead(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	#endif
	
	if (usr->input.canWrite() == 0)
	{
		#ifndef NDEBUG
		std::cerr << "Input buffer full, increasing size" << std::endl;
		#endif
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
		#ifndef NDEBUG
		std::cerr << "User disconnected!" << std::endl;
		#endif
		
		uRemove(usr);
		return;
	}
	else
	{
		#ifndef NDEBUG
		std::cout << "Received " << rb << " bytes.." << std::endl;
		#endif
		
		usr->input.write(rb);
		
		uProcessData(usr);
	}
}

void Server::uProcessData(user_ref& usr) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uProcessData(user: "
		<< static_cast<int>(usr->id) << ", in buffer: "
		<< usr->input.left << " bytes)" << std::endl;
	#endif
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
		
		#ifndef NDEBUG
		std::cout << "Verify size... ";
		#endif
		size_t len = usr->inMsg->reqDataLen(usr->input.rpos, have);
		if (len > have)
		{
			#ifndef NDEBUG
			std::cout << "Need " << (len - have) << " bytes more." << std::endl;
			#endif
			// still need more data
			return;
		}
		#ifndef NDEBUG
		std::cout << "complete." << std::endl;
		
		std::cout << "Unserialize message" << std::endl;
		#endif
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

message_ref Server::uCreateEvent(user_ref& usr, session_ref session, uint8_t event) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uCreateEvent(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
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
		#if 0
		if (usr->inMsg->user_id != protocol::null_user)
		{
			std::cerr << "Client attempted to impersonate someone." << std::endl;
			uRemove(usr);
			break;
		}
		#endif // 0
		
		// make sure the user id is correct
		usr->inMsg->user_id = usr->id;
		
		Propagate(
			usr->session,
			message_ref(usr->inMsg)
		);
		
		usr->inMsg = 0;
		break;
	case protocol::type::UserInfo:
		// TODO!!!
		break;
	case protocol::type::Unsubscribe:
		#ifdef DEBUG_SERVER
		#ifdef NDEBUG
		std::cout << "Unsubscribe" << std::endl;
		#endif
		#endif
		{
			protocol::Unsubscribe *msg = static_cast<protocol::Unsubscribe*>(usr->inMsg);
			
			std::map<uint8_t, session_ref>::iterator si(session_id_map.find(msg->session_id));
			
			if (si == session_id_map.end())
			{
				#ifdef NDEBUG
				std::cerr << "No such session: "
					<< static_cast<int>(msg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(protocol::error::UnknownSession));
			}
			else
			{
				uSendMsg(usr, msgAck(protocol::type::Unsubscribe));
				
				uLeaveSession(usr, si->second);
			}
		}
		break;
	case protocol::type::Subscribe:
		#ifdef NDEBUG
		std::cout << "Subscribe" << std::endl;
		#endif
		{
			protocol::Subscribe *msg = static_cast<protocol::Subscribe*>(usr->inMsg);
			
			std::map<uint8_t, session_ref>::iterator si(session_id_map.find(msg->session_id));
			if (si == session_id_map.end())
			{
				#ifndef NDEBUG
				std::cerr << "No such session: "
					<< static_cast<int>(msg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(protocol::error::UnknownSession));
			}
			else
			{
				uSendMsg(usr, msgAck(protocol::type::Subscribe));
				uJoinSession(usr, si->second);
			}
		}
		break;
	case protocol::type::ListSessions:
		#ifndef NDEBUG
		std::cout << "List Sessions" << std::endl;
		#endif
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
		
		uSendMsg(usr, msgAck(protocol::type::ListSessions));
		break;
	case protocol::type::Instruction:
		#ifndef NDEBUG
		std::cout << "Instruction" << std::endl;
		#endif
		uHandleInstruction(usr);
		break;
	default:
		std::cerr << "Unexpected or unknown message type." << std::endl;
		uRemove(usr);
		break;
	}
}

void Server::uHandleInstruction(user_ref& usr) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleInstruction()" << std::endl;
	#endif
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
	
	protocol::Instruction* msg = static_cast<protocol::Instruction*>(usr->inMsg);
	
	switch (msg->command)
	{
	case protocol::admin::command::Create:
		{
			uint8_t session_id = getSessionID();
			
			if (session_id == protocol::Global)
			{
				uSendMsg(usr, msgError(protocol::error::SessionLimit));
				return;
			}
			
			session_ref session(new Session);
			session->id = session_id;
			session->limit = msg->aux_data;
			session->mode = msg->aux_data2;
			
			if (fIsSet(session->mode, protocol::user_mode::Administrator))
			{
				std::cerr << "Administrator flag in default mode." << std::endl;
				uRemove(usr);
				return;
			}
			
			if (session->limit < 2)
			{
				#ifndef NDEBUG
				std::cerr << "Attempted to create single user session." << std::endl;
				#endif
				
				uSendMsg(usr, msgError(protocol::error::InvalidData));
				return;
			}
			
			size_t crop = sizeof(session->width) + sizeof(session->height);
			if (msg->length < crop)
			{
				#ifdef NDEBUG
				std::cerr << "Less data than required" << std::endl;
				#endif
				
				uRemove(usr);
				return;
			}
			
			memcpy_t(session->width, msg->data);
			memcpy_t(session->height, msg->data+sizeof(session->width));
			
			bswap(session->width);
			bswap(session->height);
			
			if (session->width < min_dimension or session->height < min_dimension)
			{
				uSendMsg(usr, msgError(protocol::error::TooSmall));
				return;
			}
			
			if (msg->length > crop)
			{
				session->len = (msg->length - crop);
				session->title = new char[session->len];
				memcpy(session->title, msg->data+crop, session->len);
			}
			else
			{
				#ifndef NDEBUG
				std::cerr << "No title set for session." << std::endl;
				#endif
			}
			
			if (!validateSessionTitle(session))
			{
				#ifndef NDEBUG
				std::cerr << "Title not unique." << std::endl;
				#endif
				
				#if 0
				uSendMsg(usr, msgError(protocol::error::NotUnique));
				#endif // 0
				
				return;
			}
			
			session->owner = usr->id;
			
			// TODO
			//registerSession(s);
			session_id_map.insert( std::make_pair(session->id, session) );
			
			#ifndef NDEBUG
			std::cout << "Session created: " << static_cast<int>(session->id) << std::endl
				<< "With dimensions: " << session->width << " x " << session->height << std::endl;
			#endif
			
			uSendMsg(usr, msgAck(protocol::type::Instruction));
		}
		break;
	case protocol::admin::command::Destroy:
		// TODO
		break;
	case protocol::admin::command::Alter:
		// TODO
		break;
	case protocol::admin::command::Password:
		if (msg->session_id == protocol::Global)
		{
			password = msg->data;
			pw_len = msg->length;
			
			msg->data = 0;
			msg->length = 0;
			
			uSendMsg(usr, msgAck(protocol::type::Instruction));
		}
		else
		{
			// TODO: Behaviour for setting session password
		}
		break;
	default:
		std::cerr << "Unrecognized command: "
			<< static_cast<int>(msg->command) << std::endl;
		break;
	}
}

void Server::uHandleLogin(user_ref& usr) throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleLogin(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	#endif
	
	switch (usr->state)
	{
	case uState::login:
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "login" << std::endl;
		#endif
		#endif
		
		if (usr->inMsg->type == protocol::type::UserInfo)
		{
			protocol::UserInfo *msg = static_cast<protocol::UserInfo*>(usr->inMsg);
			
			if (msg->user_id != protocol::null_user)
			{
				#ifndef NDEBUG
				std::cerr << "User made blasphemous assumption." << std::endl;
				#endif
				
				uRemove(usr);
				return;
			}
			
			if (msg->session_id != protocol::Global)
			{
				#ifndef NDEBUG
				std::cerr << "Wrong session identifier." << std::endl;
				#endif
				
				uRemove(usr);
				return;
			}
			
			if (msg->event != protocol::user_event::Login)
			{
				#ifndef NDEBUG
				std::cerr << "Wrong user event." << std::endl;
				#endif
				
				uRemove(usr);
				return;
			}
			
			if (msg->length > name_len_limit)
			{
				#ifndef NDEBUG
				std::cerr << "Name too long." << std::endl;
				#endif
				
				#if 0
				uSendMsg(usr, msgError(protocol::error::TooLong));
				#endif // 0
				
				uRemove(usr);
				return;
			}
			
			if (!validateUserName(usr))
			{
				#ifndef NDEBUG
				std::cerr << "Name not unique." << std::endl;
				#endif
				
				#if 0
				uSendMsg(usr, msgError(protocol::error::NotUnique));
				#endif // 0
				
				uRemove(usr);
				return;
			}
			
			// assign message the user's id
			msg->user_id = usr->id;
			
			// assign user their own name
			usr->name = msg->name;
			usr->nlen = msg->length;
			
			// null the message's name information, so they don't get deleted
			msg->length = 0;
			msg->name = 0;
			
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
			msg->mode = usr->mode;
			
			// reply
			uSendMsg(usr, message_ref(msg));
			
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
			protocol::Password *msg = static_cast<protocol::Password*>(usr->inMsg);
			
			CSHA1 hash;
			hash.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			
			if (memcmp(digest, msg->data, protocol::password_hash_size) != 0)
			{
				// mismatch, send error or disconnect.
				uRemove(usr);
				return;
			}
			
			uSendMsg(usr, msgAck(protocol::type::Password));
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
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "init" << std::endl;
		#endif
		#endif
		
		if (usr->inMsg->type == protocol::type::Identifier)
		{
			protocol::Identifier *ident = static_cast<protocol::Identifier*>(usr->inMsg);
			bool str = (
				memcmp(ident->identifier,
					protocol::identifier_string,
					protocol::identifier_size
				) == 0);
			bool rev = (ident->revision == protocol::revision);
			
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
				std::cerr << "Expected: " << protocol::revision
					<< ", Got: " << ident->revision << std::endl;
				#endif
				
				uRemove(usr);
				return;
			}
			
			if (password == 0)
			{
				#ifndef NDEBUG
				std::cout << "Proceed to login" << std::endl;
				#endif
				
				// no password set
				
				usr->state = uState::login;
				uSendMsg(usr, message_ref(msgHostInfo()));
			}
			else
			{
				#ifndef NDEBUG
				std::cout << "Request password" << std::endl;
				#endif
				
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::Propagate(session: " << static_cast<int>(session_id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	#endif
	#endif
	
	const std::map<uint8_t, session_ref>::iterator si(session_id_map.find(session_id));
	if (si == session_id_map.end())
	{
		#ifndef NDEBUG
		std::cerr << "No such session!" << std::endl;
		#endif
		
		return;
	}
	
	std::map<uint8_t, user_ref>::iterator ui( si->second->users.begin() );
	for (; ui != si->second->users.end(); ui++)
	{
		uSendMsg(ui->second, msg);
	}
}

void Server::lPropagate(uint8_t session_id, message_ref msg, user_ref usr) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::lPropagate(session: " << static_cast<int>(session_id)
		<< ", type: " << static_cast<int>(msg->type) << ", not to: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	#endif
	
	const std::map<uint8_t, session_ref>::iterator si(session_id_map.find(session_id));
	if (si == session_id_map.end())
	{
		#ifndef NDEBUG
		std::cerr << "No such session!" << std::endl;
		#endif
		
		return;
	}
	
	std::map<uint8_t, user_ref>::iterator ui( si->second->users.begin() );
	for (; ui != si->second->users.end(); ui++)
	{
		if (ui->first != usr->id)
			uSendMsg(ui->second, msg);
	}
}

void Server::uSendMsg(user_ref& usr, message_ref msg) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uSendMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	protocol::msgName(msg->type);
	#endif
	#endif
	
	usr->queue.push( msg );
	
	if (!fIsSet(usr->events, ev.write))
	{
		fSet(usr->events, ev.write);
		ev.modify(usr->sock->fd(), usr->events);
	}
}

#if 0
void Server::uSyncSession(user_ref& usr, session_ref& session) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uSyncSession(user: " << static_cast<int>(usr->id)
		<< ", session: " << static_cast<int>(session->id) << ")" << std::endl;
	#endif
	#endif
	
	if (session->users.size() == 0)
	{
	}
	else
	{
		// find source for raster.
		user_ref src_usr;
		std::map<uint8_t, user_ref>::iterator ui(session->users.begin());
		for (; ui != session->users.end(); ui++)
		{
			if (ui->second->id != usr->id)
				src_usr = ui->second;
		}
		
		// send raster request to users (NOT HERE DAMNIT! AFTER ACK/SYNC!!!)
		protocol::Synchronize *sync = new protocol::Synchronize;
		sync->session_id = session->id;
		uSendMsg(src_usr, message_ref(sync));
		// create fake tunnel
		tunnel.insert( std::make_pair(src_usr->id, usr->id) );
	}
}
#endif // 0

void Server::uJoinSession(user_ref& usr, session_ref& session) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uJoinSession()" << std::endl;
	#endif
	#endif
	
	// Add session to users session list.
	usr->sessions.insert(
		std::make_pair( session->id, SessionData(usr->id, session) )
	);
	
	// set user's active session
	usr->session = session->id;
	
	// Tell session members there's a new user.
	Propagate(session->id, message_ref(uCreateEvent(usr, session, protocol::user_event::Join)));
	
	if (session->users.size() != 0)
	{
		// put user to wait sync list.
		session->waitingSync.push( usr );
		
		// Tell the new user of the already existing users.
		std::map<uint8_t, user_ref>::iterator si(session->users.begin());
		for (; si != session->users.end(); si++)
		{
			uSendMsg(usr, uCreateEvent(si->second, session, protocol::user_event::Join));
		}
		
		// don't start new client sync if one is already in progress...
		if (!session->syncing)
		{
			// Put session to syncing state
			session->syncing = true;
			
			// tell session users to enter syncwait state.
			Propagate(session->id, msgSyncWait(session));
		}
	}
	else
	{
		// session is empty
		
		session->users.insert( std::make_pair(usr->id, usr) );
		
		protocol::Raster *raster = new protocol::Raster;
		raster->session_id = session->id;
		uSendMsg(usr, message_ref(raster));
	}
}

void Server::uLeaveSession(user_ref& usr, session_ref& session) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uLeaveSession(user: "
		<< static_cast<int>(usr->id) << ", session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uAdd()" << std::endl;
	#endif
	#endif
	
	if (sock == 0)
	{
		#ifndef NDEBUG
		std::cout << "Null socket, aborting user creation." << std::endl;
		#endif
		
		return;
	}
	
	uint8_t id = getUserID();
	
	if (id == 0)
	{
		//#ifndef NDEBUG
		std::cout << "Server is full, kicking user." << std::endl;
		//#endif
		
		delete sock;
		return;
	}
	else
	{
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "New user: " << static_cast<uint32_t>(id) << std::endl;
		#endif
		#endif
		
		user_ref usr( new User(id, sock) );
		
		usr->session = protocol::Global;
		
		fSet(usr->events, ev.read);
		ev.add(usr->sock->fd(), usr->events);
		
		users.insert( std::make_pair(sock->fd(), usr) );
		//user_id_map.insert( std::make_pair(id, usr) );
		
		if (usr->input.data == 0)
		{
			#ifdef DEBUG_BUFFER
			#ifndef NDEBUG
			std::cout << "Assigning buffer to user." << std::endl;
			#endif
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
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uRemove()" << std::endl;
	#endif
	#endif
	
	ev.remove(usr->sock->fd(), ev.write|ev.read|ev.error|ev.hangup);
	
	freeUserID(usr->id);
	
	// clear the fake tunnel of any possible instance of this user.
	if ((usr->sessions.size() != 0) and (tunnel.size() != 0))
	{
		std::map<uint8_t,uint8_t>::iterator ti(tunnel.begin());
		for (; ti != tunnel.end(); ti++)
		{
			if (ti->second == usr->id)
			{
				tunnel.erase(ti->first);
				break;
			}
		}
	}
	
	std::map<uint8_t, SessionData>::iterator si( usr->sessions.begin() );
	for (; si != usr->sessions.end(); si++ )
	{
		//si->second.session->users.erase(usr->id);
		uLeaveSession(usr, si->second.session);
	}
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Removing from mappings" << std::endl;
	#endif
	#endif
	users.erase(usr->sock->fd());
	//user_id_map.erase(usr->id);
	
	#if 0
	#ifndef NDEBUG
	std::cout << "Still in use in " << usr.use_count() << " place/s." << std::endl;
	#endif
	#endif // 0
}

int Server::init() throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Server::init()" << std::endl;
	#endif
	
	srand(time(0) - 513); // FIXME
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "creating socket" << std::endl;
	#endif
	#endif
	
	lsock.fd(socket(AF_INET, SOCK_STREAM, 0));
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "New socket: " << lsock.fd() << std::endl;
	#endif
	#endif
	
	if (lsock.fd() == INVALID_SOCKET)
	{
		std::cerr << "Failed to create a socket." << std::endl;
		return -1;
	}
	
	lsock.block(0); // nonblocking
	lsock.reuse(1); // reuse address
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "binding socket address" << std::endl;
	#endif
	#endif
	
	bool bound = false;
	for (int bport=lo_port; bport < hi_port+1; bport++)
	{
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "Trying: " << INADDR_ANY << ":" << bport << std::endl;
		#endif
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
				std::cerr << "Address already in use " << std::endl;
				break;
			case EADDRNOTAVAIL:
				std::cerr << "Address not available" << std::endl;
				break;
			case ENOBUFS:
				std::cerr << "Insufficient resources" << std::endl;
				break;
			case EACCES:
				std::cerr << "Can't bind to superuser sockets" << std::endl;
				break;
			default:
				std::cerr << "Unknown error" << std::endl;
				break;
		}
		
		return -1;
	}
	
	if (lsock.listen() == -1)
	{
		std::cerr << "Failed to open listening port." << std::endl;
		return -1;
	}
	
	std::cout << "Listening on: " << lsock.address() << ":" << lsock.port() << std::endl;
	
	if (!ev.init())
		return -1;
	
	ev.add(lsock.fd(), ev.read);
	
	return 0;
}

int Server::run() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::run()" << std::endl;
	#endif
	#endif
	
	// user map iterator
	std::map<int, user_ref>::iterator ui;
	
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
			#ifdef DEBUG_SERVER
			#ifndef NDEBUG
			std::cout << "Events waiting: " << ec << std::endl;
			#endif
			#endif
			
			if (ev.isset(lsock.fd(), ev.read))
			{
				#ifdef DEBUG_SERVER
				#ifndef NDEBUG
				std::cout << "Server socket triggered" << std::endl;
				#endif
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
					#endif // 0
					if (ev.isset(ui->first, ev.read))
					{
						#ifdef DEBUG_SERVER
						#ifndef NDEBUG
						std::cout << "Reading from client" << std::endl;
						#endif
						#endif
						
						uRead(ui->second);
						
						if (--ec == 0) break;
					}
					if (ec != 0 && ev.isset(ui->first, ev.write))
					{
						#ifdef DEBUG_SERVER
						#ifndef NDEBUG
						std::cout << "Writing to client" << std::endl;
						#endif
						#endif
						
						uWrite(ui->second);
						
						if (--ec == 0) break;
					}
					if (ec != 0 && ev.isset(ui->first, ev.error))
					{
						#ifdef DEBUG_SERVER
						#ifndef NDEBUG
						std::cout << "Error with client" << std::endl;
						#endif
						#endif
						
						uRemove(ui->second);
						
						if (--ec == 0) break;
					}
					#ifdef EV_HAS_HANGUP
					if (ec != 0 && ev.isset(ui->first, ev.hangup))
					{
						#ifdef DEBUG_SERVER
						#ifndef NDEBUG
						std::cout << "Client hung up" << std::endl;
						#endif
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
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Done?" << std::endl;
	#endif
	#endif
	
	return 0;
}

bool Server::validateUserName(user_ref& usr) const throw()
{
	#ifndef NDEBUG
	std::cout << "Server::validateUserName(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	return true;
}

bool Server::validateSessionTitle(session_ref& session) const throw()
{
	#ifndef NDEBUG
	std::cout << "Server::validateSessionTitle(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	return true;
}

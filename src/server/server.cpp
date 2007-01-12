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

#include "server.flags.h"

#include <ctime>
#include <getopt.h> // for command-line opts
#include <cstdlib>
#include <iostream>

Server::Server() throw()
	: password(0),
	a_password(0),
	pw_len(0),
	a_pw_len(0),
	user_limit(0),
	session_limit(1),
	max_subscriptions(1),
	name_len_limit(8),
	hi_port(protocol::default_port),
	lo_port(protocol::default_port),
	min_dimension(400),
	requirements(0),
	extensions(protocol::extensions::Chat|protocol::extensions::Palette),
	default_user_mode(protocol::user_mode::None),
	opmode(0)
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
	
	for (int i=1; i != defaults::hard_limit+1; i++)
	{
		if (!user_ids.test(i))
		{
			user_ids.set(i, true);
			return i;
		}
	}
	
	return protocol::null_user;
}

uint8_t Server::getSessionID() throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::getSessionID()" << std::endl;
	#endif
	#endif
	
	for (int i=1; i != defaults::hard_limit+1; i++)
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
	
	assert(user_ids.test(id));
	
	user_ids.set(static_cast<int>(id), false);
}

void Server::freeSessionID(uint8_t id) throw()
{
	assert(session_ids.test(id) == true);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::freeSessionID(" << static_cast<int>(id) << ")" << std::endl;
	#endif
	#endif
	
	assert(session_ids.test(id));
	
	session_ids.set(static_cast<int>(id), false);
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

message_ref Server::msgAuth(User* usr, uint8_t session) const throw(std::bad_alloc)
{
	assert(usr != 0);
	
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

message_ref Server::msgError(uint8_t session, uint16_t code) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgError()" << std::endl;
	#endif
	#endif
	
	protocol::Error *err = new protocol::Error;
	err->code = code;
	err->session_id = session;
	return message_ref(err);
}

message_ref Server::msgAck(uint8_t session, uint8_t type) const throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::msgAck()" << std::endl;
	#endif
	#endif
	
	protocol::Acknowledgement *ack = new protocol::Acknowledgement;
	ack->session_id = session;
	ack->event = type;
	return message_ref(ack);
}

message_ref Server::msgSyncWait(Session* session) const throw(std::bad_alloc)
{
	assert(session != 0);
	
	protocol::SyncWait *sync = new protocol::SyncWait;
	sync->session_id = session->id;
	return message_ref(sync);
}

void Server::uWrite(User*& usr) throw()
{
	assert(usr != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uWrite(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	#endif
	
	if (!usr->output.data or usr->output.canRead() == 0)
	{
		
		// if buffer is null or no data left to read
		protocol::Message *msg = boost::get_pointer(usr->queue.front());
		
		// create outgoing message list
		std::vector<message_ref> outgoing;
		message_ref n;
		while (usr->queue.size() != 0)
		{
			// TODO: Support linked lists for other message types as well..
			n = usr->queue.front();
			if (msg->type != protocol::type::StrokeInfo
				or (n->type != msg->type)
				or (n->user_id != msg->user_id)
				or (n->session_id != msg->session_id))
			{
				break;
			}
			
			outgoing.push_back(n);
			usr->queue.pop_front();
		}
		
		std::vector<message_ref>::iterator mi;
		protocol::Message *last = msg;
		if (outgoing.size() > 1)
		{
			// create linked list
			for (mi = ++(outgoing.begin()); mi != outgoing.end(); mi++)
			{
				msg = boost::get_pointer(*mi);
				msg->prev = last;
				last->next = msg;
				last = msg;
			}
		}
		
		//msg->user_id = id;
		size_t len=0;
		
		// serialize linked list
		char* buf = msg->serialize(len);
		
		usr->output.setBuffer(buf, len);
		usr->output.write(len);
		
		#ifdef DEBUG_LINKED_LIST
		#ifndef NDEBUG
		if (outgoing.size() > 1)
		{
			// user_id and type saved, count as additional header
			std::cout << "Linked " << outgoing.size() << " messages, for total size: " << len << std::endl
				<< "Bandwidth savings are [(7*n) - ((5*n)+3)]: " << (7 * outgoing.size()) - (3 + (outgoing.size() * 5)) << std::endl;
		}
		#endif
		#endif
		
		// clear linked list...
		if (outgoing.size() == 0)
		{
			usr->queue.pop_front();
		}
		else
		{
			for (mi = outgoing.begin(); mi != outgoing.end(); mi++)
			{
				(*mi)->next = 0;
				(*mi)->prev = 0;
			}
		}
	}
	
	int sb = usr->sock->send(
		usr->output.rpos,
		usr->output.canRead()
	);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	//std::cout << "Sent " << sb << " bytes.." << std::endl;
	#endif
	#endif
	
	if (sb == -1)
	{
		std::cerr << "Error occured while sending to user: "
			<< static_cast<int>(usr->id) << std::endl;
		
		uRemove(usr, protocol::user_event::BrokenPipe);
		return;
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
				assert(usr->events != 0);
				ev.modify(usr->sock->fd(), usr->events);
			}
		}
	}
}

void Server::uRead(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	
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
			
			uRemove(usr, protocol::user_event::BrokenPipe);
			return;
		}
	}
	else if (rb == 0)
	{
		#ifndef NDEBUG
		std::cerr << "User disconnected!" << std::endl;
		#endif
		
		uRemove(usr, protocol::user_event::Disconnect);
		return;
	}
	else
	{
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "Received " << rb << " bytes.." << std::endl;
		#endif
		#endif
		
		usr->input.write(rb);
		
		uProcessData(usr);
	}
}

void Server::uProcessData(User*& usr) throw()
{
	assert(usr != 0);
	
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
				uRemove(usr, protocol::user_event::Dropped);
				return;
			}
		}
		else if (usr->inMsg->type != usr->input.rpos[0])
		{
			// Input buffer frelled
			std::cerr << "Buffer corruption, message type changed." << std::endl;
			uRemove(usr, protocol::user_event::Dropped);
			return;
		}
		
		size_t cread, len;
		
		cread = usr->input.canRead();
		len = usr->inMsg->reqDataLen(usr->input.rpos, cread);
		if (len > cread)
		{
			// TODO: Make this work better!
			if (len <= usr->input.left)
			{
				// reposition the buffer
				#ifndef NDEBUG
				std::cout << "Re-positioning buffer for maximum read length." << std::endl;
				#endif
				usr->input.reposition();
				
				// update cread and len
				cread = usr->input.canRead();
				len = usr->inMsg->reqDataLen(usr->input.rpos, cread);
				
				// need more data
				if (len > cread)
					return;
			}
			else
			{
				#ifdef DEBUG_SERVER
				#ifndef NDEBUG
				std::cout << "Need " << (len - cread) << " bytes more." << std::endl
					<< "Required size: " << len << std::endl
					<< "We already have: " << cread << std::endl;
				
				#endif // NDEBUG
				#endif // DEBUG_SERVER
				
				// still need more data
				return;
			}
		}
		
		usr->input.read(
			usr->inMsg->unserialize(usr->input.rpos, cread)
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
		
		if (usr != 0)
		{
			delete usr->inMsg;
			usr->inMsg = 0;
		}
		else
		{
			break;
		}
	}
}

message_ref Server::uCreateEvent(User* usr, Session* session, uint8_t event) const throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(session != 0);
	
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

bool Server::uInSession(User* usr, uint8_t session) const throw()
{
	if (usr->sessions.find(session) == usr->sessions.end())
		return false;
	
	return true;
}

bool Server::sessionExists(uint8_t session) const throw()
{
	if (sessions.find(session) == sessions.end())
		return false;
	
	return true;
}

void Server::uHandleMsg(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	#endif
	
	assert(usr->state == uState::active);
	
	// TODO
	switch (usr->inMsg->type)
	{
	case protocol::type::ToolInfo:
		// check for protocol violations
		#ifdef CHECK_VIOLATIONS
		if (!fIsSet(usr->tags, uTag::CanChange))
		{
			std::cerr << "Protocol violation from user: "
				<< static_cast<int>(usr->id) << std::endl;
			std::cerr << "Reason: Unexpected tool info" << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		fSet(usr->tags, uTag::HaveTool);
		#endif // CHECK_VIOLATIONS
	case protocol::type::StrokeInfo:
		// check for protocol violations
		#ifdef CHECK_VIOLATIONS
		if (usr->inMsg->type == protocol::type::StrokeInfo)
		{
			/*
			if (!fIsSet(usr->tags, uTag::HaveTool))
			{
				std::cerr << "Protocol violation from user: "
					<< static_cast<int>(usr->id) << std::endl;
				std::cerr << "Reason: Stroke info without tool." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				break;
			}
			*/
			fClr(usr->tags, uTag::CanChange);
		}
		#endif // CHECK_VIOLATIONS
	case protocol::type::StrokeEnd:
		// check for protocol violations
		#ifdef CHECK_VIOLATIONS
		if (usr->inMsg->type == protocol::type::StrokeEnd)
		{
			if (!fIsSet(usr->tags, uTag::HaveTool))
			{
				std::cerr << "Protocol violation from user: "
					<< static_cast<int>(usr->id) << std::endl;
				std::cerr << "Reason: Stroke info without tool." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				break;
			}
			fSet(usr->tags, uTag::CanChange);
		}
		#endif // CHECK_VIOLATIONS
		
		#ifdef CHECK_VIOLATIONS
		if ((usr->inMsg->user_id != protocol::null_user)
			#ifdef LENIENT
			or (usr->inMsg->user_id != usr->id)
			#endif // LENIENT
		)
		{
			std::cerr << "Client attempted to impersonate someone." << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		#endif // CHECK_VIOLATIONS
		
		// handle all message with 'selected' modifier the same
		{
			// make sure the user id is correct
			usr->inMsg->user_id = usr->id;
			
			// needed for Propagate()
			usr->inMsg->session_id = usr->session;
			
			// scroll to the last messag in linked-list
			//message_ref ref;
			protocol::Message* msg = usr->inMsg;
			while (msg->next != 0)
				msg = msg->next;
			/*
			do
			{
				ref.reset(msg);
				
				msg = msg->next;
				
				// kill linked list, since it doesn't work properly with shared_ptr
				ref->next = 0;
				ref->prev = 0;
				
				Propagate(ref);
			}
			while (msg != 0);
			*/
			
			if (fIsSet(usr->caps, protocol::client::AckFeedback))
			{
				uSendMsg(usr, msgAck(usr->session, msg->type));
			}
			
			Propagate(
				message_ref(msg),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
		}
		usr->inMsg = 0;
		break;
	case protocol::type::Acknowledgement:
		uHandleAck(usr);
		break;
	case protocol::type::SessionSelect:
		#ifdef CHECK_VIOLATIONS
		if (!fIsSet(usr->tags, uTag::CanChange))
		{
			std::cerr << "Protocol violation from user: "
				<< static_cast<int>(usr->id) << std::endl;
			std::cerr << "Reason: Session change in middle of something." << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		#endif
		if (uInSession(usr, usr->inMsg->session_id))
		{
			uSendMsg(usr, msgAck(usr->inMsg->session_id, protocol::type::SessionSelect));
			usr->inMsg->user_id = usr->id;
			usr->session = usr->inMsg->session_id;
			
			if (fIsSet(usr->caps, protocol::client::AckFeedback))
			{
				uSendMsg(usr, msgAck(protocol::Global, msg->type));
			}
			
			Propagate(
				message_ref(usr->inMsg),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			usr->inMsg = 0;
			
			#ifdef CHECK_VIOLATIONS
			fSet(usr->tags, uTag::CanChange);
			#endif
		}
		else
		{
			uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			usr->session = protocol::Global;
		}
		break;
	case protocol::type::UserInfo:
		// TODO?
		break;
	case protocol::type::Raster:
		uTunnelRaster(usr);
		break;
	case protocol::type::Deflate:
		// TODO
		break;
	case protocol::type::Chat:
	case protocol::type::Palette:
		{
			message_ref pmsg(usr->inMsg);
			pmsg->user_id = usr->id;
			
			if (!sessionExists(pmsg->session_id))
			{
				uSendMsg(usr,
					msgError(usr->inMsg->session_id, protocol::error::UnknownSession)
				);
				break;
			}
			
			if (!uInSession(usr, pmsg->session_id))
			{
				uSendMsg(usr,
					msgError(usr->inMsg->session_id, protocol::error::NotSubscribed)
				);
				break;
			}
			
			
			if (fIsSet(usr->caps, protocol::client::AckFeedback))
			{
				uSendMsg(usr, msgAck(msg->session_id, msg->type));
			}
			
			Propagate(
				pmsg,
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			usr->inMsg = 0;
		}
		break;
	case protocol::type::Unsubscribe:
		#ifdef CHECK_VIOLATIONS
		if (!fIsSet(usr->tags, uTag::CanChange))
		{
			std::cerr << "Protocol violation from user: "
				<< static_cast<int>(usr->id) << std::endl;
			std::cerr << "Reason: Unsubscribe in middle of something." << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		#endif
		{
			session_iterator si(sessions.find(usr->inMsg->session_id));
			
			if (si == sessions.end())
			{
				#ifndef NDEBUG
				std::cerr << "No such session: "
					<< static_cast<int>(usr->inMsg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			}
			else
			{
				uSendMsg(usr, msgAck(usr->inMsg->session_id, protocol::type::Unsubscribe));
				
				uLeaveSession(usr, si->second);
			}
		}
		break;
	case protocol::type::Subscribe:
		#ifdef CHECK_VIOLATIONS
		if (!fIsSet(usr->tags, uTag::CanChange))
		{
			std::cerr << "Protocol violation from user: "
				<< static_cast<int>(usr->id) << std::endl;
			std::cerr << "Reason: Subscribe in middle of something." << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		#endif
		if (usr->syncing == protocol::Global)
		{
			session_iterator si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
			{
				// session not found
				#ifndef NDEBUG
				std::cerr << "No such session: "
					<< static_cast<int>(usr->inMsg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				break;
			}
			
			if (!uInSession(usr, usr->inMsg->session_id))
			{
				if (si->second->password != 0)
				{
					uSendMsg(usr, msgAuth(usr, si->second->id));
					break;
				}
				
				// join session
				uSendMsg(usr, msgAck(usr->inMsg->session_id, protocol::type::Subscribe));
				uJoinSession(usr, si->second);
			}
			else
			{
				// already subscribed
				uSendMsg(usr, msgError(
					usr->inMsg->session_id, protocol::error::InvalidRequest)
				);
			}
		}
		else
		{
			// already syncing some session, so we don't bother handling this request.
			uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
		}
		break;
	case protocol::type::ListSessions:
		if (sessions.size() != 0)
		{
			protocol::SessionInfo *nfo = 0;
			session_iterator si(sessions.begin());
			for (; si != sessions.end(); si++)
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
		
		uSendMsg(usr, msgAck(protocol::Global, protocol::type::ListSessions));
		break;
	case protocol::type::Instruction:
		uHandleInstruction(usr);
		break;
	case protocol::type::Password:
		{
			protocol::Password *msg = static_cast<protocol::Password*>(usr->inMsg);
			if (msg->session_id == protocol::Global)
			{
				// Admin login
				if (a_password == 0)
				{
					std::cerr << "User tries to pass password even though we've disallowed it." << std::endl;
					uSendMsg(usr, msgError(msg->session_id, protocol::error::PasswordFailure));
					return;
				}
				
				hash.Update(reinterpret_cast<uint8_t*>(a_password), a_pw_len);
				hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
				hash.Final();
				char digest[protocol::password_hash_size];
				hash.GetHash(reinterpret_cast<uint8_t*>(digest));
				hash.Reset();
				
				if (memcmp(digest, msg->data, protocol::password_hash_size) != 0)
				{
					// mismatch, send error or disconnect.
					uSendMsg(usr, msgError(msg->session_id, protocol::error::PasswordFailure));
					return;
				}
				
				usr->mode = protocol::user_mode::Administrator;
			}
			else
			{
				if (usr->syncing != protocol::Global)
				{
					// already syncing some session, so we don't bother handling this request.
					uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
					break;
				}
				
				session_iterator si(sessions.find(msg->session_id));
				if (si == sessions.end())
				{
					// session doesn't exist
					uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
					break;
				}
				
				if (uInSession(usr, msg->session_id))
				{
					// already in session
					uSendMsg(usr, msgError(msg->session_id, protocol::error::InvalidRequest));
					break;
				}
				
				hash.Update(reinterpret_cast<uint8_t*>(si->second->password), si->second->pw_len);
				hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
				hash.Final();
				char digest[protocol::password_hash_size];
				hash.GetHash(reinterpret_cast<uint8_t*>(digest));
				hash.Reset();
				
				if (memcmp(digest, msg->data, protocol::password_hash_size) != 0)
				{
					// mismatch, send error or disconnect.
					uSendMsg(usr, msgError(msg->session_id, protocol::error::PasswordFailure));
					break;
				}
				
				// join session
				uSendMsg(usr, msgAck(si->second->id, protocol::type::Subscribe));
				uJoinSession(usr, si->second);
			}
			
			usr->seed[0] = (rand() % 255) + 1;
			usr->seed[1] = (rand() % 255) + 1;
			usr->seed[2] = (rand() % 255) + 1;
			usr->seed[3] = (rand() % 255) + 1;
			
			uSendMsg(usr, msgAck(msg->session_id, protocol::type::Password));
		}
		break;
	default:
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cerr << "Unexpected, unhandled or unknown message type." << std::endl;
		protocol::msgName(usr->inMsg->type);
		#else
		std::cerr << "Garbage from user: " << static_cast<int>(usr->id) << std::endl;
		#endif
		#endif
		uRemove(usr, protocol::user_event::Dropped);
		break;
	}
}

void Server::uHandleAck(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	//#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleAck()" << std::endl;
	#endif
	//#endif
	
	protocol::Acknowledgement *ack = static_cast<protocol::Acknowledgement*>(usr->inMsg);
	
	switch (ack->event)
	{
	case protocol::type::SyncWait:
		#ifndef NDEBUG
		std::cout << "ACK/SyncWait!" << std::endl;
		#endif
		// TODO
		
		{
			usr_session_iterator us(usr->sessions.find(ack->session_id));
			if (us == usr->sessions.end())
			{
				uSendMsg(usr, msgError(ack->session_id, protocol::error::NotSubscribed));
				return;
			}
			
			if (us->second.syncWait)
			{
				#ifndef NDEBUG
				std::cout << "Another ACK/SyncWait for same session. Kickin'!" << std::endl;
				#endif
				
				uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			else
			{
				us->second.syncWait = true;
			}
			
			Session *session(sessions.find(ack->session_id)->second);
			session->syncCounter--;
			
			if (session->syncCounter == 0)
			{
				#ifndef NDEBUG
				std::cout << "SyncWait counter reached 0." << std::endl;
				#endif
				
				SyncSession(session);
			}
			else
			{
				#ifndef NDEBUG
				std::cout << "ACK/SyncWait counter: " << session->syncCounter << std::endl;
				#endif
			}
		}
		break;
	default:
		#ifndef NDEBUG
		std::cout << "ACK/SomethingWeDoNotCareAboutButShouldValidateNoneTheLess" << std::endl;
		#endif
		// don't care..
		break;
	}
}

void Server::uTunnelRaster(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uTunnelRaster(from: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif // NDEBUG
	#endif // DEBUG_SERVER
	
	protocol::Raster *raster = static_cast<protocol::Raster*>(usr->inMsg);
	
	bool last = (raster->offset + raster->length == raster->size);
	
	if (!uInSession(usr, raster->session_id))
	{
		std::cerr << "Raster for unsubscribed session: "
			<< static_cast<int>(raster->session_id) << std::endl;
		
		if (!last)
		{
			message_ref cancel(new protocol::Cancel);
			cancel->session_id = raster->session_id;
			uSendMsg(usr, cancel);
		}
		return;
	}
	
	// get users
	std::pair<tunnel_iterator,tunnel_iterator> ft(tunnel.equal_range(usr->id));
	if (ft.first == ft.second)
	{
		std::cerr << "Un-tunneled raster from: "
			<< static_cast<int>(usr->id) << std::endl;
		
		// We assume the raster was for user/s who disappeared
		// before we could cancel the request.
		
		if (!last)
		{
			message_ref cancel(new protocol::Cancel);
			cancel->session_id = raster->session_id;
			uSendMsg(usr, cancel);
		}
		return;
	}
	
	uSendMsg(usr, msgAck(usr->inMsg->session_id, protocol::type::Raster));
	
	// Forward to users.
	tunnel_iterator ti(ft.first);
	user_iterator ui;
	for (; ti != ft.second; ti++)
	{
		ui = users.find(ti->second);
		uSendMsg(ui->second, message_ref(raster));
		if (last) ui->second->syncing = false;
	}
	
	// Break tunnel if that was the last raster piece.
	if (last) tunnel.erase(usr->id);
	
	// Avoid premature deletion of raster data.
	usr->inMsg = 0;
}

void Server::uHandleInstruction(User*& usr) throw(std::bad_alloc)
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uHandleInstruction()" << std::endl;
	#endif
	#endif
	
	assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::type::Instruction);
	
	protocol::Instruction* msg = static_cast<protocol::Instruction*>(usr->inMsg);
	
	// TODO: Allow session owners to alter sessions.
	if (!fIsSet(usr->mode, protocol::user_mode::Administrator))
	{
		std::cerr << "Non-admin tries to pass instructions" << std::endl;
		uRemove(usr, protocol::user_event::Dropped);
		return;
	}
	
	switch (msg->command)
	{
	case protocol::admin::command::Create:
		if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
		// limited scope for switch/case
		{
			uint8_t session_id = getSessionID();
			
			if (session_id == protocol::Global)
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::SessionLimit));
				return;
			}
			
			Session* session(new Session);
			session->id = session_id;
			session->limit = msg->aux_data; // user limit
			session->mode = msg->aux_data2; // user mode
			
			if (fIsSet(session->mode, protocol::user_mode::Administrator))
			{
				std::cerr << "Administrator flag in default mode." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				delete session;
				return;
			}
			
			if (session->limit < 2)
			{
				#ifndef NDEBUG
				std::cerr << "Attempted to create single user session." << std::endl;
				#endif
				
				uSendMsg(usr, msgError(msg->session_id, protocol::error::InvalidData));
				delete session;
				return;
			}
			
			size_t crop = sizeof(session->width) + sizeof(session->height);
			if (msg->length < crop)
			{
				#ifndef NDEBUG
				std::cerr << "Less data than required" << std::endl;
				#endif
				
				uRemove(usr, protocol::user_event::Violation);
				delete session;
				return;
			}
			
			memcpy_t(session->width, msg->data);
			memcpy_t(session->height, msg->data+sizeof(session->width));
			
			bswap(session->width);
			bswap(session->height);
			
			if (session->width < min_dimension or session->height < min_dimension)
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::TooSmall));
				delete session;
				return;
			}
			
			if (msg->length < crop)
			{
				std::cerr << "Invalid data size in instruction: 'Create'." << std::endl;
				uSendMsg(usr, msgError(protocol::Global, protocol::error::InvalidData));
				delete session;
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
				session->len = 0;
				#ifndef NDEBUG
				std::cout << "No title set for session." << std::endl;
				#endif
			}
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateSessionTitle(session))
			{
				#ifndef NDEBUG
				std::cerr << "Title not unique." << std::endl;
				#endif
				
				uSendMsg(usr, msgError(msg->session_id, protocol::error::NotUnique));
				delete session;
				return;
			}
			
			session->owner = usr->id;
			
			// TODO
			//registerSession(s);
			sessions.insert( std::make_pair(session->id, session) );
			
			#ifndef NDEBUG
			std::cout << "Session created: " << static_cast<int>(session->id) << std::endl
				<< "With dimensions: " << session->width << " x " << session->height << std::endl;
			#endif
			
			uSendMsg(usr, msgAck(msg->session_id, protocol::type::Instruction));
		}
		break;
	case protocol::admin::command::Destroy:
		if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
		// TODO
		break;
	case protocol::admin::command::Alter:
		if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
		// TODO
		break;
	case protocol::admin::command::Password:
		if (msg->session_id == protocol::Global)
		{
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
			
			password = msg->data;
			pw_len = msg->length;
			
			msg->data = 0;
			//msg->length = 0;
			
			#ifndef NDEBUG
			std::cout << "Server password changed." << std::endl;
			#endif
			
			uSendMsg(usr, msgAck(protocol::Global, protocol::type::Instruction));
		}
		else
		{
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
			// TODO: Check session ownership
			
			session_iterator si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
			}
			else
			{
				#ifndef NDEBUG
				std::cout << "Password set for session: "
					<< static_cast<int>(msg->session_id) << std::endl;
				#endif
				
				si->second->password = msg->data;
				si->second->pw_len = msg->length;
				msg->data = 0;
				
				uSendMsg(usr, msgAck(protocol::Global, protocol::type::Instruction));
			}
		}
		break;
	case protocol::admin::command::Authenticate:
		#ifndef NDEBUG
		std::cout << "User wishes to authenticate itself as an admin." << std::endl;
		#endif
		if (a_password == 0)
		{
			// no admin password set
			uSendMsg(usr, msgError(msg->session_id, protocol::error::InvalidRequest));
		}
		else
		{
			// request password
			uSendMsg(usr, msgAuth(usr, protocol::Global));
		}
		return;
	case protocol::admin::command::Shutdown:
		if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
		state = server::state::Exiting;
		break;
	default:
		#ifndef NDEBUG
		std::cerr << "Unrecognized command: "
			<< static_cast<int>(msg->command) << std::endl;
		#endif
		uRemove(usr, protocol::user_event::Dropped);
		break;
	}
}

void Server::uHandleLogin(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
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
				
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			if (msg->session_id != protocol::Global)
			{
				#ifndef NDEBUG
				std::cerr << "Wrong session identifier." << std::endl;
				#endif
				
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			if (msg->event != protocol::user_event::Login)
			{
				#ifndef NDEBUG
				std::cerr << "Wrong user event." << std::endl;
				#endif
				
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			if (msg->length > name_len_limit)
			{
				#ifndef NDEBUG
				std::cerr << "Name too long." << std::endl;
				#endif
				
				#if 0
				uSendMsg(usr, msgError(msg->session_id, protocol::error::TooLong));
				#endif // 0
				
				uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateUserName(usr))
			{
				#ifndef NDEBUG
				std::cerr << "Name not unique." << std::endl;
				#endif
				
				#if 0
				uSendMsg(usr, msgError(msg->session_id, protocol::error::NotUnique));
				#endif // 0
				
				uRemove(usr, protocol::user_event::Dropped);
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
			
			if (fIsSet(opmode, server::mode::LocalhostAdmin)
				and (usr->sock->address() == INADDR_LOOPBACK))
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
			uRemove(usr, protocol::user_event::Violation);
		}
		break;
	case uState::login_auth:
		#ifndef NDEBUG
		std::cout << "login_auth" << std::endl;
		#endif
		
		if (usr->inMsg->type == protocol::type::Password)
		{
			protocol::Password *msg = static_cast<protocol::Password*>(usr->inMsg);
			
			hash.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			hash.Reset();
			
			if (memcmp(digest, msg->data, protocol::password_hash_size) != 0)
			{
				// mismatch, send error or disconnect.
				uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			
			#ifdef CHECK_VIOLATIONS
			fSet(usr->tags, uTag::CanChange);
			#endif
			
			uSendMsg(usr, msgAck(msg->session_id, protocol::type::Password));
			
			// make sure the same seed is not used for something else.
			usr->seed[0] = (rand() % 255) + 1;
			usr->seed[1] = (rand() % 255) + 1;
			usr->seed[2] = (rand() % 255) + 1;
			usr->seed[3] = (rand() % 255) + 1;
		}
		else
		{
			// not a password
			uRemove(usr, protocol::user_event::Violation);
			return;
		}
		
		usr->state = uState::login;
		uSendMsg(usr, msgHostInfo());
		
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
				
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			if (!rev)
			{
				#ifndef NDEBUG
				std::cerr << "Protocol revision mismatch ---  "
					<< "expected: " << protocol::revision
					<< ", got: " << ident->revision << std::endl;
				#endif
				
				// TODO: Implement some compatible way of announcing incompatibility
				
				uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			
			if (password == 0)
			{
				#ifndef NDEBUG
				std::cout << "Proceed to login" << std::endl;
				#endif
				
				// no password set
				
				#ifdef CHECK_VIOLATIONS
				fSet(usr->tags, uTag::CanChange);
				#endif
				
				usr->state = uState::login;
				uSendMsg(usr, msgHostInfo());
			}
			else
			{
				#ifndef NDEBUG
				std::cout << "Request password" << std::endl;
				#endif
				
				usr->state = uState::login_auth;
				uSendMsg(usr, msgAuth(usr, protocol::Global));
			}
			
			usr->caps = ident->flags;
		}
		else
		{
			#ifndef NDEBUG
			std::cerr << "Invalid data from user: "
				<< static_cast<int>(usr->id) << std::endl;
			#endif
			
			uRemove(usr, protocol::user_event::Dropped);
		}
		break;
	case uState::dead:
		std::cerr << "I see dead people." << std::endl;
		uRemove(usr, protocol::user_event::Dropped);
		break;
	default:
		assert(!"user state was something strange");
		break;
	}
}

void Server::Propagate(message_ref msg, uint8_t source) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::Propagate(session: " << static_cast<int>(msg->session_id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	#endif
	#endif
	
	session_iterator si(sessions.find(msg->session_id));
	if (si == sessions.end())
	{
		#ifndef NDEBUG
		std::cerr << "No such session!" << std::endl;
		#endif
		
		return;
	}
	
	session_usr_iterator ui( si->second->users.begin() );
	for (; ui != si->second->users.end(); ui++)
	{
		if (ui->second->id != source)
			uSendMsg(ui->second, msg);
	}
}

void Server::uSendMsg(User* usr, message_ref msg) throw()
{
	assert(usr != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uSendMsg(to user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	protocol::msgName(msg->type);
	#endif
	#endif
	
	usr->queue.push_back( msg );
	
	if (!fIsSet(usr->events, ev.write))
	{
		fSet(usr->events, ev.write);
		ev.modify(usr->sock->fd(), usr->events);
	}
}

void Server::SyncSession(Session* session) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::SyncSession(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	#endif
	
	assert(session != 0);
	assert(session->syncCounter == 0);
	
	// TODO: Need better source user selection.
	User* src(session->users.begin()->second);
	
	// request raster
	message_ref syncreq(new protocol::Synchronize);
	syncreq->session_id = session->id;
	uSendMsg(src, syncreq);
	
	// Release clients from syncwait...
	Propagate(msgAck(session->id, protocol::type::SyncWait));
	
	std::list<User*> newc;
	// Get new clients
	while (session->waitingSync.size() != 0)
	{
		newc.insert(newc.end(), session->waitingSync.front());
		session->waitingSync.pop_front();
	}
	
	message_ref msg;
	std::vector<message_ref> msg_queue;
	session_usr_iterator old(session->users.begin());
	// build msg_queue
	for (; old != session->users.end(); old++)
	{
		// clear syncwait 
		old->second->sessions.find(session->id)->second.syncWait = false;
		
		// add messages to msg_queue
		msg_queue.push_back(uCreateEvent(old->second, session, protocol::user_event::Join));
		if (old->second->session == session->id)
		{
			msg.reset(new protocol::SessionSelect);
			msg->user_id = old->second->id;
			msg->session_id = session->id;
			msg_queue.push_back(msg);
		}
		
	}
	
	std::list<User*>::iterator new_i, new_i2;
	std::vector<message_ref>::iterator msg_queue_i;
	// Send messages
	for (new_i = newc.begin(); new_i != newc.end(); new_i++)
	{
		for (msg_queue_i=msg_queue.begin(); msg_queue_i != msg_queue.end(); msg_queue_i++)
		{
			uSendMsg(*new_i, *msg_queue_i);
		}
	}
	
	// put waiting clients to normal data propagation and create raster tunnels.
	for (new_i = newc.begin(); new_i != newc.end(); new_i++)
	{
		// Create fake tunnel
		tunnel.insert( std::make_pair(src->id, (*new_i)->sock->fd()) );
		
		// add user to normal data propagation.
		session->users.insert( std::make_pair((*new_i)->id, (*new_i)) );
		
		// Tell other syncWait users of this user..
		if (newc.size() > 1)
		{
			for (new_i2 = newc.begin(); new_i2 != newc.end(); new_i2++)
			{
				if (new_i2 == new_i) continue; // skip self
				msg = uCreateEvent(old->second, session, protocol::user_event::Join);
			}
		}
	}
}

void Server::uJoinSession(User*& usr, Session* session) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uJoinSession()" << std::endl;
	#endif
	#endif
	
	// Add session to users session list.
	usr->sessions.insert(
		std::make_pair( session->id, SessionData(usr->id, session) )
	);
	
	// Tell session members there's a new user.
	Propagate(uCreateEvent(usr, session, protocol::user_event::Join));
	
	if (session->users.size() != 0)
	{
		// put user to wait sync list.
		session->waitingSync.push_back(usr);
		usr->syncing = session->id;
		
		// don't start new client sync if one is already in progress...
		if (session->syncCounter == 0)
		{
			// Put session to syncing state
			session->syncCounter = session->users.size();
			
			// tell session users to enter syncwait state.
			Propagate(msgSyncWait(session));
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

void Server::uLeaveSession(User*& usr, Session* session) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
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
		if (!fIsSet(session->flags, protocol::session::NoSelfDestruct))
		{
			freeSessionID(session->id);
			sessions.erase(session->id);
			delete session;
		}
		
		return;
	}
	
	// Tell session members the user left
	Propagate(uCreateEvent(usr, session, protocol::user_event::Leave));
	
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
	
	if (id == protocol::null_user)
	{
		//#ifndef NDEBUG
		std::cout << "Server is full, kicking user." << std::endl;
		//#endif
		
		delete sock;
		sock = 0;
		
		return;
	}
	else
	{
		#ifdef DEBUG_SERVER
		#ifndef NDEBUG
		std::cout << "New user: " << static_cast<uint32_t>(id) << std::endl;
		#endif
		#endif
		
		User* usr(new User(id, sock));
		
		usr->session = protocol::Global;
		
		usr->events = 0;
		fSet(usr->events, ev.read);
		ev.add(usr->sock->fd(), usr->events);
		
		users.insert( std::make_pair(sock->fd(), usr) );
		
		#ifdef DEBUG_BUFFER
		#ifndef NDEBUG
		std::cout << "Assigning buffer to user." << std::endl;
		#endif
		#endif
		
		usr->input.setBuffer(new char[8192], 8192);
		
		#ifndef NDEBUG
		std::cout << "Known users: " << users.size() << std::endl;
		#endif
	}
}

void Server::breakSync(User*& usr) throw()
{
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::breakSync(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	#endif
	
	uSendMsg(usr, msgError(usr->syncing, protocol::error::SyncFailure));
	uLeaveSession(usr, sessions.find(usr->syncing)->second);
	usr->syncing = protocol::Global;
}

void Server::uRemove(User *&usr, uint8_t reason) throw()
{
	assert(usr != 0);
	
	#ifdef DEBUG_SERVER
	#ifndef NDEBUG
	std::cout << "Server::uRemove()" << std::endl;
	#endif
	#endif
	
	usr->state = uState::dead;
	
	// Remove from event system
	ev.remove(usr->sock->fd(), ev.read|ev.write|ev.error|ev.hangup);
	
	// Clear the fake tunnel of any possible instance of this user.
	// We're the source...
	tunnel_iterator ti;
	while ((ti = tunnel.find(usr->id)) != tunnel.end())
	{
		breakSync(users.find(ti->second)->second);
		tunnel.erase(ti->first);
	}
	
	/*
	 * TODO: Somehow figure out which of the sources are now without a tunnel
	 * and send Cancel to them.
	 *
	 */
	do
	{
		for (ti = tunnel.begin(); ti != tunnel.end(); ti++)
		{
			if (ti->second == usr->sock->fd())
			{
				tunnel.erase(ti->first);
				break;
			}
		}
	}
	while (ti != tunnel.end());
	
	// clean sessions
	while (usr->sessions.size() != 0)
	{
		uLeaveSession(usr, usr->sessions.begin()->second.session);
	}
	
	// remove from fd -> User* map
	users.erase(usr->sock->fd());
	
	freeUserID(usr->id);
	
	delete usr;
	usr = 0;
	
	// Transient mode exit.
	if (fIsSet(opmode, server::mode::Transient)
		and users.size() == 0)
	{
		state = server::state::Exiting;
	}
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
	
	state = server::state::Init;
	
	return 0;
}

bool Server::validateUserName(User* usr) const throw()
{
	assert(usr != 0);
	
	#ifndef NDEBUG
	std::cout << "Server::validateUserName(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	if (!fIsSet(requirements, protocol::requirements::EnforceUnique))
		return true;
	
	if (usr->nlen == 0) return false;
	
	std::map<fd_t, User*>::const_iterator ui(users.begin());
	for (; ui != users.end(); ui++)
	{
		if (usr->nlen == ui->second->nlen
			and (memcmp(usr->name, ui->second->name, usr->nlen) == 0))
		{
			return false;
		}
	}
	
	return true;
}

bool Server::validateSessionTitle(Session* session) const throw()
{
	assert(session != 0);
	
	#ifndef NDEBUG
	std::cout << "Server::validateSessionTitle(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	if (!fIsSet(requirements, protocol::requirements::EnforceUnique))
		return true;
	
	if (session->len == 0) return false;
	
	std::map<uint8_t, Session*>::const_iterator si(sessions.begin());
	for (; si != sessions.end(); si++)
	{
		if (session->len == si->second->len
			and (memcmp(session->title, si->second->title, session->len) == 0))
		{
			return false;
		}
	}
	
	return true;
}

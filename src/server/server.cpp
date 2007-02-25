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
#include "../shared/protocol.types.h"
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
	time_limit(180),
	current_time(0),
	next_timer(0),
	hi_port(protocol::default_port),
	lo_port(protocol::default_port),
	min_dimension(400),
	requirements(0),
	extensions(protocol::extensions::Chat|protocol::extensions::Palette),
	default_user_mode(protocol::user_mode::None),
	opmode(0)
{
	for (uint8_t i=0; i != std::numeric_limits<uint8_t>::max(); i++)
	{
		user_ids.push(i+1);
		session_ids.push(i+1);
	}
}

Server::~Server() throw()
{
	cleanup();
}

uint8_t Server::getUserID() throw()
{
	if (user_ids.empty())
		return protocol::null_user;
	
	uint8_t n = user_ids.front();
	user_ids.pop();
	
	return n;
}

uint8_t Server::getSessionID() throw()
{
	if (session_ids.empty())
		return protocol::Global;
	
	uint8_t n = session_ids.front();
	session_ids.pop();
	
	return n;
}

void Server::freeUserID(uint8_t id) throw()
{
	assert(id != protocol::null_user);
	
	// unfortunately queue can't be iterated, so we can't test if the ID is valid
	
	user_ids.push(id);
}

void Server::freeSessionID(uint8_t id) throw()
{
	assert(id != protocol::Global);
	
	// unfortunately queue can't be iterated, so we can't test if the ID is valid
	
	session_ids.push(id);
}

void Server::cleanup() throw()
{
	// finish event system
	ev.finish();
	
	// close listening socket
	lsock.close();
	
	#if defined( FULL_CLEANUP )
	users.clear();
	#endif // FULL_CLEANUP
}

void Server::uRegenSeed(User*& usr) const throw()
{
	usr->seed[0] = (rand() % 255) + 1;
	usr->seed[1] = (rand() % 255) + 1;
	usr->seed[2] = (rand() % 255) + 1;
	usr->seed[3] = (rand() % 255) + 1;
}

inline
message_ref Server::msgAuth(User*& usr, uint8_t session) const throw(std::bad_alloc)
{
	assert(usr != 0);
	
	protocol::Authentication* auth = new protocol::Authentication;
	auth->session_id = session;
	
	uRegenSeed(usr);
	
	memcpy(auth->seed, usr->seed, protocol::password_seed_size);
	return message_ref(auth);
}
inline
message_ref Server::msgHostInfo() const throw(std::bad_alloc)
{
	protocol::HostInfo *hostnfo = new protocol::HostInfo;
	
	hostnfo->sessions = sessions.size();
	hostnfo->sessionLimit = session_limit;
	hostnfo->users = users.size();
	hostnfo->userLimit = user_limit;
	hostnfo->nameLenLimit = name_len_limit;
	hostnfo->maxSubscriptions = max_subscriptions;
	hostnfo->requirements = requirements;
	hostnfo->extensions = extensions;
	
	return message_ref(hostnfo);
}

inline
message_ref Server::msgSessionInfo(Session*& session) const throw(std::bad_alloc)
{
	protocol::SessionInfo *nfo = new protocol::SessionInfo;
	
	nfo->session_id = session->id;
	
	nfo->width = session->width;
	nfo->height = session->height;
	
	nfo->owner = session->owner;
	nfo->users = session->users.size();
	nfo->limit = session->limit;
	nfo->mode = session->mode;
	nfo->length = session->len;
	
	nfo->title = new char[session->len];
	memcpy(nfo->title, session->title, session->len);
	
	return message_ref(nfo);
}

inline
message_ref Server::msgError(uint8_t session, uint16_t code) const throw(std::bad_alloc)
{
	protocol::Error *err = new protocol::Error;
	err->code = code;
	err->session_id = session;
	return message_ref(err);
}

inline
message_ref Server::msgAck(uint8_t session, uint8_t type) const throw(std::bad_alloc)
{
	protocol::Acknowledgement *ack = new protocol::Acknowledgement;
	ack->session_id = session;
	ack->event = type;
	return message_ref(ack);
}

inline
message_ref Server::msgSyncWait(Session*& session) const throw(std::bad_alloc)
{
	assert(session != 0);
	
	protocol::SyncWait *sync = new protocol::SyncWait;
	sync->session_id = session->id;
	return message_ref(sync);
}

void Server::uWrite(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uWrite(user: " << static_cast<int>(usr->id) << ")" << std::endl;
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
			std::cout << "Linked " << outgoing.size()
				<< " messages, for total size: " << len << std::endl
				<< "Bandwidth savings are [(7*n) - ((5*n)+3)]: "
				<< (7 * outgoing.size()) - (3 + (outgoing.size() * 5)) << std::endl;
		}
		#endif // NDEBUG
		#endif // DEBUG_LINKED_LIST
		
		if (outgoing.empty())
		{
			usr->queue.pop_front();
		}
		else
		{
			// clear linked list...
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
	
	if (sb == SOCKET_ERROR)
	{
		switch (usr->sock->getError())
		{
		#ifdef WSA_SOCKET
		case WSA_IO_PENDING:
		case WSAEWOULDBLOCK:
		#endif // WSA_SOCKET
		case EINTR:
		case EAGAIN:
		case ENOBUFS:
		case ENOMEM:
			// retry
			break;
		default:
			std::cerr << "Error occured while sending to user: "
				<< static_cast<int>(usr->id) << std::endl;
			
			uRemove(usr, protocol::user_event::BrokenPipe);
			break;
		}
		
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
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uRead(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	if (usr->input.canWrite() == 0)
	{
		#ifndef NDEBUG
		std::cerr << "Input buffer full, increasing size by 8 kiB." << std::endl;
		#endif
		usr->input.resize(usr->input.size + 8192);
	}
	
	int rb = usr->sock->recv(
		usr->input.wpos,
		usr->input.canWrite()
	);
	
	if (rb == SOCKET_ERROR)
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
		std::cerr << "User disconnected: " << static_cast<int>(usr->id) << std::endl;
		uRemove(usr, protocol::user_event::Disconnect);
		return;
	}
	else
	{
		usr->input.write(rb);
		
		uProcessData(usr);
	}
}

void Server::uProcessData(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uProcessData(user: "
		<< static_cast<int>(usr->id) << ", in buffer: "
		<< usr->input.left << " bytes)" << std::endl;
	#endif
	
	while (usr->input.canRead() != 0)
	{
		if (usr->inMsg == 0)
		{
			usr->inMsg  = protocol::getMessage(usr->input.rpos[0]);
			if (usr->inMsg == 0)
			{
				// unknown message type
				uRemove(usr, protocol::user_event::Violation);
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
				#if defined(DEBUG_SERVER) and !defined(NDEBUG)
				std::cout << "Need " << (len - cread) << " bytes more." << std::endl
					<< "Required size: " << len << std::endl
					<< "We already have: " << cread << std::endl;
				#endif
				
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

message_ref Server::uCreateEvent(User*& usr, Session*& session, uint8_t event) const throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uCreateEvent(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
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

bool Server::uInSession(User*& usr, uint8_t session) const throw()
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
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uHandleMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
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
				<< static_cast<int>(usr->id) << std::endl
				<< "Reason: Unexpected tool info" << std::endl;
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
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: Stroke info without tool." << std::endl;
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
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: Stroke info without tool." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				break;
			}
			fSet(usr->tags, uTag::CanChange);
		}
		#endif // CHECK_VIOLATIONS
		
		if (usr->inMsg->type == protocol::type::StrokeInfo)
			usr->strokes++;
		else if (usr->inMsg->type == protocol::type::StrokeEnd)
			usr->strokes = 0;
		
		if (usr->inMsg->type != protocol::type::ToolInfo
			and usr->layer == protocol::null_layer)
		{
			#ifndef NDEBUG
			if (usr->strokes == 1)
			{
				std::cout << "User #" << static_cast<int>(usr->id)
					<< " is drawing on null layer!" << std::endl;
			}
			#endif
			
			#ifdef LAYER_SUPPORT
			break;
			#endif
		}
		
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
		if (fIsSet(usr->a_mode, protocol::user_mode::Locked))
		{
			// TODO: Warn user?
			break;
		}
		
		// make sure the user id is correct
		usr->inMsg->user_id = usr->id;
		
		{
			session_iterator si(sessions.find(usr->session));
			if (si == sessions.end())
			{
				#ifndef NDEBUG
				std::cerr << "(draw) no such session as "
					<< static_cast<int>(usr->session) << "!" << std::endl;
				#endif
				
				break;
			}
			else if (si->second->locked)
			{
				// session is in full lock
				break;
			}
			
			Session *session = si->second;
			
			// scroll to the last messag in linked-list
			//message_ref ref;
			protocol::Message* msg = usr->inMsg;
			while (msg->next != 0)
				msg = msg->next;
			
			Propagate(
				session,
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
			std::cerr << "Protocol violation from user." << usr->id << std::endl
				<< "Session change in middle of something." << std::endl;
			
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		else
		#endif
		{
			session_iterator si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
			{
				#ifndef NDEBUG
				std::cerr << "(select) no such session as "
					<< static_cast<int>(usr->session) << "!" << std::endl;
				#endif
				
				break;
			}
			
			usr_session_iterator usi(usr->sessions.find(si->second->id));
			if (usi == usr->sessions.end())
			{
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
				usr->session = protocol::Global;
				break;
			}
			
			usr->inMsg->user_id = usr->id;
			usr->session = si->second->id;
			usr->a_mode = usi->second.mode;
			
			Propagate(
				si->second,
				message_ref(usr->inMsg),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			usr->inMsg = 0;
			
			#ifdef CHECK_VIOLATIONS
			fSet(usr->tags, uTag::CanChange);
			#endif
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
			
			session_iterator si(sessions.find(usr->session));
			if (si == sessions.end())
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
			
			Propagate(
				si->second,
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
				<< static_cast<int>(usr->id) << std::endl
				<< "Reason: Unsubscribe in middle of something." << std::endl;
			uRemove(usr, protocol::user_event::Violation);
			break;
		}
		#endif
		{
			session_iterator si(sessions.find(usr->inMsg->session_id));
			
			if (si == sessions.end())
			{
				#ifndef NDEBUG
				std::cerr << "(unsubscribe) no such session: "
					<< static_cast<int>(usr->inMsg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			}
			else
			{
				uSendMsg(usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
				
				uLeaveSession(usr, si->second);
			}
		}
		break;
	case protocol::type::Subscribe:
		#ifdef CHECK_VIOLATIONS
		if (!fIsSet(usr->tags, uTag::CanChange))
		{
			std::cerr << "Protocol violation from user: "
				<< static_cast<int>(usr->id) << std::endl
				<< "Reason: Subscribe in middle of something." << std::endl;
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
				std::cerr << "(subscribe) no such session: "
					<< static_cast<int>(usr->inMsg->session_id) << std::endl;
				#endif
				
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				break;
			}
			Session *session = si->second;
			
			if (!uInSession(usr, usr->inMsg->session_id))
			{
				// Test userlimit
				if (session->canJoin() == false)
				{
					uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
					break;
				}
				
				if (session->password != 0)
				{
					uSendMsg(usr, msgAuth(usr, session->id));
					break;
				}
				
				// join session
				uSendMsg(usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
				uJoinSession(usr, session);
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
			session_iterator si(sessions.begin());
			for (; si != sessions.end(); si++)
			{
				uSendMsg(usr, msgSessionInfo(si->second));
			}
		}
		
		uSendMsg(usr, msgAck(protocol::Global, usr->inMsg->type));
		break;
	case protocol::type::SessionEvent:
		{
			session_iterator si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				return;
			}
			
			usr->inMsg->user_id = usr->id;
			Session *session = si->second;
			uSessionEvent(session, usr);
			//usr->inMsg = 0;
		}
		break;
	case protocol::type::LayerEvent:
		// TODO
		break;
	case protocol::type::LayerSelect:
		{
			protocol::LayerSelect *layer = static_cast<protocol::LayerSelect*>(usr->inMsg);
			
			// Find user's session instance
			usr_session_iterator ui(usr->sessions.find(layer->session_id));
			if (ui == usr->sessions.end())
			{
				uSendMsg(usr, msgError(layer->session_id, protocol::error::NotSubscribed));
				break;
			}
			
			if (layer->layer_id == ui->second.layer)
			{
				// Tries to select currently selected layer
				uSendMsg(usr, msgError(layer->session_id, protocol::error::InvalidLayer));
				break;
			}
			
			// Check if user is locked to another layer
			if (ui->second.layer_lock != protocol::null_layer
				and ui->second.layer_lock != layer->layer_id)
			{
				uSendMsg(usr, msgError(layer->session_id, protocol::error::InvalidLayer));
				break;
			}
			
			Session *session = ui->second.session;
			
			// Find layer and check if its locked
			session_layer_iterator li(session->layers.find(layer->layer_id));
			if (li == session->layers.end())
			{
				uSendMsg(usr, msgError(layer->session_id, protocol::error::UnknownLayer));
				break;
			}
			else if (li->second.locked)
			{
				uSendMsg(usr, msgError(layer->session_id, protocol::error::LayerLocked));
				break;
			}
			
			uSendMsg(usr, msgAck(layer->session_id, layer->type));
			
			layer->user_id = usr->id;
			
			Propagate(
				session,
				message_ref(layer),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			
			ui->second.layer = layer->layer_id;
		}
		break;
	case protocol::type::Instruction:
		uHandleInstruction(usr);
		break;
	case protocol::type::Password:
		{
			session_iterator si;
			Session *session=0;
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
			}
			else
			{
				if (usr->syncing != protocol::Global)
				{
					// already syncing some session, so we don't bother handling this request.
					uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
					break;
				}
				
				si = sessions.find(msg->session_id);
				if (si == sessions.end())
				{
					// session doesn't exist
					uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
					break;
				}
				session = si->second;
				
				if (uInSession(usr, msg->session_id))
				{
					// already in session
					uSendMsg(usr, msgError(msg->session_id, protocol::error::InvalidRequest));
					break;
				}
				
				// Test userlimit
				if (session->canJoin() == false)
				{
					uSendMsg(usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
					break;
				}
				
				hash.Update(reinterpret_cast<uint8_t*>(session->password), session->pw_len);
			}
			
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			hash.Reset();
			
			uRegenSeed(usr);
			
			if (memcmp(digest, msg->data, protocol::password_hash_size) != 0)
			{
				// mismatch, send error or disconnect.
				uSendMsg(usr, msgError(msg->session_id, protocol::error::PasswordFailure));
				return;
			}
			
			uSendMsg(usr, msgAck(msg->session_id, msg->type));
			
			if (msg->session_id == protocol::Global)
			{
				// set as admin
				fSet(usr->mode, protocol::user_mode::Administrator);
			}
			else
			{
				// join session
				// uSendMsg(usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
				uJoinSession(usr, session);
			}
		}
		break;
	default:
		std::cerr << "Unknown message: #" << static_cast<int>(usr->inMsg->type)
			<< ", from user: #" << static_cast<int>(usr->id)
			<< " (dropping)" << std::endl;
		uRemove(usr, protocol::user_event::Dropped);
		break;
	}
}

void Server::uHandleAck(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	protocol::Acknowledgement *ack = static_cast<protocol::Acknowledgement*>(usr->inMsg);
	
	switch (ack->event)
	{
	case protocol::type::SyncWait:
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
			
			Session *session = sessions.find(ack->session_id)->second;
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
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uTunnelRaster(from: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	protocol::Raster *raster = static_cast<protocol::Raster*>(usr->inMsg);
	
	bool last = (raster->offset + raster->length == raster->size);
	
	if (!uInSession(usr, raster->session_id))
	{
		std::cerr << "Raster for unsubscribed session: "
			<< static_cast<int>(raster->session_id)
			<< ", from: " << static_cast<int>(usr->id)
			<< std::endl;
		
		if (!last)
		{
			message_ref cancel(new protocol::Cancel);
			cancel->session_id = raster->session_id;
			uSendMsg(usr, cancel);
		}
		
		return;
	}
	
	// get users
	std::pair<tunnel_iterator,tunnel_iterator> ft(tunnel.equal_range(usr->sock->fd()));
	if (ft.first == ft.second)
	{
		std::cerr << "Un-tunneled raster from: "
			<< static_cast<int>(usr->id)
			<< ", for session: " << static_cast<int>(raster->session_id)
			<< std::endl;
		
		// We assume the raster was for user/s who disappeared
		// before we could cancel the request.
		
		if (!last)
		{
			protocol::Cancel *cancel = new protocol::Cancel;
			cancel->session_id = raster->session_id;
			uSendMsg(usr, message_ref(cancel));
		}
		return;
	}
	
	// ACK raster
	uSendMsg(usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
	
	// Forward to users.
	tunnel_iterator ti(ft.first);
	user_iterator ui;
	User *usr_ptr;
	
	message_ref raster_ref(raster);
	
	for (; ti != ft.second; ti++)
	{
		ui = users.find(ti->second);
		usr_ptr = ui->second;
		uSendMsg(usr_ptr, raster_ref);
		if (last) usr_ptr->syncing = false;
	}
	
	// Break tunnel/s if that was the last raster piece.
	if (last) tunnel.erase(usr->sock->fd());
	
	// Avoid premature deletion of raster data.
	usr->inMsg = 0;
}

void Server::uSessionEvent(Session*& session, User*& usr) throw()
{
	assert(session != 0);
	assert(usr != 0);
	assert(usr->inMsg->type == protocol::type::SessionEvent);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uSessionEvent(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	if (!fIsSet(usr->mode, protocol::user_mode::Administrator)
		and (session->owner != usr->id))
	{
		uSendMsg(usr, msgError(session->id, protocol::error::Unauthorized));
		return;
	}
	
	protocol::SessionEvent *event = static_cast<protocol::SessionEvent*>(usr->inMsg);
	
	switch (event->action)
	{
	case protocol::session_event::Kick:
		{
			session_usr_iterator sui(session->users.find(event->target));
			if (sui == session->users.end())
			{
				// user not found
				uSendMsg(usr, msgError(session->id, protocol::error::UnknownUser));
				break;
			}
			
			Propagate(session, message_ref(event));
			usr->inMsg = 0;
			User *usr_ptr = sui->second;
			uLeaveSession(usr_ptr, session, protocol::user_event::Kicked);
		}
		break;
	case protocol::session_event::Lock:
	case protocol::session_event::Unlock:
		if (event->target == protocol::null_user)
		{
			// Lock whole board
			
			#ifndef NDEBUG
			std::cout << "Changing lock for session: "
				<< static_cast<int>(session->id)
				<< ", locked: "
				<< (event->action == protocol::session_event::Lock ? "true" : "false") << std::endl;
			#endif
			
			session->locked = (event->action == protocol::session_event::Lock ? true : false);
		}
		else
		{
			// Lock single user
			
			#ifndef NDEBUG
			std::cout << "Changing lock for user: "
				<< static_cast<int>(event->target)
				<< ", locked: "
				<< (event->action == protocol::session_event::Lock ? "true" : "false")
				<< ", in session: " << static_cast<int>(session->id) << std::endl;
			#endif
			
			// Find user
			session_usr_iterator session_usr(session->users.find(event->target));
			if (session_usr == session->users.end())
			{
				uSendMsg(usr, msgError(session->id, protocol::error::UnknownUser));
				break;
			}
			User *usr = session_usr->second;
			
			// Find user's session instance (SessionData&)
			usr_session_iterator usr_session(usr->sessions.find(session->id));
			if (usr_session == usr->sessions.end())
			{
				uSendMsg(usr, msgError(session->id, protocol::error::NotInSession));
				break;
			}
			
			if (event->aux == protocol::null_layer)
			{
				// Set session flags
				if (event->action == protocol::session_event::Lock)
				{
					fSet(usr_session->second.mode, protocol::user_mode::Locked);
				}
				else
				{
					fClr(usr_session->second.mode, protocol::user_mode::Locked);
				}
				
				// Copy active session flags
				if (usr->session == event->session_id)
					usr->a_mode = usr_session->second.mode;
			}
			else
			{
				if (event->action == protocol::session_event::Lock)
				{
					#ifndef NDEBUG
					std::cout << "Lock ignored, limiting user to layer: "
						<< static_cast<int>(event->aux) << std::endl;
					#endif
					
					usr_session->second.layer_lock = event->aux;
					
					// Null-ize the active layer if the target layer is not the active one.
					if (usr_session->second.layer != usr_session->second.layer_lock)
						usr_session->second.layer = protocol::null_layer;
					if (usr->session == event->session_id)
						usr->layer = protocol::null_layer;
				}
				else // unlock
				{
					#ifndef NDEBUG
					std::cout << "Lock ignored, releasing user from layer: "
						<< static_cast<int>(event->aux) << std::endl;
					#endif
					
					usr_session->second.layer_lock = protocol::null_layer;
				}
			}
		}
		
		Propagate(session, message_ref(event));
		usr->inMsg = 0;
		
		break;
	case protocol::session_event::Delegate:
		{
			session_usr_iterator sui(session->users.find(event->target));
			if (sui == session->users.end())
			{
				// User not found
				uSendMsg(usr, msgError(session->id, protocol::error::NotInSession));
				break;
			}
			
			session->owner = sui->second->id;
			
			Propagate(
				session,
				message_ref(event),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			
			usr->inMsg = 0;
		}
		break;
	case protocol::session_event::Mute:
	case protocol::session_event::Unmute:
		{
			session_usr_iterator sui(session->users.find(event->target));
			if (sui == session->users.end())
			{
				// user not found
				uSendMsg(usr, msgError(session->id, protocol::error::NotInSession));
				break;
			}
			
			usr_session_iterator usi(sui->second->sessions.find(session->id));
			if (usi == sui->second->sessions.end())
			{
				uSendMsg(usr, msgError(session->id, protocol::error::NotInSession));
				break;
			}
			
			// Set mode
			if (event->action == protocol::session_event::Mute)
				fSet(usi->second.mode, protocol::user_mode::Mute);
			else
				fClr(usi->second.mode, protocol::user_mode::Mute);
			
			if (usr->session == event->target)
			{
				usr->a_mode = usi->second.mode;
			}
			
			Propagate(
				session,
				message_ref(event),
				(fIsSet(usr->caps, protocol::client::AckFeedback) ? usr->id : protocol::null_user)
			);
			
			usr->inMsg = 0;
		}
		break;
	case protocol::session_event::Persist:
		break;
	case protocol::session_event::CacheRaster:
		break;
	default:
		std::cerr << "Unknown session action: "
			<< static_cast<int>(event->action) << ")" << std::endl;
		uRemove(usr, protocol::user_event::Violation);
		return;
	}
}

void Server::uHandleInstruction(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uHandleInstruction(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::type::Instruction);
	
	protocol::Instruction *msg = static_cast<protocol::Instruction*>(usr->inMsg);
	
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
			
			Session *session(new Session);
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
			
			sessions.insert( std::make_pair(session->id, session) );
			
			std::cout << "Session created: " << static_cast<int>(session->id) << std::endl
				<< "Dimensions: " << session->width << " x " << session->height << std::endl
				<< "User limit: " << static_cast<int>(session->limit)
				<< ", default mode: " << static_cast<int>(session->mode) << std::endl
				<< "Owner: " << static_cast<int>(usr->id)
				<< ", from: " << usr->sock->address() << std::endl;
			
			uSendMsg(usr, msgAck(msg->session_id, msg->type));
		}
		return;
	case protocol::admin::command::Destroy:
		{
			session_iterator si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)
				and (session->owner != usr->id))
			{
				uSendMsg(usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			// tell users session was lost
			message_ref err = msgError(session->id, protocol::error::SessionLost);
			Propagate(session, err, protocol::null_user, true);
			
			// clean session users
			session_usr_iterator sui(session->users.begin());
			User* usr_ptr;
			for (; sui != session->users.end(); sui++)
			{
				usr_ptr = sui->second;
				uLeaveSession(usr_ptr, session, protocol::user_event::None);
			}
			
			// destruct
			delete session;
			sessions.erase(si->first);
		}
		break;
	case protocol::admin::command::Alter:
		{
			session_iterator si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)
				and (session->owner != usr->id))
			{
				uSendMsg(usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			if (msg->length < sizeof(session->width) + sizeof(session->height))
			{
				std::cerr << "Protocol violation from user: "
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: Too little data for Alter instruction." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			// user limit adjustment
			session->limit = msg->aux_data;
			
			// default user mode adjustment
			session->mode = msg->aux_data2;
			
			// session canvas's size
			uint16_t width, height;
			
			memcpy_t(width, msg->data),
			memcpy_t(height, msg->data+sizeof(width));
			
			bswap(width), bswap(height);
			
			if ((width < session->width) or (height < session->height))
			{
				std::cerr << "Protocol violation from user: "
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: Attempted to reduce session's canvas size." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			session->height = height,
			session->width = width;
			
			// new title
			if (msg->length > (sizeof(width) + sizeof(height)))
			{
				session->len = msg->length - (sizeof(width) + sizeof(height));
				delete [] session->title;
				session->title = new char[session->len];
				memcpy(session->title, msg->data+sizeof(width)+sizeof(height), session->len);
			}
			
			#ifndef NDEBUG
			std::cout << "Session #" << static_cast<int>(session->id) << " altered." << std::endl
				<< "Dimensions: " << width << " x " << height << std::endl
				<< "User limit: " << static_cast<int>(session->limit) << ", default mode: "
				<< static_cast<int>(session->mode) << std::endl;
			#endif // NDEBUG
			
			Propagate(session, msgSessionInfo(session));
		}
		break;
	case protocol::admin::command::Password:
		if (msg->session_id == protocol::Global)
		{
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
			
			if (password != 0)
				delete [] password;
			
			password = msg->data;
			pw_len = msg->length;
			
			msg->data = 0;
			//msg->length = 0;
			
			#ifndef NDEBUG
			std::cout << "Server password changed." << std::endl;
			#endif
			
			uSendMsg(usr, msgAck(msg->session_id, msg->type));
		}
		else
		{
			session_iterator si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!fIsSet(usr->mode, protocol::user_mode::Administrator)
				and (session->owner != usr->id))
			{
				uSendMsg(usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			#ifndef NDEBUG
			std::cout << "Password set for session: "
				<< static_cast<int>(msg->session_id) << std::endl;
			#endif
			
			if (session->password != 0)
				delete [] session->password;
			
			session->password = msg->data;
			session->pw_len = msg->length;
			msg->data = 0;
			
			uSendMsg(usr, msgAck(msg->session_id, msg->type));
			return;
		}
		break;
	case protocol::admin::command::Authenticate:
		#ifndef NDEBUG
		std::cout << "User wishes to authenticate itself as an admin." << std::endl;
		#endif
		if (a_password == 0)
		{
			// no admin password set
			uSendMsg(usr, msgError(msg->session_id, protocol::error::Unauthorized));
		}
		else
		{
			// request password
			uSendMsg(usr, msgAuth(usr, protocol::Global));
		}
		return;
	case protocol::admin::command::Shutdown:
		if (!fIsSet(usr->mode, protocol::user_mode::Administrator)) { break; }
		// Shutdown server..
		state = server::state::Exiting;
		break;
	default:
		#ifndef NDEBUG
		std::cerr << "Unrecognized command: "
			<< static_cast<int>(msg->command) << std::endl;
		#endif
		uRemove(usr, protocol::user_event::Dropped);
		return;
	}
	
	// Allow session owners to alter sessions, but warn others.
	if (!fIsSet(usr->mode, protocol::user_mode::Administrator))
	{
		std::cerr << "Non-admin tries to pass instructions: "
			<< static_cast<int>(usr->id) << std::endl;
		uSendMsg(usr, msgError(msg->session_id, protocol::error::Unauthorized));
		//uRemove(usr, protocol::user_event::Dropped);
	}
}

void Server::uHandleLogin(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uHandleLogin(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	
	switch (usr->state)
	{
	case uState::login:
		if (usr->inMsg->type == protocol::type::UserInfo)
		{
			protocol::UserInfo *msg = static_cast<protocol::UserInfo*>(usr->inMsg);
			
			if ((msg->user_id != protocol::null_user)
				or (msg->session_id != protocol::Global)
				or (msg->event != protocol::user_event::Login))
			{
				std::cerr << "Protocol violation from user: "
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: ";
				if (msg->user_id != protocol::null_user)
				{
					std::cerr << "Client assumed it knows its user ID." << std::endl;
				}
				else if (msg->session_id != protocol::Global)
				{
					std::cerr << "Wrong session identifier." << std::endl;
				}
				else if (msg->event != protocol::user_event::Login)
				{
					std::cerr << "Wrong user event for login." << std::endl;
				}
				
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			if (msg->length > name_len_limit)
			{
				#ifndef NDEBUG
				std::cerr << "Name too long." << std::endl;
				#endif
				
				uSendMsg(usr, msgError(msg->session_id, protocol::error::TooLong));
				
				//uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			
			// assign user their own name
			usr->name = msg->name;
			usr->nlen = msg->length;
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateUserName(usr))
			{
				#ifndef NDEBUG
				std::cerr << "User name not unique." << std::endl;
				#endif
				
				uSendMsg(usr, msgError(msg->session_id, protocol::error::NotUnique));
				
				//uRemove(usr, protocol::user_event::Dropped);
				return;
			}
			
			// assign message the user's id (for sending back)
			msg->user_id = usr->id;
			
			// null the message's name information, so they don't get deleted
			msg->length = 0;
			msg->name = 0;
			
			usr->mode = default_user_mode;
			
			std::string IPPort(usr->sock->address());
			std::string::size_type ns(IPPort.find_last_of(":", IPPort.length()-1));
			assert(ns != std::string::npos);
			std::string IP(
				IPPort.substr(
					0,
					ns
				)
			);
			
			if (fIsSet(opmode, server::mode::LocalhostAdmin)
				#ifdef IPV6_SUPPORT
				and (IP == "::1")) // Loopback
				#else
				and (IP == "127.0.0.1")) // Loopback
				#endif // IPV6_SUPPORT
			{
				// auto admin promotion.
				// also, don't put any other flags on the user.
				fSet(usr->mode, protocol::user_mode::Administrator);
			}
			
			msg->mode = usr->mode;
			
			// reply
			uSendMsg(usr, message_ref(msg));
			
			usr->state = uState::active;
			
			// remove fake timer
			utimer.erase(utimer.find(usr));
			
			usr->deadtime = 0;
			
			usr->inMsg = 0;
		}
		else
		{
			// wrong message type
			uRemove(usr, protocol::user_event::Violation);
		}
		break;
	case uState::login_auth:
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
			
			uSendMsg(usr, msgAck(msg->session_id, msg->type));
			
			// make sure the same seed is not used for something else.
			uRegenSeed(usr);
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
			usr->extensions = ident->extensions;
			usr->mode = default_user_mode;
			if (!fIsSet(usr->extensions, protocol::extensions::Chat))
				fSet(usr->mode, protocol::user_mode::Deaf);
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

void Server::Propagate(Session*& session, message_ref msg, uint8_t source, bool toAll) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::Propagate(session: " << static_cast<int>(msg->session_id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	#endif
	
	if (source != protocol::null_user)
	{
		user_iterator sui(users.find(source));
		uSendMsg(sui->second, msgAck(session->id, msg->type));
	}
	
	User *usr_ptr;
	session_usr_iterator ui( session->users.begin() );
	for (; ui != session->users.end(); ui++)
	{
		if (ui->second->id != source)
		{
			usr_ptr = ui->second;
			uSendMsg(usr_ptr, msg);
		}
	}
	
	if (toAll)
	{
		// send to users waiting sync as well.
		std::list<User*>::iterator wui(session->waitingSync.begin());
		for (; wui != session->waitingSync.end(); wui++)
		{
			if ((*wui)->id != source)
				uSendMsg(*wui, msg);
		}
	}
}

void Server::uSendMsg(User*& usr, message_ref msg) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uSendMsg(to user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	protocol::msgName(msg->type);
	#endif
	
	switch (msg->type)
	{
	case protocol::type::Chat:
		if (!fIsSet(usr->extensions, protocol::extensions::Chat))
			return;
		break;
	case protocol::type::Palette:
		if (!fIsSet(usr->extensions, protocol::extensions::Palette))
			return;
		break;
	default:
		break;
	}
	
	usr->queue.push_back( msg );
	
	if (!fIsSet(usr->events, ev.write))
	{
		fSet(usr->events, ev.write);
		ev.modify(usr->sock->fd(), usr->events);
	}
}

void Server::SyncSession(Session*& session) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::SyncSession(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
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
	Propagate(session, msgAck(session->id, protocol::type::SyncWait));
	
	std::list<User*> newc;
	// Get new clients
	while (session->waitingSync.size() != 0)
	{
		newc.insert(newc.end(), session->waitingSync.front());
		session->waitingSync.pop_front();
	}
	
	message_ref msg;
	protocol::LayerSelect *layer=0;
	protocol::SessionSelect *select=0;
	std::vector<message_ref> msg_queue;
	session_usr_iterator old(session->users.begin());
	// build msg_queue
	User *usr_ptr;
	for (; old != session->users.end(); old++)
	{
		// clear syncwait 
		usr_ptr = old->second;
		usr_session_iterator usi(usr_ptr->sessions.find(session->id));
		if (usi != usr_ptr->sessions.end())
		{
			usi->second.syncWait = false;
		}
		
		// add messages to msg_queue
		msg_queue.push_back(uCreateEvent(usr_ptr, session, protocol::user_event::Join));
		if (usr_ptr->session == session->id)
		{
			select = new protocol::SessionSelect;
			select->user_id = usr_ptr->id;
			select->session_id = session->id;
			msg_queue.push_back(message_ref(select));
			
			if (usr_ptr->layer != protocol::null_layer)
			{
				layer = new protocol::LayerSelect;
				layer->user_id = usr_ptr->id;
				layer->session_id = session->id;
				layer->layer_id = usr_ptr->layer;
				msg_queue.push_back(message_ref(layer));
			}
		}
	}
	
	std::list<User*>::iterator new_i(newc.begin()), new_i2;
	std::vector<message_ref>::iterator msg_queue_i;
	// Send messages
	for (; new_i != newc.end(); new_i++)
	{
		for (msg_queue_i=msg_queue.begin(); msg_queue_i != msg_queue.end(); msg_queue_i++)
		{
			uSendMsg(*new_i, *msg_queue_i);
		}
	}
	
	protocol::SessionEvent *sev=0;
	message_ref sev_ref;
	if (session->locked)
	{
		sev = new protocol::SessionEvent;
		sev->action = protocol::session_event::Lock;
		sev->target = protocol::null_user;
		sev->aux = 0;
		sev_ref.reset(sev);
	}
	
	// put waiting clients to normal data propagation and create raster tunnels.
	for (new_i = newc.begin(); new_i != newc.end(); new_i++)
	{
		// Create fake tunnel
		tunnel.insert( std::make_pair(src->sock->fd(), (*new_i)->sock->fd()) );
		
		// add user to normal data propagation.
		session->users.insert( std::make_pair((*new_i)->id, (*new_i)) );
		
		// Tell other syncWait users of this user..
		if (newc.size() > 1)
		{
			for (new_i2 = newc.begin(); new_i2 != newc.end(); new_i2++)
			{
				if (new_i2 == new_i) continue; // skip self
				uSendMsg(
					(*new_i),
					uCreateEvent((*new_i2), session, protocol::user_event::Join)
				);
			}
		}
		
		if (session->locked)
		{
			uSendMsg(*new_i, sev_ref);
		}
	}
}

void Server::uJoinSession(User*& usr, Session*& session) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uJoinSession(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	// Add session to users session list.
	usr->sessions.insert(
		std::make_pair( session->id, SessionData(usr->id, session) )
	);
	
	// Tell session members there's a new user.
	Propagate(session, uCreateEvent(usr, session, protocol::user_event::Join));
	
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
			Propagate(session, msgSyncWait(session));
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

void Server::uLeaveSession(User*& usr, Session*& session, uint8_t reason) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uLeaveSession(user: "
		<< static_cast<int>(usr->id) << ", session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	session->users.erase(usr->id);
	
	// remove
	usr->sessions.erase(session->id);
	
	// last user in session.. destruct it
	if (session->users.empty())
	{
		if (!fIsSet(session->flags, protocol::session::NoSelfDestruct))
		{
			freeSessionID(session->id);
			sessions.erase(session->id);
			delete session;
		}
		
		return;
	}
	else
	{
		// Cancel raster sending for this user
		User* usr;
		std::multimap<fd_t, fd_t>::iterator tunnel_i(tunnel.begin());
		while (tunnel_i != tunnel.end())
		{
			if (tunnel_i->second == usr->sock->fd())
			{
				// In case we've not cleaned the tunnel properly from dead users.
				assert(users.find(tunnel_i->first) != users.end());
				
				usr = users.find(tunnel_i->first)->second;
				protocol::Cancel *cancel = new protocol::Cancel;
				// TODO: Figure out which session it is from
				//cancel->session_id = session->id;
				uSendMsg(usr, message_ref(cancel));
				
				tunnel.erase(tunnel_i);
				// iterator was invalidated
				tunnel_i = tunnel.begin();
			}
			else
				++tunnel_i;
		}
	}
	
	// Tell session members the user left
	if (reason != protocol::user_event::None)
	{
		Propagate(session, uCreateEvent(usr, session, reason));
		
		if (session->owner == usr->id)
		{
			session->owner = protocol::null_user;
			
			// Announce owner disappearance.
			protocol::SessionEvent *sev = new protocol::SessionEvent;
			sev->session_id = session->id;
			sev->action = protocol::session_event::Delegate;
			sev->target = session->owner;
			Propagate(session, message_ref(sev));
		}
	}
}

void Server::uAdd(Socket* sock) throw(std::bad_alloc)
{
	if (sock == 0)
	{
		#ifndef NDEBUG
		std::cout << "Null socket, aborting user creation." << std::endl;
		#endif
		
		return;
	}
	
	// Check duplicate connections.
	user_iterator ui(users.begin());
	User* usr=0;
	for (; ui != users.end(); ui++)
	{
		usr = ui->second;
		
		if (sock->matchAddress(usr->sock))
		{
			#ifndef NDEBUG
			std::cout << "Duplicate source address: " << sock->address() << std::endl;
			#endif // NDEBUG
			
			#ifdef NO_DUPLICATE_CONNECTIONS
			delete sock;
			return;
			#endif // NO_DUPLICATE_CONNECTIONS
			
			break;
		}
	}
	
	uint8_t id = getUserID();
	
	if (id == protocol::null_user)
	{
		#ifndef NDEBUG
		std::cout << "Disconnecting new user: " << sock->address() << std::endl;
		#endif
		
		delete sock;
		sock = 0;
		
		return;
	}
	
	std::cout << "New user: " << static_cast<int>(id)
		<< ", from: " << sock->address() << std::endl;
	
	usr = new User(id, sock);
	
	usr->session = protocol::Global;
	
	usr->events = 0;
	fSet(usr->events, ev.read);
	ev.add(usr->sock->fd(), usr->events);
	
	users.insert( std::make_pair(sock->fd(), usr) );
	
	usr->deadtime = time(0) + time_limit;
	
	// re-schedule user culling
	if (next_timer > usr->deadtime)
		next_timer = usr->deadtime;
	
	// add timer
	utimer.insert(utimer.end(), usr);
	
	usr->input.setBuffer(new char[8192], 8192);
	
	#ifndef NDEBUG
	std::cout << "Known users: " << users.size() << std::endl;
	#endif
}

void Server::breakSync(User*& usr) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::breakSync(user: "
		<< static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	uSendMsg(usr, msgError(usr->syncing, protocol::error::SyncFailure));
	
	session_iterator sui(sessions.find(usr->syncing));
	if (sui == sessions.end())
	{
		#ifndef NDEBUG
		std::cerr << "Session to break sync with was not found!" << std::endl;
		#endif
		
		return;
	}
	
	uLeaveSession(usr, sui->second);
	usr->syncing = protocol::Global;
}

void Server::uRemove(User *&usr, uint8_t reason) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uRemove(user: " << static_cast<int>(usr->id)
		<< ", at address: " << usr->sock->address() << ")" << std::endl;
	#endif
	
	usr->state = uState::dead;
	
	// Remove from event system
	ev.remove(usr->sock->fd(), ev.read|ev.write|ev.error|ev.hangup);
	
	// Clear the fake tunnel of any possible instance of this user.
	// We're the source...
	tunnel_iterator ti;
	user_iterator usi;
	while ((ti = tunnel.find(usr->sock->fd())) != tunnel.end())
	{
		usi = users.find(ti->second);
		if (usi == users.end())
		{
			#ifndef NDEBUG
			std::cerr << "Tunnel's other end was not found in users." << std::endl;
			#endif
			continue;
		}
		breakSync(usi->second);
		tunnel.erase(ti);
	}
	
	/*
	 * TODO: Somehow figure out which of the sources are now without a tunnel
	 * and send Cancel to them.
	 */
	ti = tunnel.begin();
	while (ti != tunnel.end())
	{
		if (ti->second == usr->sock->fd())
		{
			// TODO: Send cancel
			/*
			protocol::Cancel *cancel = new protocol::Cancel;
			cancel->session_id = session->id;
			uSendMsg(src_usr, message_ref(cancel));
			*/
			
			tunnel.erase(ti);
			// iterator was invalidated
			ti = tunnel.begin();
		}
		else
			++ti;
	}
	
	// clean sessions
	Session *session;
	while (usr->sessions.size() != 0)
	{
		session = usr->sessions.begin()->second.session;
		uLeaveSession(usr, session, reason);
	}
	
	// remove from fd -> User* map
	users.erase(usr->sock->fd());
	
	freeUserID(usr->id);
	
	// clear any idle timer associated with this user.
	std::set<User*>::iterator tui(utimer.find(usr));
	if (tui != utimer.end())
		utimer.erase(tui);
	
	delete usr;
	usr = 0;
	
	// Transient mode exit.
	if (fIsSet(opmode, server::mode::Transient) and users.empty())
	{
		state = server::state::Exiting;
	}
}

int Server::init() throw(std::bad_alloc)
{
	srand(time(0) - 513); // FIXME
	
	if (lsock.create() == INVALID_SOCKET)
	{
		std::cerr << "! Failed to create a socket." << std::endl;
		return -1;
	}
	
	lsock.block(false); // nonblocking
	lsock.reuse(true); // reuse address
	
	bool bound = false;
	for (int bport=lo_port; bport < hi_port+1; bport++)
	{
		#ifdef IPV6_SUPPORT
		if (lsock.bindTo("::", bport) == SOCKET_ERROR)
		#else
		if (lsock.bindTo("0.0.0.0", bport) == SOCKET_ERROR)
		#endif
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
		std::cerr << "Failed to bind to any port" << std::endl;
		return -1;
	}
	
	if (lsock.listen() == SOCKET_ERROR)
	{
		std::cerr << "Failed to open listening port." << std::endl;
		return -1;
	}
	
	std::cout << "Listening on: " << lsock.address() << std::endl;
	
	if (!ev.init())
		return -1;
	
	ev.add(lsock.fd(), ev.read);
	
	state = server::state::Init;
	
	return 0;
}

bool Server::validateUserName(User*& usr) const throw()
{
	assert(usr != 0);
	
	if (!fIsSet(requirements, protocol::requirements::EnforceUnique))
		return true;
	
	if (usr->nlen == 0)
	{
		#ifndef NDEBUG
		std::cerr << "Name: zero length" << std::endl;
		#endif
		
		return false;
	}
	
	std::map<fd_t, User*>::const_iterator ui(users.begin());
	for (; ui != users.end(); ui++)
	{
		if (ui->second == usr) continue; // skip self
		
		if (usr->nlen == ui->second->nlen
			and (memcmp(usr->name, ui->second->name, usr->nlen) == 0))
		{
			#ifndef NDEBUG
			std::cerr << "Name: conflicts with user #"
				<< static_cast<int>(ui->second->id) << std::endl;
			#endif
			return false;
		}
	}
	
	return true;
}

bool Server::validateSessionTitle(Session*& session) const throw()
{
	assert(session != 0);
	
	if (!fIsSet(requirements, protocol::requirements::EnforceUnique))
		return true;
	
	if (session->len == 0) return false;
	
	std::map<uint8_t, Session*>::const_iterator si(sessions.begin());
	for (; si != sessions.end(); si++)
	{
		if (si->second == session) continue; // skip self
		
		if (session->len == si->second->len
			and (memcmp(session->title, si->second->title, session->len) == 0))
		{
			return false;
		}
	}
	
	return true;
}

void Server::cullIdlers() throw()
{
	// cull idlers
	User *usr;
	std::set<User*>::iterator tui(utimer.begin());
	do
	{
		usr = *tui;
		
		if (usr->deadtime <= current_time)
		{
			#ifndef NDEBUG
			std::cout << "Killing idle user: "
				<< static_cast<int>(usr->id) << std::endl;
			#endif
			
			utimer.erase(tui);
			uRemove(usr, protocol::user_event::TimedOut);
			
			tui = utimer.begin();
		}
		else
		{
			if (usr->deadtime < next_timer)
			{
				// re-schedule next culling to come sooner
				next_timer = usr->deadtime;
			}
			
			tui++;
		}
	}
	while (tui != utimer.end());
}

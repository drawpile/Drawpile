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

#if defined(HAVE_ZLIB)
	#include <zlib.h>
#endif

#include <limits>

#include <ctime>
#include <getopt.h> // for command-line opts
#include <cstdlib>
#include <iostream>

#include <vector>
#include <map>

// iterators
#if defined(HAVE_HASH_MAP)
typedef __gnu_cxx::hash_map<uint8_t, Session*>::iterator sessions_i;
typedef __gnu_cxx::hash_map<uint8_t, Session*>::const_iterator sessions_const_i;
typedef __gnu_cxx::hash_map<fd_t, User*>::iterator users_i;
typedef __gnu_cxx::hash_map<fd_t, User*>::const_iterator users_const_i;
#else
typedef std::map<uint8_t, Session*>::iterator sessions_i;
typedef std::map<uint8_t, Session*>::const_iterator sessions_const_i;
typedef std::map<fd_t, User*>::iterator users_i;
typedef std::map<fd_t, User*>::const_iterator users_const_i;
#endif

typedef std::multimap<User*, User*>::iterator tunnel_i;
typedef std::multimap<User*, User*>::const_iterator tunnel_const_i;

#if defined(HAVE_SLIST)
	#include <ext/slist>
typedef __gnu_cxx::slist<User*>::iterator userlist_i;
typedef __gnu_cxx::slist<User*>::const_iterator userlist_const_i;
#else
	#include <list>
typedef std::list<User*>::iterator userlist_i;
typedef std::list<User*>::const_iterator userlist_const_i;
#endif

typedef std::set<User*>::iterator userset_i;
typedef std::set<User*>::const_iterator userset_const_i;

Server::Server() throw()
	: state(server::state::None),
	password(0),
	a_password(0),
	pw_len(0),
	a_pw_len(0),
	user_limit(0),
	session_limit(srv_defaults::session_limit),
	max_subscriptions(srv_defaults::max_subscriptions),
	name_len_limit(srv_defaults::name_len_limit),
	time_limit(srv_defaults::time_limit),
	current_time(0),
	next_timer(0),
	hi_port(protocol::default_port),
	lo_port(protocol::default_port),
	min_dimension(srv_defaults::min_dimension),
	requirements(0),
	extensions(
		protocol::extensions::Chat|protocol::extensions::Palette
		#ifdef HAVE_ZLIB
		|protocol::extensions::Deflate
		#endif // HAVE_ZLIB
	),
	enforceUnique(false),
	wideStrings(false),
	noGlobalChat(false),
	#ifdef HAVE_ZLIB
	extDeflate(true),
	#else
	extDeflate(false),
	#endif
	extPalette(true),
	extChat(true),
	default_user_mode(protocol::user_mode::None),
	Transient(false),
	LocalhostAdmin(false),
	DaemonMode(false),
	blockDuplicateConnections(true)
	// stats
	#ifndef NDEBUG
	,
	protocolReallocation(0),
	largestLinkList(0),
	largestOutputBuffer(0),
	largestInputBuffer(0),
	bufferRepositions(0),
	bufferResets(0),
	discardedCompressions(0),
	smallestCompression(std::numeric_limits<size_t>::max()),
	deflateSaved(0),
	linkingSaved(0)
	#endif
{
	for (uint8_t i=0; i != std::numeric_limits<uint8_t>::max(); ++i)
	{
		user_ids.push(i+1);
		session_ids.push(i+1);
	}
}

Server::~Server() throw()
{
}

inline
const uint8_t Server::getUserID() throw()
{
	if (user_ids.empty())
		return protocol::null_user;
	
	const uint8_t n = user_ids.front();
	user_ids.pop();
	
	return n;
}

inline
const uint8_t Server::getSessionID() throw()
{
	if (session_ids.empty())
		return protocol::Global;
	
	const uint8_t n = session_ids.front();
	session_ids.pop();
	
	return n;
}

inline
void Server::freeUserID(const uint8_t id) throw()
{
	assert(id != protocol::null_user);
	
	// unfortunately queue can't be iterated, so we can't test if the ID is valid
	
	user_ids.push(id);
}

inline
void Server::freeSessionID(const uint8_t id) throw()
{
	assert(id != protocol::Global);
	
	// unfortunately queue can't be iterated, so we can't test if the ID is valid
	
	session_ids.push(id);
}

inline
void Server::uRegenSeed(User& usr) const throw()
{
	usr.seed[0] = (rand() % 255) + 1;
	usr.seed[1] = (rand() % 255) + 1;
	usr.seed[2] = (rand() % 255) + 1;
	usr.seed[3] = (rand() % 255) + 1;
}

inline
message_ref Server::msgAuth(User& usr, const uint8_t session) const throw(std::bad_alloc)
{
	protocol::Authentication* auth = new protocol::Authentication;
	
	assert(inBoundsOf<uint8_t>(session));
	
	auth->session_id = session;
	
	uRegenSeed(usr);
	
	memcpy(auth->seed, usr.seed, protocol::password_seed_size);
	
	return message_ref(auth);
}

inline
message_ref Server::msgHostInfo() const throw(std::bad_alloc)
{
	return message_ref(
		new protocol::HostInfo(
			sessions.size(),
			session_limit,
			users.size(),
			user_limit,
			name_len_limit,
			max_subscriptions,
			requirements,
			extensions
		)
	);
}

inline
message_ref Server::msgSessionInfo(const Session& session) const throw(std::bad_alloc)
{
	protocol::SessionInfo *nfo = new protocol::SessionInfo(
		session.width,
		session.height,
		session.owner,
		session.users.size(),
		session.limit,
		session.mode,
		session.getFlags(),
		session.level,
		session.title_len,
		new char[session.title_len]
	);
	
	nfo->session_id = session.id;
	
	memcpy(nfo->title, session.title, session.title_len);
	
	return message_ref(nfo);
}

inline
message_ref Server::msgError(const uint8_t session, const uint16_t code) const throw(std::bad_alloc)
{
	protocol::Error *err = new protocol::Error(code);
	err->session_id = session;
	return message_ref(err);
}

inline
message_ref Server::msgAck(const uint8_t session, const uint8_t type) const throw(std::bad_alloc)
{
	protocol::Acknowledgement *ack = new protocol::Acknowledgement(type);
	ack->session_id = session;
	return message_ref(ack);
}

inline
message_ref Server::msgSyncWait(const uint8_t session_id) const throw(std::bad_alloc)
{
	message_ref sync_ref(new protocol::SyncWait);
	sync_ref->session_id = session_id;
	return sync_ref;
}

// May delete User*
inline
void Server::uWrite(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uWrite(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	// if buffer is empty
	if (usr->output.isEmpty())
	{
		assert(!usr->queue.empty());
		
		const std::deque<message_ref>::iterator f_msg(usr->queue.begin());
		std::deque<message_ref>::iterator l_msg(f_msg+1), iter(f_msg);
		size_t links=1;
		for (; l_msg != usr->queue.end(); ++l_msg, ++iter, ++links)
		{
			if (links == std::numeric_limits<uint8_t>::max()
				or ((*l_msg)->type != (*f_msg)->type)
				or ((*l_msg)->user_id != (*f_msg)->user_id)
				or ((*l_msg)->session_id != (*f_msg)->session_id)
			)
				break; // type changed or reached maximum size of linked list
			
			// create linked list
			(*l_msg)->prev = boost::get_pointer(*iter);
			(*iter)->next = boost::get_pointer(*l_msg);
		}
		
		#ifndef NDEBUG
		if (links > largestLinkList)
			largestLinkList = links; // stats
		#endif
		
		//msg->user_id = id;
		size_t len=0, size=usr->output.canWrite();
		
		// serialize message/s
		char* buf = (*--l_msg)->serialize(len, usr->output.wpos, size);
		
		// in case new buffer was allocated
		if (buf != usr->output.wpos)
		{
			#ifndef NDEBUG
			++protocolReallocation; // stats
			
			std::cout << __LINE__ << ": Output buffer was too small!" << std::endl
				<< "Original size: " << usr->output.canWrite()
				<< ", actually needed: " << size << std::endl
				<< "... for user #" << static_cast<int>(usr->id) << std::endl;
			#endif
			
			usr->output.setBuffer(buf, size);
			#ifndef NDEBUG
			++bufferResets; // stats
			if (usr->output.size > largestOutputBuffer)
				largestOutputBuffer = usr->output.size; // stats
			#endif
		}
		
		usr->output.write(len);
		
		#if defined(HAVE_ZLIB)
		if (usr->ext_deflate
			and usr->output.canRead() > 300
			and (*f_msg)->type != protocol::type::Raster)
		{
			// TODO: Move to separate function
			
			char* temp;
			unsigned long buffer_len = len + 12;
			// make the potential new buffer generous in its size
			
			bool inBuffer;
			
			if (usr->output.canWrite() < buffer_len)
			{ // can't write continuous stream of data in buffer
				assert(usr->output.free() < buffer_len);
				size = buffer_len*2048+1024;
				temp = new char[size];
				inBuffer = false;
			}
			else
			{
				temp = usr->output.wpos;
				inBuffer = true;
			}
			
			const int r = compress2(reinterpret_cast<unsigned char*>(temp), &buffer_len, reinterpret_cast<unsigned char*>(usr->output.rpos), len, 5);
			
			assert(r != Z_STREAM_ERROR);
			
			switch (r)
			{
			default:
			case Z_OK:
				#ifndef NDEBUG
				std::cout << "zlib: " << len << "B compressed down to " << buffer_len << "B." << std::endl;
				#endif
				
				// compressed size was equal or larger than original size
				if (buffer_len >= len)
				{
					#ifndef NDEBUG
					++discardedCompressions; // stats
					#endif
					
					if (!inBuffer)
						delete [] temp;
					
					break;
				}
				
				#ifndef NDEBUG
				if (len < smallestCompression)
					smallestCompression = len; // stats
				#endif
				
				usr->output.read(len);
				
				if (inBuffer)
				{
					usr->output.write(buffer_len);
					size = usr->output.canWrite();
				}
				else
				{
					usr->output.setBuffer(temp, size);
					#ifndef NDEBUG
					++bufferResets; // stats
					if (usr->output.size > largestOutputBuffer)
						largestOutputBuffer = usr->output.size; // stats
					#endif
					usr->output.write(buffer_len);
				}
				
				{
					const protocol::Deflate t_deflate(len, buffer_len, temp);
					
					#if 0 // what was this?
					if (usr->output.canWrite() < buffer_len + 9)
						if (usr->output.free() >= buffer_len + 9)
						{
							reposition();
							size = usr->output.canWrite();
						}
					#endif
					
					buf = t_deflate.serialize(len, usr->output.wpos, size);
					
					if (buf != usr->output.wpos)
					{
						#ifndef NDEBUG
						++protocolReallocation; // stats
						#endif
						
						#ifndef NDEBUG
						if (!inBuffer)
						{
							std::cout << __LINE__ << ": Pre-allocated buffer was too small!" << std::endl
								<< "Allocated: " << buffer_len*2+1024
								<< ", actually needed: " << size << std::endl
								<< "... for user #" << static_cast<int>(usr->id) << std::endl;
						}
						else
						{
							std::cout << __LINE__ << ": Output buffer was too small!" << std::endl
								<< "Original size: " << usr->output.canWrite()
								<< ", actually needed: " << size << std::endl
								<< "... for user #" << static_cast<int>(usr->id) << std::endl;
						}
						#endif
						usr->output.setBuffer(buf, size);
						#ifndef NDEBUG
						++bufferResets; // stats
						if (usr->output.size > largestOutputBuffer)
							largestOutputBuffer = usr->output.size; // stats
						#endif
					}
					
					usr->output.write(len);
					
					// cleanup
				}
				break;
			case Z_MEM_ERROR:
				throw std::bad_alloc();
			case Z_BUF_ERROR:
				#ifndef NDEBUG
				std::cerr << "zlib: output buffer is too small." << std::endl
					<< "source size: " << len << ", target size: " << buffer_len << std::endl;
				#endif
				assert(r != Z_BUF_ERROR);
				
				if (!inBuffer)
					delete [] temp;
				
				break;
			}
		}
		#endif // HAVE_ZLIB
		
		usr->queue.erase(f_msg, ++l_msg);
	}
	
	const int sb = usr->sock.send( usr->output.rpos, usr->output.canRead() );
	
	switch (sb)
	{
	case SOCKET_ERROR:
		switch (usr->sock.getError())
		{
		#ifdef WIN32
		case WSA_IO_PENDING:
		#endif
		case EINTR:
		case EAGAIN:
		case ENOBUFS:
		case ENOMEM:
			// retry
			break;
		default:
			std::cerr << "Error occured while sending to user #"
				<< static_cast<int>(usr->id) << std::endl;
			
			uRemove(usr, protocol::user_event::BrokenPipe);
			break;
		}
		break;
	case 0:
		// no data was sent, and no error occured.
		break;
	default:
		#if defined(DEBUG_SERVER) and !defined(NDEBUG)
		std::cout << "Sent " << sb << " bytes to user #" << static_cast<int>(usr->id) << std::endl;
		#endif
		
		usr->output.read(sb);
		
		if (usr->output.isEmpty())
		{
			#if defined(DEBUG_SERVER) and !defined(NDEBUG)
			std::cout << "Output buffer empty, rewinding." << std::endl;
			#endif
			
			// rewind buffer
			usr->output.rewind();
			
			// remove fd from write list if no buffers left.
			if (usr->queue.empty())
			{
				#if defined(DEBUG_SERVER) and !defined(NDEBUG)
				std::cout << "Output queue empty, clearing event flag." << std::endl;
				#endif
				
				fClr(usr->events, ev.write);
				ev.modify(usr->sock.fd(), usr->events);
			}
			else
			{
				#if defined(DEBUG_SERVER) and !defined(NDEBUG)
				std::cout << "Still have " << usr->queue.size() << " message/s in queue." << std::endl
					<< "... first is of type: " << static_cast<int>(usr->queue.front()->type) << std::endl;
				#endif
			}
		}
		break;
	}
}

// May delete User*
inline
void Server::uRead(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uRead(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	if (usr->input.free() == 0)
	{
		// buffer full
		#ifndef NDEBUG
		std::cerr << "Input buffer full, increasing size by 4 kiB." << std::endl;
		#endif
		
		usr->input.resize(usr->input.size + 4096);
		
		#ifndef NDEBUG
		if (usr->input.size > largestInputBuffer)
			largestInputBuffer = usr->input.size;
		#endif
	}
	else if (usr->input.canWrite() == 0)
	{
		// can't write, but buffer isn't full.
		usr->input.reposition();
		#ifndef NDEBUG
		++bufferRepositions;
		#endif
	}
	
	const int rb = usr->sock.recv(
		usr->input.wpos,
		usr->input.canWrite()
	);
	
	switch (rb)
	{
	case SOCKET_ERROR:
		switch (usr->sock.getError())
		{
		case EAGAIN:
		case EINTR:
			// retry later
			#if defined(DEBUG_SERVER) and !defined(NDEBUG)
			std::cerr << "# Operation would block / interrupted" << std::endl;
			#endif
			break;
		default:
			std::cerr << "Unrecoverable error occured while reading from user #"
				<< static_cast<int>(usr->id) << std::endl;
			
			uRemove(usr, protocol::user_event::BrokenPipe);
			break;
		}
		break;
	case 0:
		std::cerr << "User #" << static_cast<int>(usr->id) << " disconnected." << std::endl;
		uRemove(usr, protocol::user_event::Disconnect);
		break;
	default:
		usr->input.write(rb);
		uProcessData(usr);
		break;
	}
}

// calls uHandleMsg() and uHandleLogin()
inline
void Server::uProcessData(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uProcessData(user: "
		<< static_cast<int>(usr->id) << ", in buffer: "
		<< usr->input.left << " bytes)" << std::endl;
	#endif
	
	while (!usr->input.isEmpty())
	{
		if (usr->inMsg == 0)
		{
			usr->inMsg = protocol::getMessage(usr->input.rpos[0]);
			if (usr->inMsg == 0)
			{
				// unknown message type
				std::cerr << "Unknown data from user #"
					<< static_cast<int>(usr->id) << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
		}
		
		assert(usr->inMsg->type == static_cast<uint8_t>(usr->input.rpos[0]));
		
		size_t cread = usr->input.canRead();
		size_t len = usr->inMsg->reqDataLen(usr->input.rpos, cread);
		if (len > usr->input.left)
			return; // need more data
		else if (len > cread)
		{
			// so, we reposition the buffer
			usr->input.reposition();
			#ifndef NDEBUG
			++bufferRepositions;
			#endif
			continue;
		}
		
		usr->input.read( usr->inMsg->unserialize(usr->input.rpos, cread) );
		
		#if 0 // isValid() can't be used before it is properly implemented!
		if (!usr->inMsg->isValid())
		{
			std::cerr << "Message is invalid, dropping user." << std::endl;
			uRemove(usr, protocol::user_event::Dropped);
			return;
		}
		#endif
		
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
		{ // user is still alive.
			delete usr->inMsg;
			usr->inMsg = 0;
		}
		else // quite dead
			break;
	}
	
	// rewind circular buffer
	usr->input.rewind();
}

inline
message_ref Server::msgUserEvent(const User& usr, const uint8_t session_id, const uint8_t event) const throw(std::bad_alloc)
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::msgUserEvent(user: "
		<< static_cast<int>(usr.id) << ")" << std::endl;
	#endif
	
	usr_session_const_i usi(usr.sessions.find(session_id));
	assert(usi != usr.sessions.end());
	
	protocol::UserInfo *uevent = new protocol::UserInfo(
		usi->second->getMode(),
		event,
		usr.name_len,
		(usr.name_len == 0 ? 0 : new char[usr.name_len])
	);
	
	uevent->user_id = usr.id;
	uevent->session_id = session_id;
	
	memcpy(uevent->name, usr.name, usr.name_len);
	
	return message_ref(uevent);
}

// May delete User*
inline
void Server::uHandleMsg(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uHandleMsg(user: " << static_cast<int>(usr->id)
		<< ", type: " << static_cast<int>(usr->inMsg->type) << ")" << std::endl;
	#endif
	
	assert(usr->state == uState::active);
	
	switch (usr->inMsg->type)
	{
	case protocol::type::ToolInfo:
		usr->cacheTool(static_cast<protocol::ToolInfo*>(usr->inMsg));
	case protocol::type::StrokeInfo:
	case protocol::type::StrokeEnd:
		switch (usr->inMsg->type)
		{
		case protocol::type::StrokeInfo:
			++usr->strokes;
			break;
		case protocol::type::StrokeEnd:
			usr->strokes = 0;
			break;
		}
		
		// no session selected
		if (usr->session == 0)
		{
			#ifndef NDEBUG
			if (usr->strokes == 1)
				std::cerr << "User #" << static_cast<int>(usr->id)
					<< " attempts to draw on null session." << std::endl;
			#endif
			break;
		}
		
		#ifdef LAYER_SUPPORT
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
			
			break;
		}
		#endif
		
		// user or session is locked
		if (usr->session->locked or usr->a_locked)
		{
			// TODO: Warn user?
			#ifndef NDEBUG
			if (usr->strokes == 1)
			{
				std::cerr << "User #" << static_cast<int>(usr->id)
					<< " tries to draw, despite the lock." << std::endl;
				
				if (usr->session->locked)
					std::cerr << "Clarification: Sesssion #"
						<< static_cast<int>(usr->session->id) << " is locked." << std::endl;
				else
					std::cerr << "Clarification: User is locked in session #"
						<< static_cast<int>(usr->session->id) << "." << std::endl;
			}
			#endif
			break;
		}
		
		// make sure the user id is correct
		usr->inMsg->user_id = usr->id;
		
		Propagate(*usr->session, message_ref(usr->inMsg), (usr->c_acks ? usr : 0));
		
		usr->inMsg = 0;
		break;
	case protocol::type::Acknowledgement:
		uHandleAck(usr);
		break;
	case protocol::type::SessionSelect:
		if (usr->makeActive(usr->inMsg->session_id))
		{
			usr->inMsg->user_id = usr->id;
			Propagate(*usr->session, message_ref(usr->inMsg), (usr->c_acks ? usr : 0));
			usr->inMsg = 0;
		}
		else
		{
			uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			usr->session = 0;
		}
		break;
	case protocol::type::UserInfo:
		// TODO?
		break;
	case protocol::type::Raster:
		uTunnelRaster(*usr);
		break;
	#if defined(HAVE_ZLIB)
	case protocol::type::Deflate:
		DeflateReprocess(usr);
		break;
	#endif
	case protocol::type::Chat:
		// TODO: Check session for deaf flag
	case protocol::type::Palette:
		{
			const usr_session_const_i usi(usr->sessions.find(usr->inMsg->session_id));
			if (usi == usr->sessions.end())
				uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			else
			{
				message_ref pmsg(usr->inMsg);
				pmsg->user_id = usr->id;
				Propagate(*usi->second->session, pmsg, (usr->c_acks ? usr : 0));
				usr->inMsg = 0;
			}
		}
		break;
	case protocol::type::Unsubscribe:
		{
			const sessions_i smi(sessions.find(usr->inMsg->session_id));
			if (smi == sessions.end())
				uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			else
			{
				uSendMsg(*usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
				uLeaveSession(*usr, smi->second);
			}
		}
		break;
	case protocol::type::Subscribe:
		if (usr->syncing == protocol::Global)
		{
			const sessions_const_i si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				break;
			}
			
			Session *session = si->second;
			
			if (!usr->inSession(usr->inMsg->session_id))
			{
				// Test userlimit
				if (session->canJoin() == false)
					uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
				else if (session->level != usr->level)
					uSendMsg(*usr, msgError(protocol::Global, protocol::error::ImplementationMismatch));
				else if (session->password != 0)
					uSendMsg(*usr, msgAuth(*usr, session->id));
				else // join session
				{
					uSendMsg(*usr, msgAck(usr->inMsg->session_id, usr->inMsg->type));
					uJoinSession(usr, session);
				}
			}
			else
			{
				// already subscribed
				uSendMsg(*usr, msgError(
					usr->inMsg->session_id, protocol::error::InvalidRequest)
				);
			}
		}
		else
		{
			// already syncing some session, so we don't bother handling this request.
			uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
		}
		break;
	case protocol::type::ListSessions:
		if (sessions.size() != 0)
		{
			sessions_const_i si(sessions.begin());
			for (; si != sessions.end(); ++si)
				uSendMsg(*usr, msgSessionInfo(*si->second));
		}
		
		uSendMsg(*usr, msgAck(protocol::Global, usr->inMsg->type));
		break;
	case protocol::type::SessionEvent:
		{
			const sessions_const_i si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
				uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			else
			{
				usr->inMsg->user_id = usr->id;
				Session *session = si->second;
				uSessionEvent(session, usr);
			}
		}
		break;
	case protocol::type::LayerEvent:
		// TODO
		break;
	case protocol::type::LayerSelect:
		{
			protocol::LayerSelect *layer = static_cast<protocol::LayerSelect*>(usr->inMsg);
			
			if (layer->layer_id == usr->a_layer // trying to select current layer
				or (usr->a_layer_lock != protocol::null_layer
				and usr->a_layer_lock != layer->layer_id)) // locked to another
				uSendMsg(*usr, msgError(layer->session_id, protocol::error::InvalidLayer));
			else
			{
				// find layer
				const session_layer_const_i li(usr->session->layers.find(layer->layer_id));
				if (li == usr->session->layers.end()) // target layer doesn't exist
					uSendMsg(*usr, msgError(layer->session_id, protocol::error::UnknownLayer));
				else if (li->second.locked) // target layer is locked
					uSendMsg(*usr, msgError(layer->session_id, protocol::error::LayerLocked));
				else
				{
					// set user id to message
					layer->user_id = usr->id;
					
					// set user's layer
					usr->a_layer = layer->layer_id;
					
					// Tell other users about it
					Propagate(*usr->session, message_ref(layer), (usr->c_acks ? usr : 0));
				}
			}
		}
		break;
	case protocol::type::Instruction:
		uHandleInstruction(usr);
		break;
	case protocol::type::Password:
		{
			sessions_const_i si;
			protocol::Password *msg = static_cast<protocol::Password*>(usr->inMsg);
			if (msg->session_id == protocol::Global)
			{
				// Admin login
				if (a_password == 0)
				{
					std::cerr << "User tries to pass password even though we've disallowed it." << std::endl;
					uSendMsg(*usr, msgError(msg->session_id, protocol::error::PasswordFailure));
					return;
				}
				else
					hash.Update(reinterpret_cast<uint8_t*>(a_password), a_pw_len);
			}
			else
			{
				if (usr->syncing != protocol::Global)
				{
					// already syncing some session, so we don't bother handling this request.
					uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
					break;
				}
				
				si = sessions.find(msg->session_id);
				if (si == sessions.end())
				{
					// session doesn't exist
					uSendMsg(*usr, msgError(msg->session_id, protocol::error::UnknownSession));
					break;
				}
				else if (usr->inSession(msg->session_id))
				{
					// already in session
					uSendMsg(*usr, msgError(msg->session_id, protocol::error::InvalidRequest));
					break;
				}
				else if (si->second->canJoin() == false)
				{
					uSendMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
					break;
				}
				else
					hash.Update(reinterpret_cast<uint8_t*>(si->second->password), si->second->pw_len);
			}
			
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			hash.Reset();
			
			uRegenSeed(*usr);
			
			if (memcmp(digest, msg->data, protocol::password_hash_size) != 0) // mismatch
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::PasswordFailure));
			else
			{
				// ACK the password
				uSendMsg(*usr, msgAck(msg->session_id, msg->type));
				
				if (msg->session_id == protocol::Global)
					usr->isAdmin = true;
				else
				{
					// join session
					Session* session = si->second;
					uJoinSession(usr, session);
				}
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

// May delete User*
#if defined(HAVE_ZLIB)
inline
void Server::DeflateReprocess(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if !defined(NDEBUG)
	std::cout << "Server::DeflateReprocess(user: " << static_cast<int>(usr->id) << ")" << std::endl;
	#endif
	
	protocol::Deflate* stream = static_cast<protocol::Deflate*>(usr->inMsg);
	usr->inMsg = 0;
	
	#if !defined(NDEBUG)
	std::cout << "Compressed: " << stream->length
		<< " bytes, uncompressed: " << stream->uncompressed << " bytes." << std::endl;
	#endif
	
	char* outbuf = new char[stream->uncompressed];
	unsigned long outlen = stream->uncompressed;
	const int r = uncompress(reinterpret_cast<unsigned char*>(outbuf), &outlen, reinterpret_cast<unsigned char*>(stream->data), stream->length);
	
	switch (r)
	{
	default:
	case Z_OK:
		{
			// Store input buffer. (... why?)
			Buffer temp;
			temp << usr->input;
			
			// Set the uncompressed data stream as the input buffer.
			usr->input.setBuffer(outbuf, stream->uncompressed);
			
			#ifndef NDEBUG
			++bufferResets; // stats
			if (usr->input.size > largestInputBuffer)
				largestInputBuffer = usr->input.size; // stats
			#endif
			
			// Process the data.
			uProcessData(usr);
			
			// Since uProcessData might've deleted the user
			if (usr != 0)
				usr->input << temp; // Restore input buffer
		}
		break;
	case Z_MEM_ERROR:
		// TODO: Out of Memory
		throw std::bad_alloc();
		//break;
	case Z_BUF_ERROR:
	case Z_DATA_ERROR:
		// Input buffer corrupted
		std::cout << "Corrupted data from user #" << static_cast<int>(usr->id)
			<< ", dropping." << std::endl;
		uRemove(usr, protocol::user_event::Dropped);
		delete [] outbuf;
		break;
	}
}
#endif

// May delete User*
inline
void Server::uHandleAck(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	const protocol::Acknowledgement *ack = static_cast<protocol::Acknowledgement*>(usr->inMsg);
	
	switch (ack->event)
	{
	case protocol::type::SyncWait:
		{
			// check active session first
			const usr_session_const_i usi(usr->sessions.find(ack->session_id));
			if (usi == usr->sessions.end())
				uSendMsg(*usr, msgError(ack->session_id, protocol::error::NotSubscribed));
			else if (usi->second->syncWait) // duplicate syncwait
				uRemove(usr, protocol::user_event::Dropped);
			else
			{
				usi->second->syncWait = true;
				
				Session *session = usi->second->session;
				--session->syncCounter;
				
				if (session->syncCounter == 0)
				{
					#ifndef NDEBUG
					std::cout << "Synchronizing raster for session #"
						<< static_cast<int>(session->id) << "." << std::endl;
					#endif
					
					SyncSession(session);
				}
				#ifndef NDEBUG
				else
				{
					std::cout << "ACK/SyncWait counter: " << session->syncCounter << std::endl;
				}
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

// Calls uSendMsg()
inline
void Server::uTunnelRaster(User& usr) throw()
{
	assert(usr.inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uTunnelRaster(from: "
		<< static_cast<int>(usr.id) << ")" << std::endl;
	#endif
	
	protocol::Raster *raster = static_cast<protocol::Raster*>(usr.inMsg);
	
	const bool last = (raster->offset + raster->length == raster->size);
	
	if (!usr.inSession(raster->session_id))
	{
		std::cerr << "Raster for unsubscribed session #"
			<< static_cast<int>(raster->session_id)
			<< ", from user #" << static_cast<int>(usr.id)
			<< std::endl;
		
		if (!last)
		{
			message_ref cancel_ref(new protocol::Cancel);
			cancel_ref->session_id = raster->session_id;
			uSendMsg(usr, cancel_ref);
		}
		
		return;
	}
	
	// get users
	const std::pair<tunnel_i,tunnel_i> ft(tunnel.equal_range(&usr));
	if (ft.first == ft.second)
	{
		std::cerr << "Un-tunneled raster from user #"
			<< static_cast<int>(usr.id)
			<< ", for session #" << static_cast<int>(raster->session_id)
			<< std::endl;
		
		// We assume the raster was for user/s who disappeared
		// before we could cancel the request.
		
		if (!last)
		{
			message_ref cancel_ref(new protocol::Cancel);
			cancel_ref->session_id = raster->session_id;
			uSendMsg(usr, cancel_ref);
		}
	}
	else
	{
		// ACK raster
		uSendMsg(usr, msgAck(usr.inMsg->session_id, usr.inMsg->type));
		
		tunnel_i ti(ft.first);
		// Forward raster data to users.
		User *usr_ptr;
		
		message_ref raster_ref(raster);
		
		usr.inMsg = 0; // Avoid premature deletion of raster data.
		
		for (; ti != ft.second; ++ti)
		{
			usr_ptr = ti->second;
			uSendMsg(*usr_ptr, raster_ref);
			if (last) usr_ptr->syncing = false;
		}
		
		// Break tunnel/s if that was the last raster piece.
		if (last) tunnel.erase(&usr);
	}
}

// Calls Propagate, uSendMsg and uLeaveSession
// May delete User*
inline
void Server::uSessionEvent(Session*& session, User*& usr) throw()
{
	assert(session != 0);
	assert(usr != 0);
	assert(usr->inMsg->type == protocol::type::SessionEvent);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uSessionEvent(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	if (!usr->isAdmin and !isOwner(*usr, *session))
	{
		uSendMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
		return;
	}
	
	protocol::SessionEvent *event = static_cast<protocol::SessionEvent*>(usr->inMsg);
	
	switch (event->action)
	{
	case protocol::session_event::Kick:
		{
			const session_usr_const_i sui(session->users.find(event->target));
			if (sui == session->users.end()) // user not found in session
				uSendMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
			else
			{
				Propagate(*session, message_ref(event));
				usr->inMsg = 0;
				User *usr_ptr = sui->second;
				uLeaveSession(*usr_ptr, session, protocol::user_event::Kicked);
			}
		}
		break;
	case protocol::session_event::Lock:
	case protocol::session_event::Unlock:
		if (event->target == protocol::null_user)
		{
			// Lock whole board
			
			#ifndef NDEBUG
			std::cout << "Changing lock state for session #"
				<< static_cast<int>(session->id)
				<< " to "
				<< (event->action == protocol::session_event::Lock ? "enabled" : "disabled") << std::endl;
			#endif
			
			session->locked = (event->action == protocol::session_event::Lock ? true : false);
		}
		else
		{
			// Lock single user
			
			#ifndef NDEBUG
			std::cout << "Changing lock state for user #"
				<< static_cast<int>(event->target)
				<< " to "
				<< (event->action == protocol::session_event::Lock ? "enabled" : "disabled")
				<< " in session #" << static_cast<int>(session->id) << std::endl;
			#endif
			
			// Find user
			const session_usr_const_i session_usr(session->users.find(event->target));
			if (session_usr == session->users.end())
			{
				uSendMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
				break;
			}
			User *usr = session_usr->second;
			
			// Find user's session instance (SessionData*)
			const usr_session_const_i usi(usr->sessions.find(session->id));
			if (usi == usr->sessions.end())
				uSendMsg(*usr, msgError(session->id, protocol::error::NotInSession));
			else if (event->aux == protocol::null_layer)
			{
				usi->second->locked = (event->action == protocol::session_event::Lock);
				
				// Copy active session
				if (usr->session->id == event->session_id)
					usr->a_locked = usi->second->locked;
			}
			else if (event->action == protocol::session_event::Lock)
			{
				#ifndef NDEBUG
				std::cout << "Lock ignored, limiting user to layer #"
					<< static_cast<int>(event->aux) << std::endl;
				#endif
				
				usi->second->layer_lock = event->aux;
				// copy to active session
				if (session->id == usr->session->id)
					usr->a_layer_lock = event->aux;
				
				// Null-ize the active layer if the target layer is not the active one.
				if (usi->second->layer != usi->second->layer_lock)
					usi->second->layer = protocol::null_layer;
				if (usr->session->id == event->session_id)
					usr->layer = protocol::null_layer;
			}
			else // unlock
			{
				#ifndef NDEBUG
				std::cout << "Lock ignored, releasing user from layer #"
					<< static_cast<int>(event->aux) << std::endl;
				#endif
				
				usi->second->layer_lock = protocol::null_layer;
				
				// copy to active session
				if (session->id == usr->session->id)
					usr->a_layer_lock = protocol::null_layer;
			}
		}
		
		Propagate(*session, message_ref(event));
		usr->inMsg = 0;
		
		break;
	case protocol::session_event::Delegate:
		{
			const session_usr_const_i sui(session->users.find(event->target));
			if (sui == session->users.end()) // User not found
				uSendMsg(*usr, msgError(session->id, protocol::error::NotInSession));
			else
			{
				session->owner = sui->second->id;
				Propagate(*session, message_ref(event), (usr->c_acks ? usr : 0));
				usr->inMsg = 0;
			}
		}
		break;
	case protocol::session_event::Mute:
	case protocol::session_event::Unmute:
		{
			users_i ui(users.find(event->target));
			if (ui == users.end())
				uSendMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
			else
			{
				usr_session_const_i usi(ui->second->sessions.find(session->id));
				if (usi == ui->second->sessions.end()) // user not in session
					uSendMsg(*usr, msgError(session->id, protocol::error::NotInSession));
				else
				{
					// Set mode
					usi->second->muted = (event->action == protocol::session_event::Mute);
					
					// Copy to active session's mode, too.
					if (usr->session->id == event->target)
						usr->a_muted = usi->second->muted;
					
					Propagate(*session, message_ref(event), (usr->c_acks ? usr : 0));
					
					usr->inMsg = 0;
				}
			}
		}
		break;
	case protocol::session_event::Persist:
		// TODO
		break;
	case protocol::session_event::CacheRaster:
		// TODO
		break;
	default:
		std::cerr << "Unknown session action: "
			<< static_cast<int>(event->action) << ")" << std::endl;
		uRemove(usr, protocol::user_event::Violation);
		return;
	}
}

inline
bool Server::isOwner(const User& usr, const Session& session) const throw()
{
	return session.owner == usr.id;
}

// Calls uSendMsg, Propagate, sRemove, uLeaveSession
// May delete User*
inline
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
		if (!usr->isAdmin) { break; }
		// limited scope for switch/case
		{
			const uint8_t session_id = getSessionID();
			static const size_t crop = sizeof(uint16_t)*2;
			
			if (session_id == protocol::Global)
			{
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::SessionLimit));
				break;
			}
			else if (msg->aux_data < 2)
			{
				#ifndef NDEBUG
				std::cerr << "Attempted to create single user session." << std::endl;
				#endif
				
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::InvalidData));
				break;
			}
			else if (msg->length < crop)
			{
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			// temporaries
			uint16_t w, h;
			
			memcpy_t(w, msg->data);
			memcpy_t(h, msg->data+sizeof(uint16_t));
			
			bswap(w);
			bswap(h);
			
			if (w < min_dimension or h < min_dimension)
			{
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::TooSmall));
				break;
			}
			
			// more temporaries
			uint8_t title_len;
			char* title_tmp;
			if (msg->length < crop)
			{
				std::cerr << "Invalid data size in instruction: 'Create'." << std::endl;
				uSendMsg(*usr, msgError(protocol::Global, protocol::error::InvalidData));
				break;
			}
			else if (msg->length != crop)
			{
				title_len = (msg->length - crop);
				title_tmp = msg->data;
				memmove(title_tmp, title_tmp+crop, title_len);
				title_tmp[title_len] = '\0';
			}
			else
			{
				title_len = 0;
				title_tmp = 0;
				#ifndef NDEBUG
				std::cout << "No title set for session." << std::endl;
				#endif
			}
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateSessionTitle(title_tmp, title_len))
			{
				#ifndef NDEBUG
				std::cerr << "Session title not unique." << std::endl;
				#endif
				
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::NotUnique));
			}
			else
			{
				msg->data = 0; // prevent title from being deleted
				
				Session *session = new Session(
					session_id, // identifier
					msg->aux_data2, // mode
					msg->aux_data, // user limit
					usr->id, // owner
					w, // width
					h, // height
					usr->level, // inherit user's feature level
					title_len,
					title_tmp
				);
				
				sessions[session->id] = session;
				
				std::cout << "Session #" << static_cast<int>(session->id) << " created!" << std::endl
					<< "  Dimensions: " << session->width << " x " << session->height << std::endl
					<< "  User limit: " << static_cast<int>(session->limit)
					<< ", default mode: " << static_cast<int>(session->mode)
					<< ", level: " << static_cast<int>(session->level) << std::endl
					<< "  Owner: " << static_cast<int>(usr->id)
					<< ", from: " << usr->sock.address() << std::endl;
				
				uSendMsg(*usr, msgAck(msg->session_id, msg->type));
			}
		}
		return; // because session owner might've done this
	case protocol::admin::command::Destroy:
		{
			const sessions_const_i si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uSendMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			// tell users session was lost
			message_ref err = msgError(session->id, protocol::error::SessionLost);
			Propagate(*session, err, 0, true);
			
			// clean session users
			session_usr_const_i sui(session->users.begin());
			User* usr_ptr;
			for (; sui != session->users.end(); ++sui)
			{
				usr_ptr = sui->second;
				uLeaveSession(*usr_ptr, session, protocol::user_event::None);
			}
			
			// destruct
			sRemove(session);
		}
		break;
	case protocol::admin::command::Alter:
		{
			const sessions_const_i si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uSendMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			static const int crop = sizeof(uint16_t)*2;
			
			if (msg->length < crop)
			{
				std::cerr << "Protocol violation from user #"
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
				std::cerr << "Protocol violation from user #"
					<< static_cast<int>(usr->id) << std::endl
					<< "Reason: Attempted to reduce session's canvas size." << std::endl;
				uRemove(usr, protocol::user_event::Violation);
				return;
			}
			
			session->height = height,
			session->width = width;
			
			// new title
			if (msg->length > crop)
			{
				const int nlen = msg->length - crop;
				if (session->title_len < nlen)
				{
					// re-alloc session title only if the new one is longer than old one
					delete [] session->title;
					session->title = new char[nlen];
				}
				
				memcpy(session->title, msg->data+crop, nlen);
				session->title_len = nlen;
			}
			
			#ifndef NDEBUG
			std::cout << "Session #" << static_cast<int>(session->id) << " altered!" << std::endl
				<< "  Dimensions: " << width << " x " << height << std::endl
				<< "  User limit: " << static_cast<int>(session->limit) << ", default mode: "
				<< static_cast<int>(session->mode) << std::endl;
			#endif // NDEBUG
			
			Propagate(*session, msgSessionInfo(*session));
		}
		break;
	case protocol::admin::command::Password:
		if (msg->session_id == protocol::Global)
		{
			if (!usr->isAdmin) { break; }
			
			if (password != 0)
				delete [] password;
			
			password = msg->data;
			pw_len = msg->length;
			
			msg->data = 0;
			//msg->length = 0;
			
			#ifndef NDEBUG
			std::cout << "Server password changed." << std::endl;
			#endif
			
			uSendMsg(*usr, msgAck(msg->session_id, msg->type));
		}
		else
		{
			const sessions_const_i si(sessions.find(msg->session_id));
			if (si == sessions.end())
			{
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uSendMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
				return;
			}
			
			#ifndef NDEBUG
			std::cout << "Password set for session #"
				<< static_cast<int>(msg->session_id) << std::endl;
			#endif
			
			if (session->password != 0)
				delete [] session->password;
			
			session->password = msg->data;
			session->pw_len = msg->length;
			msg->data = 0;
			
			uSendMsg(*usr, msgAck(msg->session_id, msg->type));
			return;
		}
		break;
	case protocol::admin::command::Authenticate:
		#ifndef NDEBUG
		std::cout << "User #" << static_cast<int>(usr->id) << " wishes to authenticate itself as an admin." << std::endl;
		#endif
		if (a_password == 0) // no admin password set
			uSendMsg(*usr, msgError(msg->session_id, protocol::error::Unauthorized));
		else // request password
			uSendMsg(*usr, msgAuth(*usr, protocol::Global));
		return;
	case protocol::admin::command::Shutdown:
		if (!usr->isAdmin) { break; }
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
	if (!usr->isAdmin)
		uSendMsg(*usr, msgError(msg->session_id, protocol::error::Unauthorized));
}

inline
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
			
			if (msg->length > name_len_limit)
			{
				#ifndef NDEBUG
				std::cerr << "Name too long: " << static_cast<int>(msg->length)
					<< " > " << static_cast<int>(name_len_limit) << std::endl;
				#endif
				
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::TooLong));
				
				//uRemove(usr, protocol::user_event::Dropped);
				break;
			}
			
			// assign user their own name
			usr->name = msg->name;
			usr->name_len = msg->length;
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateUserName(usr))
			{
				#ifndef NDEBUG
				std::cerr << "User name not unique." << std::endl;
				#endif
				
				uSendMsg(*usr, msgError(msg->session_id, protocol::error::NotUnique));
				
				//uRemove(usr, protocol::user_event::Dropped);
				break;
			}
			
			// assign message the user's id (for sending back)
			msg->user_id = usr->id;
			
			// null the message's name information, so they don't get deleted
			msg->length = 0;
			msg->name = 0;
			
			usr->setAMode(default_user_mode);
			
			if (LocalhostAdmin)
			{
				const std::string IPPort(usr->sock.address());
				const std::string::size_type ns(IPPort.find_last_of(":", IPPort.length()-1));
				assert(ns != std::string::npos);
				const std::string IP(IPPort.substr(0, ns));
				
				// Loopback address
				if (IP == std::string(
					#ifdef IPV6_SUPPORT
					"::1"
					#else
					"127.0.0.1"
					#endif // IPV6_SUPPORT
				))
					usr->isAdmin = true;
			}
			
			msg->mode = usr->getAMode();
			
			usr->state = uState::active;
			usr->deadtime = 0;
			usr->inMsg = 0;
			
			// remove fake timer
			utimer.erase(utimer.find(usr));
			
			// reply
			uSendMsg(*usr, message_ref(msg));
		}
		else // wrong message type
			uRemove(usr, protocol::user_event::Violation);
		break;
	case uState::login_auth:
		if (usr->inMsg->type == protocol::type::Password)
		{
			const protocol::Password *msg = static_cast<protocol::Password*>(usr->inMsg);
			
			hash.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			hash.Reset();
			
			if (memcmp(digest, msg->data, protocol::password_hash_size) == 0)
			{
				uSendMsg(*usr, msgAck(msg->session_id, msg->type)); // ACK
				uRegenSeed(*usr); // mangle seed
				usr->state = uState::login; // set state
				uSendMsg(*usr, msgHostInfo()); // send hostinfo
			}
			else  // mismatch
				uRemove(usr, protocol::user_event::Dropped);
		}
		else // not a password
			uRemove(usr, protocol::user_event::Violation);
		break;
	case uState::init:
		if (usr->inMsg->type == protocol::type::Identifier)
		{
			const protocol::Identifier *ident = static_cast<protocol::Identifier*>(usr->inMsg);
			
			if (memcmp(ident->identifier, protocol::identifier_string, protocol::identifier_size) != 0)
			{
				#ifndef NDEBUG
				std::cerr << "Protocol string mismatch" << std::endl;
				#endif
				
				uRemove(usr, protocol::user_event::Violation);
			}
			else if (ident->revision != protocol::revision)
			{
				#ifndef NDEBUG
				std::cerr << "Protocol revision mismatch ---  "
					<< "expected: " << protocol::revision
					<< ", got: " << ident->revision << std::endl;
				#endif
				
				// TODO: Implement some compatible way of announcing incompatibility?
				
				uRemove(usr, protocol::user_event::Dropped);
			}
			else
			{
				if (password == 0) // no password set
				{
					usr->state = uState::login;
					uSendMsg(*usr, msgHostInfo());
				}
				else
				{
					usr->state = uState::login_auth;
					uSendMsg(*usr, msgAuth(*usr, protocol::Global));
				}
				
				usr->level = ident->level; // feature level used by client
				usr->setCapabilities(ident->flags);
				usr->setExtensions(ident->extensions);
				usr->setAMode(default_user_mode);
				//if (!usr->ext_chat)
				//	usr->a_deaf = true;
			}
		}
		else
		{
			std::cerr << "Invalid data from user #"
				<< static_cast<int>(usr->id) << ", dropping connection." << std::endl;
			uRemove(usr, protocol::user_event::Dropped);
		}
		break;
	case uState::dead: // this should never happen
		std::cerr << "I see dead people." << std::endl;
		uRemove(usr, protocol::user_event::Dropped);
		break;
	default:
		assert(!"user state was something strange");
		break;
	}
}

// Calls uSendMsg
inline
void Server::Propagate(const Session& session, message_ref msg, User* source, const bool toAll) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::Propagate(session: " << static_cast<int>(session.id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	#endif
	
	//the assert fails with messages that are session selected
	//assert(session.id == msg->session_id);
	
	// Send ACK for the message we're about to share..
	if (source != 0)
		uSendMsg(*source, msgAck(session.id, msg->type));
	
	User *usr_ptr;
	for (session_usr_const_i ui(session.users.begin()); ui != session.users.end(); ++ui)
		if (ui->second != source)
		{
			usr_ptr = ui->second;
			uSendMsg(*usr_ptr, msg);
		}
	
	// send to users in waitingSync list, too
	if (toAll)
		for (userlist_const_i wui(session.waitingSync.begin()); wui != session.waitingSync.end(); ++wui)
			if ((*wui) != source)
			{
				usr_ptr = *wui;
				uSendMsg(*usr_ptr, msg);
			}
}

inline
void Server::uSendMsg(User& usr, message_ref msg) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uSendMsg(to user: " << static_cast<int>(usr.id)
		<< ", type: " << static_cast<int>(msg->type) << ")" << std::endl;
	protocol::msgName(msg->type);
	#endif
	
	switch (msg->type)
	{
	case protocol::type::Chat:
		if (!usr.ext_chat)
			return;
		break;
	case protocol::type::Palette:
		if (!usr.ext_palette)
			return;
		break;
	default:
		// do nothing
		break;
	}
	
	usr.queue.push_back( msg );
	
	if (!fIsSet(usr.events, ev.write))
	{
		fSet(usr.events, ev.write);
		ev.modify(usr.sock.fd(), usr.events);
	}
}

inline
void Server::SyncSession(Session* session) throw()
{
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::SyncSession(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	assert(session->syncCounter == 0);
	
	// TODO: Need better source user selection.
	const session_usr_const_i sui(session->users.begin());
	assert(sui != session->users.end());
	User* src(sui->second);
	
	// request raster
	message_ref ref(new protocol::Synchronize);
	ref->session_id = session->id;
	uSendMsg(*src, ref);
	
	// Release clients from syncwait...
	Propagate(*session, msgAck(session->id, protocol::type::SyncWait));
	
	std::vector<message_ref> msg_queue;
	msg_queue.reserve(4);
	
	if (session->locked)
		msg_queue.push_back(message_ref(new protocol::SessionEvent(protocol::session_event::Lock, protocol::null_user, 0)));
	
	// build msg_queue of the old users
	User *usr_ptr;
	protocol::ToolInfo *nfo;
	for (session_usr_const_i old(session->users.begin()); old != session->users.end(); ++old)
	{
		// clear syncwait 
		usr_ptr = old->second;
		const usr_session_const_i usi(usr_ptr->sessions.find(session->id));
		assert(usi != usr_ptr->sessions.end());
		usi->second->syncWait = false;
		
		// add join
		msg_queue.push_back(msgUserEvent(*usr_ptr, session->id, protocol::user_event::Join));
		if (usr_ptr->session->id == session->id)
		{
			// add session select
			ref.reset(new protocol::SessionSelect);
			ref->user_id = usr_ptr->id;
			ref->session_id = session->id;
			msg_queue.push_back(ref);
			
			if (usr_ptr->layer != protocol::null_layer)
			{
				// add layer select
				ref.reset(new protocol::LayerSelect(usr_ptr->layer));
				ref->user_id = usr_ptr->id;
				ref->session_id = session->id;
				msg_queue.push_back(ref);
			}
		}
		
		if (usi->second->cachedToolInfo != 0)
		{
			const protocol::ToolInfo *u_nfo = usi->second->cachedToolInfo;
			nfo = new protocol::ToolInfo(u_nfo->tool_id, u_nfo->mode, u_nfo->lo_size, u_nfo->hi_size, u_nfo->lo_hardness, u_nfo->hi_hardness, u_nfo->spacing);
			
			memcpy(nfo->lo_color, u_nfo->lo_color, 4);
			memcpy(nfo->hi_color, u_nfo->hi_color, 4);
			
			msg_queue.push_back(message_ref(nfo));
		}
	}
	
	userlist_const_i n_user;
	
	// announce the new users
	for (n_user = session->waitingSync.begin(); n_user != session->waitingSync.end(); ++n_user)
		msg_queue.push_back(msgUserEvent(**n_user, session->id, protocol::user_event::Join));
	
	std::vector<message_ref>::const_iterator m_iter;
	for (n_user = session->waitingSync.begin(); n_user != session->waitingSync.end(); ++n_user)
	{
		// Send messages
		for (m_iter=msg_queue.begin(); m_iter != msg_queue.end(); ++m_iter)
			uSendMsg(**n_user, *m_iter);
		
		// Create fake tunnel so the user can receive raster data
		tunnel.insert( std::make_pair(src, *n_user) );
		
		// add user to normal data propagation.
		session->users[(*n_user)->id] = *n_user;
	}
	
	session->waitingSync.clear();
}

inline
void Server::uJoinSession(User* usr, Session* session) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uJoinSession(session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	// Add session to users session list.
	SessionData *sdata = new SessionData(session);
	usr->sessions[session->id] = sdata;
	assert(usr->sessions[session->id]->session != 0);
	
	// Remove locked and mute, if the user is the session's owner.
	if (isOwner(*usr, *session))
	{
		sdata->locked = false;
		sdata->muted = false;
	}
	// Remove mute if the user is server admin.
	else if (usr->isAdmin)
		sdata->muted = false;
	
	if (session->users.size() != 0)
	{
		// Tell session members there's a new user.
		Propagate(*session, msgUserEvent(*usr, session->id, protocol::user_event::Join));
		
		// put user to wait sync list.
		//session->waitingSync.push_back(usr);
		session->waitingSync.insert(session->waitingSync.begin(), usr);
		usr->syncing = session->id;
		
		// don't start new client sync if one is already in progress...
		if (session->syncCounter == 0)
		{
			// Put session to syncing state
			session->syncCounter = session->users.size();
			
			// tell session users to enter syncwait state.
			Propagate(*session, msgSyncWait(session->id));
		}
	}
	else
	{
		// session is empty
		session->users[usr->id] = usr;
		
		message_ref raster_ref(new protocol::Raster(0, 0, 0, 0));
		raster_ref->session_id = session->id;
		
		uSendMsg(*usr, raster_ref);
	}
}

// Calls sRemove, uSendMsg, Propagate
inline
void Server::uLeaveSession(User& usr, Session*& session, const uint8_t reason) throw()
{
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uLeaveSession(user: "
		<< static_cast<int>(usr.id) << ", session: "
		<< static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	session->users.erase(usr.id);
	
	const uint8_t session_id = session->id;
	
	if (usr.session == session)
	{
		delete usr.cachedToolInfo;
		usr.cachedToolInfo = 0;
		usr.toolChanged = false;
		usr.session = 0;
	}
	
	// last user in session.. destruct it
	if (session->users.empty())
	{
		if (session->SelfDestruct)
			sRemove(session);
	}
	else
	{
		// Cancel raster sending for this user
		User* usr_ptr;
		tunnel_i t_iter(tunnel.begin());
		while (t_iter != tunnel.end())
		{
			if (t_iter->second == &usr)
			{
				usr_ptr = t_iter->first;
				message_ref cancel_ref(new protocol::Cancel);
				// TODO: Figure out which session it is from
				//cancel->session_id = session->id;
				uSendMsg(*usr_ptr, cancel_ref);
				
				tunnel.erase(t_iter);
				// iterator was invalidated
				t_iter = tunnel.begin();
			}
			else
				++t_iter;
		}
		
		// Tell session members the user left
		if (reason != protocol::user_event::None)
		{
			Propagate(*session, msgUserEvent(usr, session_id, reason));
			
			if (isOwner(usr, *session))
			{
				session->owner = protocol::null_user;
				
				// Announce owner disappearance.
				message_ref sev_ref(new protocol::SessionEvent(protocol::session_event::Delegate, session->owner, 0));
				sev_ref->session_id = session_id;
				Propagate(*session, sev_ref);
			}
		}
	}
	
	// remove user session instance
	delete usr.sessions.find(session_id)->second;
	usr.sessions.erase(session_id);
}

inline
void Server::uAdd(Socket sock) throw(std::bad_alloc)
{
	if (sock.fd() == INVALID_SOCKET)
	{
		#if defined(DEBUG_SERVER) and !defined(NDEBUG)
		std::cout << "Invalid socket, aborting user creation." << std::endl;
		#endif
		
		return;
	}
	
	// Check duplicate connections (should be enabled with command-line switch instead)
	if (blockDuplicateConnections)
		for (users_const_i ui(users.begin()); ui != users.end(); ++ui)
			if (sock.matchAddress(ui->second->sock))
			{
				std::cout << "Duplicate connection from: " << sock.address() << std::endl;
				return;
			}
	
	const uint8_t id = getUserID();
	
	if (id == protocol::null_user)
	{
		#ifndef NDEBUG
		std::cout << "Server full, disconnecting user: " << sock.address() << std::endl;
		#endif
		
		return;
	}
	
	std::cout << "User #" << static_cast<int>(id)
		<< " connected from " << sock.address() << std::endl;
	
	User* usr = new User(id, sock);
	
	fSet(usr->events, ev.read);
	ev.add(usr->sock.fd(), usr->events);
	
	const size_t ts = utimer.size();
	
	if (ts > 20)
		time_limit = srv_defaults::time_limit / 6; // half-a-minute
	else if (ts > 10)
		time_limit = srv_defaults::time_limit / 3; // 1 minute
	else
		time_limit = srv_defaults::time_limit; // 3 minutes
	
	usr->deadtime = time(0) + time_limit;
	
	// re-schedule user culling
	if (next_timer > usr->deadtime)
		next_timer = usr->deadtime + 1;
	
	// add user to timer
	utimer.insert(utimer.end(), usr);
	
	const size_t nwbuffer = 4096*2;
	
	// these two don't count towards bufferResets because the buffers are empty at this point
	usr->input.setBuffer(new char[nwbuffer], nwbuffer);
	#ifndef NDEBUG
	if (usr->input.size > largestInputBuffer)
		largestInputBuffer = usr->input.size; // stats
	#endif
	
	usr->output.setBuffer(new char[nwbuffer], nwbuffer);
	#ifndef NDEBUG
	if (usr->output.size > largestOutputBuffer)
		largestOutputBuffer = usr->output.size; // stats
	#endif
	
	users[sock.fd()] = usr;
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Known users: " << users.size() << std::endl;
	#endif
}

// Calls uSendMsg, uLeaveSession
inline
void Server::breakSync(User& usr) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::breakSync(user: "
		<< static_cast<int>(usr.id) << ")" << std::endl;
	#endif
	
	uSendMsg(usr, msgError(usr.syncing, protocol::error::SyncFailure));
	
	const sessions_i sui(sessions.find(usr.syncing));
	if (sui == sessions.end())
	{
		#if defined(DEBUG_SERVER) and !defined(NDEBUG)
		std::cerr << "Session to break sync with was not found!" << std::endl;
		#endif
	}
	else
	{
		uLeaveSession(usr, sui->second);
		usr.syncing = protocol::Global;
	}
}

inline
void Server::uRemove(User*& usr, const uint8_t reason) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::uRemove(user: " << static_cast<int>(usr->id)
		<< ", at address: " << usr->sock.address() << ")" << std::endl;
	#endif
	
	usr->state = uState::dead;
	
	// Remove from event system
	ev.remove(usr->sock.fd(), ev.read|ev.write|ev.error|ev.hangup);
	
	// Clear the fake tunnel of any possible instance of this user.
	// We're the source...
	tunnel_i ti;
	users_const_i usi;
	while ((ti = tunnel.find(usr)) != tunnel.end())
	{
		breakSync(*(ti->second));
		tunnel.erase(ti);
	}
	
	// break tunnels with the user in the receiving end
	ti = tunnel.begin();
	while (ti != tunnel.end())
	{
		if (ti->second == usr)
		{
			tunnel.erase(ti);
			//iterator was invalidated?
			ti = tunnel.begin();
		}
		else
			++ti;
	}
	
	// clean sessions
	Session *session;
	while (usr->sessions.size() != 0)
	{
		session = usr->sessions.begin()->second->session;
		uLeaveSession(*usr, session, reason);
	}
	
	// remove from fd -> User* map
	users.erase(usr->sock.fd());
	
	freeUserID(usr->id);
	
	// clear any idle timer associated with this user.
	utimer.erase(usr);
	
	delete usr;
	usr = 0;
	
	// Transient mode exit.
	if (Transient and users.empty())
		state = server::state::Exiting;
}

inline
void Server::sRemove(Session*& session) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::sRemove(session: " << static_cast<int>(session->id) << ")" << std::endl;
	#endif
	
	freeSessionID(session->id);
	sessions.erase(session->id);
	delete session;
	session = 0;
}

bool Server::init() throw(std::bad_alloc)
{
	assert(state == server::state::None);
	
	srand(time(0) - 513); // FIXME
	
	if (lsock.create() == INVALID_SOCKET)
	{
		std::cerr << "! Failed to create a socket." << std::endl;
		return false;
	}
	
	lsock.block(false); // nonblocking
	lsock.reuse(true); // reuse address
	
	bool bound = false;
	for (uint16_t bport=lo_port; bport != hi_port+1; ++bport)
	{
		#ifdef IPV6_SUPPORT
		if (lsock.bindTo("::", bport) == SOCKET_ERROR)
		#else
		if (lsock.bindTo("0.0.0.0", bport) == SOCKET_ERROR)
		#endif
		{
			const int bind_err = lsock.getError();
			if (bind_err == EBADF or bind_err == EINVAL)
				break;
		}
		else
		{
			bound = true;
			break;
		}
	}
	
	if (!bound)
		std::cerr << "Failed to bind to any port" << std::endl;
	else if (lsock.listen() == SOCKET_ERROR)
		std::cerr << "Failed to open listening port." << std::endl;
	else if (!ev.init())
		std::cerr << "Event system initialization failed." << std::endl;
	else
	{
		std::cout << "Listening on: " << lsock.address() << std::endl << std::endl;
		
		// add listening socket to event system
		ev.add(lsock.fd(), ev.read);
		
		// set event timeout
		ev.timeout(30000);
		
		state = server::state::Init;
		
		return true;
	}
	
	return false;
}

inline
bool Server::validateUserName(User* usr) const throw()
{
	assert(usr != 0);
	assert(fIsSet(requirements, protocol::requirements::EnforceUnique));
	
	if (usr->name_len == 0)
		return false;
	
	for (users_const_i ui(users.begin()); ui != users.end(); ++ui)
	{
		if (ui->second != usr // skip self
			and usr->name_len == ui->second->name_len // equal length
			and (memcmp(usr->name, ui->second->name, usr->name_len) == 0))
			return false;
	}
	
	return true;
}

inline
bool Server::validateSessionTitle(const char* name, const uint8_t len) const throw()
{
	assert(name != 0);
	
	#ifndef NDEBUG
	if (name == 0)
		assert(len == 0);
	#endif
	
	// Session title is never unique if it's an empty string.
	if (len == 0)
		return false;
	
	for (sessions_const_i si(sessions.begin()); si != sessions.end(); ++si)
		if (len == si->second->title_len and (memcmp(name, si->second->title, len) == 0))
			return false;
	
	return true;
}

inline
void Server::cullIdlers() throw()
{
	User *usr;
	for (userset_i tui(utimer.begin()); tui != utimer.end(); ++tui)
	{
		if ((*tui)->deadtime < current_time)
		{
			#ifndef NDEBUG
			std::cout << "User #" << static_cast<int>((*tui)->id)
				<< " kicked for idling in login." << std::endl;
			#endif
			
			usr = *tui;
			--tui;
			
			uRemove(usr, protocol::user_event::TimedOut);
		}
		else if ((*tui)->deadtime < next_timer)
		{
			// re-schedule next culling to come sooner
			next_timer = (*tui)->deadtime;
		}
	}
}

// main loop
int Server::run() throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	std::cout << "Server::run()" << std::endl;
	#endif
	
	assert(state == server::state::Init);
	state = server::state::Active;
	
	User *usr;
	
	// event count
	int ec;
	
	fd_t fd;
	uint32_t events;
	
	// main loop
	do
	{
		ec = ev.wait();
		current_time = time(0);
		switch (ec)
		{
		case 0:
			#ifndef NDEBUG
			std::cout << "+ Idling..." << std::endl;
			#endif
			break; // Nothing triggered
		case -1:
			std::cerr << "- Error in event system." << std::endl;
			state = server::state::Error;
			return -1;
		default:
			while (ev.getEvent(fd, events))
			{
				if (fd == lsock.fd())
				{
					uAdd( lsock.accept() );
					continue;
				}
				
				assert(users.find(fd) != users.end());
				
				usr = users.find(fd)->second;
				
				#ifdef EV_HAS_ERROR
				if (fIsSet(events, ev.error))
				{
					uRemove(usr, protocol::user_event::BrokenPipe);
					continue;
				}
				#endif // EV_HAS_ERROR
				#ifdef EV_HAS_HANGUP
				if (fIsSet(events, ev.hangup))
				{
					uRemove(usr, protocol::user_event::Disconnect);
					continue;
				}
				#endif // EV_HAS_HANGUP
				if (fIsSet(events, ev.read))
				{
					uRead(usr);
					if (usr == 0) continue;
				}
				if (fIsSet(events, ev.write))
				{
					uWrite(usr);
					if (usr == 0) continue;
				}
			}
			break;
		}
		
		// check timer
		if (next_timer < current_time)
		{
			if (!utimer.empty())
				cullIdlers();
			
			// reschedule to much later time if there's no users left
			if (utimer.empty())
				next_timer = current_time + 1800;
		}
	}
	while (state == server::state::Active);
	
	return 0;
}

#ifndef NDEBUG
void Server::stats() const throw()
{
	std::cout << "Statistics:" << std::endl
		<< std::endl
		<< "Under allocations:     " << protocolReallocation << std::endl
		<< "Largest linked list:   " << largestLinkList << std::endl
		<< "Largest output buffer: " << largestOutputBuffer << std::endl
		<< "Largest input buffer:  " << largestInputBuffer << std::endl
		<< "Buffer repositionings: " << bufferRepositions << std::endl
		<< "Buffer resets:         " << bufferResets << std::endl
		<< "Deflates discarded:    " << discardedCompressions << std::endl
		<< "Smallest deflate set:  " << smallestCompression << std::endl
		<< std::endl
		<< "Note: in all cases (except largest linked list), smaller value is better!" << std::endl;
}
#endif

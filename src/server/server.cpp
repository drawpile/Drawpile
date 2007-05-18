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
#include "common.h"

#include "../shared/protocol.defaults.h"
#include "../shared/protocol.helper.h"

#if defined(HAVE_ZLIB)
	#include <zlib.h>
#endif

#include <limits> // std::numeric_limits<T>
#include <iostream>

using std::cout;
using std::endl;
using std::cerr;

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

// the only iterator in which we need the ->first
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
	: state(Server::Dead),
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
	blockDuplicateConnections(true)
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

const uint8_t Server::getUserID() throw()
{
	if (user_ids.empty())
		return protocol::null_user;
	else
	{
		const uint8_t n = user_ids.front();
		user_ids.pop();
		
		return n;
	}
}

const uint8_t Server::getSessionID() throw()
{
	if (session_ids.empty())
		return protocol::Global;
	else
	{
		const uint8_t n = session_ids.front();
		session_ids.pop();
		
		return n;
	}
}

void Server::freeUserID(const uint8_t id) throw()
{
	assert(id != protocol::null_user);
	
	user_ids.push(id);
}

void Server::freeSessionID(const uint8_t id) throw()
{
	assert(id != protocol::Global);
	
	session_ids.push(id);
}

void Server::uRegenSeed(User& usr) const throw()
{
	usr.seed[0] = rand() % 256; // 0 - 255
	usr.seed[1] = rand() % 256;
	usr.seed[2] = rand() % 256;
	usr.seed[3] = rand() % 256;
}

message_ref Server::msgPWRequest(User& usr, const uint8_t session) const throw(std::bad_alloc)
{
	protocol::PasswordRequest* pwreq = new protocol::PasswordRequest;
	
	pwreq->session_id = session;
	
	uRegenSeed(usr);
	
	memcpy(pwreq->seed, usr.seed, protocol::password_seed_size);
	
	return message_ref(pwreq);
}

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

message_ref Server::msgError(const uint8_t session, const uint16_t code) const throw(std::bad_alloc)
{
	protocol::Error *err = new protocol::Error(code);
	err->session_id = session;
	return message_ref(err);
}

message_ref Server::msgAck(const uint8_t session, const uint8_t type) const throw(std::bad_alloc)
{
	protocol::Acknowledgement *ack = new protocol::Acknowledgement(type);
	ack->session_id = session;
	return message_ref(ack);
}

message_ref Server::msgSyncWait(const uint8_t session_id) const throw(std::bad_alloc)
{
	message_ref sync_ref(new protocol::SyncWait);
	sync_ref->session_id = session_id;
	return sync_ref;
}

// May delete User*
void Server::uWrite(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Sending data to user #" << usr->id << endl;
	#endif
	
	// if buffer is empty
	if (usr->output.isEmpty())
	{
		assert(!usr->queue.empty());
		
		const usr_message_i f_msg(usr->queue.begin());
		usr_message_i l_msg(f_msg+1), iter(f_msg);
		// create linked list
		size_t links=1;
		for (; l_msg != usr->queue.end(); ++l_msg, ++iter, ++links)
		{
			if (links == std::numeric_limits<uint8_t>::max()
				or ((*l_msg)->type != (*f_msg)->type)
				or ((*l_msg)->user_id != (*f_msg)->user_id)
				or ((*l_msg)->session_id != (*f_msg)->session_id)
			)
				break; // type changed or reached maximum size of linked list
			
			(*l_msg)->prev = boost::get_pointer(*iter);
			(*iter)->next = boost::get_pointer(*l_msg);
		}
		
		//msg->user_id = id;
		size_t len=0, size=usr->output.canWrite();
		
		// serialize message/s
		char* buf = (*--l_msg)->serialize(len, usr->output.wpos, size);
		
		// in case new buffer was allocated
		if (buf != usr->output.wpos)
			usr->output.setBuffer(buf, size, len);
		else
			usr->output.write(len);
		
		#if defined(HAVE_ZLIB)
		if (usr->ext_deflate
			and usr->output.canRead() > 300
			and (*f_msg)->type != protocol::Message::Raster)
		{
			// TODO: Move to separate function
			
			char* temp;
			ulong buffer_len = len + 12;
			// make the potential new buffer generous in its size
			
			bool inBuffer;
			
			if (usr->output.canWrite() < buffer_len)
			{ // can't write continuous stream of data in buffer
				assert(usr->output.free() < buffer_len);
				size = usr->output.size*2;
				temp = new char[size];
				inBuffer = false;
			}
			else
			{
				temp = usr->output.wpos;
				inBuffer = true;
			}
			
			const int r = compress2(reinterpret_cast<uchar*>(temp), &buffer_len, reinterpret_cast<uchar*>(usr->output.rpos), len, 5);
			
			assert(r != Z_STREAM_ERROR);
			assert(r != Z_BUF_ERROR); // too small buffer
			
			switch (r)
			{
			default:
			case Z_OK:
				#ifndef NDEBUG
				cout << "zlib: " << len << "B compressed down to " << buffer_len << "B" << endl;
				#endif
				
				// compressed size was equal or larger than original size
				if (buffer_len >= len)
					goto cleanup;
				
				usr->output.read(len);
				
				if (inBuffer)
				{
					usr->output.write(buffer_len);
					size = usr->output.canWrite();
				}
				else
					usr->output.setBuffer(temp, size, buffer_len);
				
				{
					const protocol::Deflate t_deflate(len, buffer_len, temp);
					
					// make sure we can write the whole message in
					if (usr->output.canWrite() < (buffer_len + 9) <= usr->output.free())
					{
						usr->output.reposition();
						size = usr->output.canWrite();
					}
					
					buf = t_deflate.serialize(len, usr->output.wpos, size);
					
					if (buf != usr->output.wpos)
					{
						#ifndef NDEBUG
						if (!inBuffer)
						{
							cout << "[Deflate] Pre-allocated buffer was too small!" << endl
								<< "Allocated: " << buffer_len*2+1024
								<< ", actually needed: " << size << endl
								<< "... for user #" << usr->id << endl;
						}
						else
						{
							cout << "[Deflate] Output buffer was too small!" << endl
								<< "Original size: " << usr->output.canWrite()
								<< ", actually needed: " << size << endl
								<< "... for user #" << usr->id << endl;
						}
						#endif
						usr->output.setBuffer(buf, size, len);
					}
					else
						usr->output.write(len);
				}
				break;
			case Z_MEM_ERROR:
				goto cleanup;
			}
			
			cleanup:
			if (!inBuffer)
				delete [] temp;
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
			cerr << "- Error occured while sending to user #" << usr->id << endl;
			
			uRemove(usr, protocol::UserInfo::BrokenPipe);
			break;
		}
		break;
	case 0:
		// no data was sent, and no error occured.
		break;
	default:
		#if defined(DEBUG_SERVER) and !defined(NDEBUG)
		cout << "? Sent " << sb << " bytes to user #" << usr->id << endl;
		#endif
		
		usr->output.read(sb);
		
		if (usr->output.isEmpty())
		{
			#if defined(DEBUG_SERVER) and !defined(NDEBUG)
			cout << "~ Output buffer empty, rewinding." << endl;
			#endif
			
			// rewind buffer
			usr->output.rewind();
			
			// remove fd from write list if no buffers left.
			if (usr->queue.empty())
			{
				#if defined(DEBUG_SERVER) and !defined(NDEBUG)
				cout << "~ Output queue empty, clearing event flag." << endl;
				#endif
				
				fClr(usr->events, ev.write);
				ev.modify(usr->sock.fd(), usr->events);
			}
			#if defined(DEBUG_SERVER) and !defined(NDEBUG)
			else
			{
				cout << "? Still have " << usr->queue.size() << " message/s in queue." << endl
					<< "... first is of type: " << static_cast<uint>(usr->queue.front()->type) << endl;
			}
			#endif
		}
		break;
	}
}

// May delete User*
void Server::uRead(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Reading data from user #" << usr->id << endl;
	#endif
	
	if (usr->input.free() == 0)
	{
		// buffer full
		#ifndef NDEBUG
		cerr << "~ Input buffer full, increasing size by 4 kiB." << endl;
		#endif
		
		usr->input.resize(usr->input.size + 4096);
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
		case EINTR: // retry later
			break;
		default:
			cerr << "- Read error from user #" << usr->id << endl;
			
			uRemove(usr, protocol::UserInfo::BrokenPipe);
			break;
		}
		break;
	case 0:
		// shouldn't happen if EV_HAS_HANGUP is defined
		cerr << "- Connection closed by user #" << usr->id << endl;
		uRemove(usr, protocol::UserInfo::Disconnect);
		break;
	default:
		usr->input.write(rb);
		uProcessData(usr);
		break;
	}
}

// calls uHandleMsg() and uHandleLogin()
void Server::uProcessData(User*& usr) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Processing data from user #" << usr->id << ", data left: "
		<< usr->input.left << " bytes" << endl;
	#endif
	
	while (!usr->input.isEmpty())
	{
		if (!usr->inMsg)
		{
			usr->inMsg = protocol::getMessage(usr->input.rpos[0]);
			if (!usr->inMsg)
			{
				// unknown message type
				cerr << "- Unknown data from user #" << usr->id << endl;
				uRemove(usr, protocol::UserInfo::Dropped);
				return;
			}
		}
		
		assert(usr->inMsg->type == static_cast<uint8_t>(usr->input.rpos[0]));
		
		size_t cread = usr->input.canRead();
		size_t reqlen = usr->inMsg->reqDataLen(usr->input.rpos, cread);
		if (reqlen > usr->input.left)
		{
			assert(!(usr->inMsg->type != protocol::Message::Raster and reqlen > 1024));
			return; // need more data
		}
		else if (reqlen > cread)
		{
			// Required length is greater than we can currently read,
			// but not greater than we have in total.
			// So, we reposition the buffer for maximal reading.
			usr->input.reposition();
		}
		else
		{
			#ifndef NDEBUG
			const size_t readlen = usr->inMsg->unserialize(usr->input.rpos, cread);
			assert(readlen == reqlen);
			usr->input.read( readlen );
			#else
			usr->input.read( usr->inMsg->unserialize(usr->input.rpos, cread) );
			#endif
			
			if (usr->state == User::Active)
				uHandleMsg(usr);
			else
				uHandleLogin(usr);
			
			if (usr) // still alive
			{
				delete usr->inMsg;
				usr->inMsg = 0;
			}
			else // quite dead
				break;
		}
	}
	
	if (usr) // rewind circular buffer
		usr->input.rewind();
}

message_ref Server::msgUserEvent(const User& usr, const uint8_t session_id, const uint8_t event) const throw(std::bad_alloc)
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Constructing user event for user #" << usr.id << endl;
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

void Server::uHandleDrawing(User& usr) throw()
{
	assert(usr.inMsg != 0);
	
	#ifndef NDEBUG
	switch (usr.inMsg->type)
	{
	case protocol::Message::StrokeInfo:
		++usr.strokes;
		break;
	case protocol::Message::StrokeEnd:
		usr.strokes = 0;
		break;
	}
	#endif
	
	// no session selected
	if (!usr.session)
	{
		#ifndef NDEBUG
		if (usr.strokes == 1)
			cerr << "? User #" << usr.id << " attempts to draw on null session." << endl;
		if (usr.strokes > 1000)
		{
			cerr << "User persist on drawing on null session, dropping." << endl;
			User* usr_ptr = &usr;
			uRemove(usr_ptr, protocol::UserInfo::Dropped);
		}
		#endif
		return;
	}
	
	// user or session is locked
	if (usr.session->locked or usr.a_locked)
	{
		// TODO: Warn user?
		#ifndef NDEBUG
		if (usr.strokes == 1)
		{
			cerr << "- User #" << usr.id << " tries to draw, despite the lock." << endl;
			
			if (usr.session->locked)
				cerr << "... Sesssion #" << usr.session->id << " is locked." << endl;
			else
				cerr << "... User is locked in session #" << usr.session->id << "." << endl;
		}
		#endif
	}
	else
	{
		#ifdef LAYER_SUPPORT
		if (usr.inMsg->type != protocol::Message::ToolInfo
			and usr.layer == protocol::null_layer)
		{
			#ifndef NDEBUG
			if (usr.strokes == 1)
				cerr << "User #" << usr.id << " is drawing on null layer!" << endl;
			if (usr.strokes > 1000)
			{
				cerr << "User persist on drawing on null layer, dropping." << endl;
				uRemove(&usr, protocol::UserInfo::Dropped);
			}
			#endif
			
			return;
		}
		#endif
		
		// make sure the user id is correct
		usr.inMsg->user_id = usr.id;
		
		Propagate(*usr.session, message_ref(usr.inMsg), (usr.c_acks ? &usr : 0));
		
		usr.inMsg = 0;
	}
}

// calls uQueueMsg, Propagate, uJoinSession
void Server::uHandlePassword(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	Session *session;
	protocol::Password &msg = *static_cast<protocol::Password*>(usr->inMsg);
	if (msg.session_id == protocol::Global)
	{
		// Admin login
		if (!a_password)
		{
			#ifndef NDEBUG
			cerr << "- User tries to pass password even though we've disallowed it." << endl;
			#endif
			uQueueMsg(*usr, msgError(msg.session_id, protocol::error::PasswordFailure));
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
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
			return;
		}
		
		sessions_const_i si(sessions.find(msg.session_id));
		if (si == sessions.end())
		{
			// session doesn't exist
			uQueueMsg(*usr, msgError(msg.session_id, protocol::error::UnknownSession));
			return;
		}
		else if (si->second->canJoin() == false)
		{
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
			return;
		}
		else if (usr->inSession(msg.session_id))
		{
			// already in session
			uQueueMsg(*usr, msgError(msg.session_id, protocol::error::InvalidRequest));
			return;
		}
		else
		{
			hash.Update(reinterpret_cast<uint8_t*>(si->second->password), si->second->pw_len);
			session = si->second;
		}
	}
	
	hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
	hash.Final();
	char digest[protocol::password_hash_size];
	hash.GetHash(reinterpret_cast<uint8_t*>(digest));
	hash.Reset();
	
	if (memcmp(digest, msg.data, protocol::password_hash_size) != 0) // mismatch
		uQueueMsg(*usr, msgError(msg.session_id, protocol::error::PasswordFailure));
	else
	{
		// ACK the password
		uQueueMsg(*usr, msgAck(msg.session_id, protocol::Message::Password));
		
		if (msg.session_id == protocol::Global)
			usr->isAdmin = true;
		else
			uJoinSession(usr, session);
	}
	
	// invalidate previous seed
	uRegenSeed(*usr);
}

// May delete User*
void Server::uHandleMsg(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Handling message from user #" << usr->id
		<< " of type #" << static_cast<uint>(usr->inMsg->type) << endl;
	#endif
	
	assert(usr->state == User::Active);
	
	switch (usr->inMsg->type)
	{
	case protocol::Message::ToolInfo:
		usr->inMsg->user_id = usr->id;
		usr->cacheTool(static_cast<protocol::ToolInfo*>(usr->inMsg));
	case protocol::Message::StrokeInfo:
	case protocol::Message::StrokeEnd:
		uHandleDrawing(*usr);
		break;
	case protocol::Message::Acknowledgement:
		uHandleAck(usr);
		break;
	case protocol::Message::SessionSelect:
		if (usr->makeActive(usr->inMsg->session_id))
		{
			usr->inMsg->user_id = usr->id;
			Propagate(*usr->session, message_ref(usr->inMsg), (usr->c_acks ? usr : 0));
			usr->inMsg = 0;
		}
		else
		{
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			usr->session = 0;
		}
		break;
	case protocol::Message::UserInfo:
		// TODO?
		break;
	case protocol::Message::Raster:
		uTunnelRaster(*usr);
		break;
	#if defined(HAVE_ZLIB)
	case protocol::Message::Deflate:
		DeflateReprocess(usr);
		break;
	#endif
	case protocol::Message::Chat:
		// TODO: Check session for deaf flag
	case protocol::Message::Palette:
		{
			const usr_session_const_i usi(usr->sessions.find(usr->inMsg->session_id));
			if (usi == usr->sessions.end())
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			else
			{
				message_ref pmsg(usr->inMsg);
				pmsg->user_id = usr->id;
				Propagate(*usi->second->session, pmsg, (usr->c_acks ? usr : 0));
				usr->inMsg = 0;
			}
		}
		break;
	case protocol::Message::Unsubscribe:
		{
			const usr_session_i usi(usr->sessions.find(usr->inMsg->session_id));
			if (usi == usr->sessions.end())
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			else
			{
				uQueueMsg(*usr, msgAck(usr->inMsg->session_id, protocol::Message::Unsubscribe));
				uLeaveSession(*usr, usi->second->session);
			}
		}
		break;
	case protocol::Message::Subscribe:
		if (usr->syncing == protocol::Global)
		{
			const sessions_const_i si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
			{
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				break;
			}
			
			Session *session = si->second;
			
			if (!usr->inSession(usr->inMsg->session_id))
			{
				// Test userlimit
				if (session->canJoin() == false)
					uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
				else if (session->level != usr->level)
					uQueueMsg(*usr, msgError(protocol::Global, protocol::error::ImplementationMismatch));
				else if (session->password != 0)
					uQueueMsg(*usr, msgPWRequest(*usr, session->id));
				else // join session
				{
					uQueueMsg(*usr, msgAck(usr->inMsg->session_id, protocol::Message::Subscribe));
					uJoinSession(usr, session);
				}
			}
			else // already subscribed
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::InvalidRequest));
		}
		else
		{
			// already syncing some session, so we don't bother handling this request.
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SyncInProgress));
		}
		break;
	case protocol::Message::ListSessions:
		if (sessions.size() != 0)
		{
			sessions_const_i si(sessions.begin());
			for (; si != sessions.end(); ++si)
				uQueueMsg(*usr, msgSessionInfo(*si->second));
		}
		
		uQueueMsg(*usr, msgAck(protocol::Global, protocol::Message::ListSessions));
		break;
	case protocol::Message::SessionEvent:
		{
			const sessions_const_i si(sessions.find(usr->inMsg->session_id));
			if (si == sessions.end())
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			else
			{
				usr->inMsg->user_id = usr->id;
				Session *session = si->second;
				uSessionEvent(session, usr);
			}
		}
		break;
	#ifdef LAYER_SUPPORT
	case protocol::Message::LayerEvent:
		uLayerEvent(usr);
		break;
	case protocol::Message::LayerSelect:
		{
			protocol::LayerSelect &layer = *static_cast<protocol::LayerSelect*>(usr->inMsg);
			
			if (layer->layer_id == usr->a_layer // trying to select current layer
				or (usr->a_layer_lock != protocol::null_layer
				and usr->a_layer_lock != layer->layer_id)) // locked to another
				uQueueMsg(*usr, msgError(layer->session_id, protocol::error::InvalidLayer));
			else
			{
				// find layer
				const session_layer_const_i li(usr->session->layers.find(layer->layer_id));
				if (li == usr->session->layers.end()) // target layer doesn't exist
					uQueueMsg(*usr, msgError(layer->session_id, protocol::error::UnknownLayer));
				else if (li->second.locked) // target layer is locked
					uQueueMsg(*usr, msgError(layer->session_id, protocol::error::LayerLocked));
				else
				{
					layer->user_id = usr->id; // set user id
					usr->a_layer = layer->layer_id; // set active layer
					
					// Tell other users about it
					Propagate(*usr->session, message_ref(layer), (usr->c_acks ? usr : 0));
				}
			}
		}
		break;
	#endif
	case protocol::Message::SessionInstruction:
		uSessionInstruction(usr);
		break;
	case protocol::Message::SetPassword:
		
	case protocol::Message::Shutdown:
		if (usr->isAdmin)
			state = Server::Exiting;
		break;
	case protocol::Message::Authenticate:
		if (!a_password) // no admin password set
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::Unauthorized));
		else // request password
			uQueueMsg(*usr, msgPWRequest(*usr, protocol::Global));
		break;
	case protocol::Message::Password:
		uHandlePassword(usr);
		break;
	default:
		cerr << "Unknown message: #" << static_cast<uint>(usr->inMsg->type)
			<< ", from user: #" << usr->id << " (dropping)" << endl;
		uRemove(usr, protocol::UserInfo::Dropped);
		break;
	}
}

// May delete User*
#if defined(HAVE_ZLIB)
void Server::DeflateReprocess(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if !defined(NDEBUG)
	cout << "[Server] Decompressing data from user #" << usr->id << endl;
	#endif
	
	protocol::Deflate &stream = *static_cast<protocol::Deflate*>(usr->inMsg);
	usr->inMsg = 0;
	
	#if !defined(NDEBUG)
	cout << "? Compressed: " << stream.length
		<< " bytes, uncompressed: " << stream.uncompressed << " bytes." << endl;
	#endif
	
	bool inBuffer;
	char *temp;
	ulong tmplen = stream.uncompressed;
	if (usr->input.canWrite() < stream.uncompressed)
	{
		temp = new char[stream.uncompressed];
		inBuffer = false;
	}
	else
	{
		temp = usr->input.wpos;
		inBuffer = true;
	}
	
	// temp is target buffer, tmplen is the number of bytes placed in that bufer
	const int r = uncompress(reinterpret_cast<uchar*>(temp), &tmplen, reinterpret_cast<uchar*>(stream.data), stream.length);
	
	switch (r)
	{
	default:
	case Z_OK:
		if (inBuffer)
		{
			usr->input.write(tmplen);
			uProcessData(usr);
		}
		else
		{
			// Store input buffer, it might not be empty
			Buffer tmpbuf;
			if (usr->input.left != 0)
				tmpbuf << usr->input;
			
			// Set the uncompressed data stream as the input buffer.
			usr->input.setBuffer(temp, stream.uncompressed, tmplen);
			
			// Process the data.
			uProcessData(usr);
			
			// uProcessData might've deleted the user, restore input buffer if not
			if (usr != 0 and (tmpbuf.left != 0 or tmpbuf.size > usr->input.size))
				usr->input << tmpbuf;
		}
		break;
	case Z_MEM_ERROR:
		// TODO: Out of Memory
		throw std::bad_alloc();
		//break;
	case Z_BUF_ERROR:
	case Z_DATA_ERROR:
		// Input buffer corrupted
		cerr << "- Corrupted data from user #" << usr->id << ", dropping." << endl;
		uRemove(usr, protocol::UserInfo::Dropped);
		if (!inBuffer)
			delete [] temp;
		break;
	}
}
#endif

// May delete User*
void Server::uHandleAck(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	const protocol::Acknowledgement &ack = *static_cast<protocol::Acknowledgement*>(usr->inMsg);
	
	switch (ack.event)
	{
	case protocol::Message::SyncWait:
		{
			// check active session first
			const usr_session_const_i usi(usr->sessions.find(ack.session_id));
			if (usi == usr->sessions.end())
				uQueueMsg(*usr, msgError(ack.session_id, protocol::error::NotSubscribed));
			else if (usi->second->syncWait) // duplicate syncwait
				uRemove(usr, protocol::UserInfo::Violation);
			else
			{
				usi->second->syncWait = true;
				
				Session *session = usi->second->session;
				--session->syncCounter;
				
				#ifndef NDEBUG
				cout << "? ACK/SyncWait counter: " << session->syncCounter << endl;
				#endif
				
				if (session->syncCounter == 0)
					SyncSession(session);
			}
		}
		break;
	default:
		#ifndef NDEBUG
		cout << "ACK/SomethingWeDoNotCareAboutButShouldValidateNoneTheLess" << endl;
		#endif
		// don't care..
		break;
	}
}

// Calls uQueueMsg()
void Server::uTunnelRaster(User& usr) throw()
{
	assert(usr.inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Serving raster data from user #" << usr.id << endl;
	#endif
	
	protocol::Raster *raster = static_cast<protocol::Raster*>(usr.inMsg);
	
	const bool last = (raster->offset + raster->length == raster->size);
	
	if (!usr.inSession(raster->session_id))
	{
		cerr << "- Raster for unsubscribed session #"
			<< static_cast<uint>(raster->session_id)
			<< ", from user #" << usr.id << endl;
		
		if (!last)
		{
			message_ref cancel_ref(new protocol::Cancel);
			cancel_ref->session_id = raster->session_id;
			uQueueMsg(usr, cancel_ref);
		}
		
		return;
	}
	
	// get users
	const std::pair<tunnel_i,tunnel_i> ft(tunnel.equal_range(&usr));
	if (ft.first == ft.second)
	{
		cerr << "Un-tunneled raster from user #" << usr.id
			<< ", for session #" << static_cast<uint>(raster->session_id)
			<< endl;
		
		// We assume the raster was for user/s who disappeared
		// before we could cancel the request.
		
		if (!last)
		{
			message_ref cancel_ref(new protocol::Cancel);
			cancel_ref->session_id = raster->session_id;
			uQueueMsg(usr, cancel_ref);
		}
	}
	else
	{
		// ACK raster
		uQueueMsg(usr, msgAck(usr.inMsg->session_id, protocol::Message::Raster));
		
		tunnel_i ti(ft.first);
		// Forward raster data to users.
		User *usr_ptr;
		
		message_ref raster_ref(raster);
		
		usr.inMsg = 0; // Avoid premature deletion of raster data.
		
		for (; ti != ft.second; ++ti)
		{
			usr_ptr = ti->second;
			uQueueMsg(*usr_ptr, raster_ref);
			if (last) usr_ptr->syncing = false;
		}
		
		// Break tunnel/s if that was the last raster piece.
		if (last) tunnel.erase(&usr);
	}
}

// Calls Propagate, uQueueMsg and uLeaveSession
// May delete User*
void Server::uSessionEvent(Session*& session, User*& usr) throw()
{
	assert(session != 0);
	assert(usr != 0);
	assert(usr->inMsg->type == protocol::Message::SessionEvent);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Handling event for session #" << session->id << endl;
	#endif
	
	if (!usr->isAdmin and !isOwner(*usr, *session))
	{
		uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
		return;
	}
	
	protocol::SessionEvent &event = *static_cast<protocol::SessionEvent*>(usr->inMsg);
	
	switch (event.action)
	{
	case protocol::SessionEvent::Kick:
		{
			const session_usr_const_i sui(session->users.find(event.target));
			if (sui == session->users.end()) // user not found in session
				uQueueMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
			else
			{
				Propagate(*session, message_ref(&event));
				usr->inMsg = 0;
				User *usr_ptr = sui->second;
				uLeaveSession(*usr_ptr, session, protocol::UserInfo::Kicked);
			}
		}
		break;
	case protocol::SessionEvent::Lock:
	case protocol::SessionEvent::Unlock:
		if (event.target == protocol::null_user)
		{
			// Lock whole board
			
			session->locked = (event.action == protocol::SessionEvent::Lock ? true : false);
			
			#ifndef NDEBUG
			cout << "~ Session #" << session->id
				<< " lock: " << (session->locked ? "Enabled" : "Disabled") << endl;
			#endif
			
		}
		else
		{
			// Lock single user
			
			#ifndef NDEBUG
			cout << "~ User #" << static_cast<uint>(event.target) << " lock: "
				<< (event.action == protocol::SessionEvent::Lock ? "Enabled" : "Disabled")
				<< ", in session #" << session->id << endl;
			#endif
			
			// Find user
			const session_usr_const_i session_usr(session->users.find(event.target));
			if (session_usr == session->users.end())
			{
				uQueueMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
				break;
			}
			User *usr = session_usr->second;
			
			// Find user's session instance (SessionData*)
			const usr_session_const_i usi(usr->sessions.find(session->id));
			if (usi == usr->sessions.end())
				uQueueMsg(*usr, msgError(session->id, protocol::error::NotInSession));
			else if (event.aux == protocol::null_layer)
			{
				// lock completely
				usi->second->locked = (event.action == protocol::SessionEvent::Lock);
				
				// Copy active session
				if (usr->session->id == event.session_id)
					usr->a_locked = usi->second->locked;
			}
			else if (event.action == protocol::SessionEvent::Lock)
			{
				#ifndef NDEBUG
				cout << "? Lock ignored, limiting user to layer #"
					<< static_cast<uint>(event.aux) << endl;
				#endif
				
				usi->second->layer_lock = event.aux;
				// copy to active session
				if (session->id == usr->session->id)
					usr->a_layer_lock = event.aux;
				
				// Null-ize the active layer if the target layer is not the active one.
				if (usi->second->layer != usi->second->layer_lock)
					usi->second->layer = protocol::null_layer;
				if (usr->session->id == event.session_id)
					usr->layer = protocol::null_layer;
			}
			else // unlock
			{
				#ifndef NDEBUG
				cout << "? Lock ignored, releasing user from layer #"
					<< static_cast<uint>(event.aux) << endl;
				#endif
				
				usi->second->layer_lock = protocol::null_layer;
				
				// copy to active session
				if (session->id == usr->session->id)
					usr->a_layer_lock = protocol::null_layer;
			}
		}
		
		Propagate(*session, message_ref(&event));
		usr->inMsg = 0;
		
		break;
	case protocol::SessionEvent::Delegate:
		{
			const session_usr_const_i sui(session->users.find(event.target));
			if (sui == session->users.end()) // User not found
				uQueueMsg(*usr, msgError(session->id, protocol::error::NotInSession));
			else
			{
				session->owner = sui->second->id;
				Propagate(*session, message_ref(&event), (usr->c_acks ? usr : 0));
				usr->inMsg = 0;
			}
		}
		break;
	case protocol::SessionEvent::Mute:
	case protocol::SessionEvent::Unmute:
		{
			users_i ui(users.find(event.target));
			if (ui == users.end())
				uQueueMsg(*usr, msgError(session->id, protocol::error::UnknownUser));
			else
			{
				usr_session_const_i usi(ui->second->sessions.find(session->id));
				if (usi == ui->second->sessions.end()) // user not in session
					uQueueMsg(*usr, msgError(session->id, protocol::error::NotInSession));
				else
				{
					// Set mode
					usi->second->muted = (event.action == protocol::SessionEvent::Mute);
					
					// Copy to active session's mode, too.
					if (usr->session->id == event.target)
						usr->session_data->muted = usi->second->muted;
					
					Propagate(*session, message_ref(&event), (usr->c_acks ? usr : 0));
					
					usr->inMsg = 0;
				}
			}
		}
		break;
	case protocol::SessionEvent::Persist:
		cerr << "- Setting 'Persist' for session not supported." << endl;
		// TODO
		break;
	case protocol::SessionEvent::CacheRaster:
		cerr << "- Setting 'Cache Raster' for session not supported." << endl;
		// TODO
		break;
	default:
		cerr << "- Unknown session action: "
			<< static_cast<uint>(event.action) <<  endl;
		uRemove(usr, protocol::UserInfo::Dropped);
		return;
	}
}

bool Server::isOwner(const User& usr, const Session& session) const throw()
{
	return session.owner == usr.id;
}

// Calls uQueueMsg, Propagate, sRemove, uLeaveSession
// May delete User*
void Server::uSessionInstruction(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::Message::SessionInstruction);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Handling session instruction from user #" << usr->id << endl;
	#endif
	
	protocol::SessionInstruction &msg = *static_cast<protocol::SessionInstruction*>(usr->inMsg);
	
	switch (msg.action)
	{
	case protocol::SessionInstruction::Create:
		if (!usr->isAdmin) { break; }
		// limited scope for switch/case
		{
			const uint8_t session_id = getSessionID();
			
			if (session_id == protocol::Global)
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::SessionLimit));
				break;
			}
			else if (msg.user_limit < 2)
			{
				#ifndef NDEBUG
				cerr << "- Attempted to create single user session." << endl;
				#endif
				
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::InvalidData));
				break;
			}
			/*
			else if (fIsSet(msg.user_mode, )) // admin flag
			{
				// TODO
				break;
			}
			*/
			
			if (msg.width < min_dimension or msg.height < min_dimension
				or msg.width > protocol::max_dimension or msg.height > protocol::max_dimension)
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::InvalidSize));
				break;
			}
			
			#ifndef NDEBUG
			if (!msg.title)
				cout << "- No title set for session." << endl;
			#endif
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateSessionTitle(msg.title, msg.title_len))
			{
				#ifndef NDEBUG
				cerr << "- Session title not unique." << endl;
				#endif
				
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::NotUnique));
			}
			else
			{
				Session *session = new Session(
					session_id, // identifier
					msg.user_mode, // mode
					msg.user_limit, // user limit
					usr->id, // owner
					msg.width, // width
					msg.height, // height
					usr->level, // inherit user's feature level
					msg.title_len,
					msg.title
				);
				
				sessions[session->id] = session;
				
				cout << "+ Session #" << session->id << " created by user #" << usr->id
					<< " [" << usr->sock.address() << "]" << endl
					<< "  Size: " << session->width << "x" << session->height
					<< ", Limit: " << static_cast<uint>(session->limit)
					<< ", Mode: " << static_cast<uint>(session->mode)
					<< ", Level: " << static_cast<uint>(session->level) << endl;
				
				msg.title = 0; // prevent title from being deleted
				
				uQueueMsg(*usr, msgAck(msg.session_id, protocol::Message::SessionInstruction));
			}
		}
		return; // because session owner might've done this
	case protocol::SessionInstruction::Destroy:
		{
			const sessions_const_i si(sessions.find(msg.session_id));
			if (si == sessions.end())
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::UnknownSession));
				return;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
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
				uLeaveSession(*usr_ptr, session, protocol::UserInfo::None);
			}
			
			// destruct
			sRemove(session);
		}
		break;
	case protocol::SessionInstruction::Alter:
		{
			const sessions_const_i si(sessions.find(msg.session_id));
			if (si == sessions.end())
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::UnknownSession));
				break;
			}
			Session *session = si->second;
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			// user limit adjustment
			session->limit = msg.user_limit;
			
			// default user mode adjustment
			session->mode = msg.user_mode;
			
			if ((msg.width < session->width) or (msg.height < session->height))
			{
				cerr << "- Protocol violation from user #" << usr->id << endl
					<< "  Reason: Attempted to reduce session's canvas size." << endl;
				uRemove(usr, protocol::UserInfo::Violation);
				break;
			}
			else if (msg.width > protocol::max_dimension or msg.height > protocol::max_dimension)
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::InvalidSize));
				break;
			}
			
			session->width = msg.width;
			session->height = msg.height;
			
			// new title
			delete [] session->title;
			if (msg.title_len != 0)
			{
				session->title = msg.title;
				session->title_len = msg.title_len;
			}
			else
				session->title_len = 0;
			
			#ifndef NDEBUG
			cout << "+ Session #" << session->id << " altered by user #" << usr->id << endl
				<< "  Size: " << session->width << "x" << session->height
				<< ", Limit: " << static_cast<uint>(session->limit)
				<< ", Mode: " << static_cast<uint>(session->mode) << endl;
			#endif // NDEBUG
			
			Propagate(*session, msgSessionInfo(*session));
		}
		break;
	default:
		#ifndef NDEBUG
		cerr << "- Unrecognized action: " << static_cast<uint>(msg.action) << endl;
		#endif
		uRemove(usr, protocol::UserInfo::Dropped);
		return;
	}
}

void Server::uSetPassword(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::Message::SetPassword);
	
	protocol::SetPassword &setpw = *static_cast<protocol::SetPassword*>(usr->inMsg);
	if (setpw.session_id == protocol::Global)
	{
		if (usr->isAdmin)
		{
			setPassword(setpw.password, setpw.password_len);
			setpw.password = 0;
			
			#ifndef NDEBUG
			cout << "& Server password changed." << endl;
			#endif
			
			uQueueMsg(*usr, msgAck(protocol::Global, protocol::Message::SetPassword));
		}
		else
			uQueueMsg(*usr, msgError(protocol::Global, protocol::error::Unauthorized));
	}
	else
	{
		const sessions_const_i si(sessions.find(setpw.session_id));
		if (si == sessions.end())
		{
			uQueueMsg(*usr, msgError(setpw.session_id, protocol::error::UnknownSession));
			return;
		}
		Session *session = si->second;
		
		// Check session ownership
		if (!usr->isAdmin and !isOwner(*usr, *session))
		{
			uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
			return;
		}
		
		#ifndef NDEBUG
		cout << "& Password set for session #"
			<< static_cast<uint>(setpw.session_id) << endl;
		#endif
		
		if (session->password != 0)
			delete [] session->password;
		
		session->password = setpw.password;
		session->pw_len = setpw.password_len;
		setpw.password = 0;
		
		uQueueMsg(*usr, msgAck(setpw.session_id, protocol::Message::SetPassword));
	}
}

void Server::uHandleLogin(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Handling login message for user #" << usr->id
		<< " of type: " << static_cast<uint>(usr->inMsg->type) << endl;
	#endif
	
	switch (usr->state)
	{
	case User::Login:
		if (usr->inMsg->type == protocol::Message::UserInfo)
		{
			protocol::UserInfo &msg = *static_cast<protocol::UserInfo*>(usr->inMsg);
			
			if (msg.length > name_len_limit)
			{
				#ifndef NDEBUG
				cerr << "- Name too long: " << static_cast<uint>(msg.length)
					<< " > " << static_cast<uint>(name_len_limit) << endl;
				#endif
				
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::TooLong));
				
				//uRemove(usr, protocol::UserInfo::Dropped);
				break;
			}
			
			// assign user their own name
			usr->name = msg.name;
			usr->name_len = msg.length;
			
			if (fIsSet(requirements, protocol::requirements::EnforceUnique)
				and !validateUserName(usr))
			{
				#ifndef NDEBUG
				cerr << "- User name not unique." << endl;
				#endif
				
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::NotUnique));
				
				//uRemove(usr, protocol::UserInfo::Dropped);
				break;
			}
			
			// assign message the user's id (for sending back)
			msg.user_id = usr->id;
			
			// null the message's name information, so they don't get deleted
			msg.length = 0;
			msg.name = 0;
			
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
			
			msg.mode = usr->getAMode();
			
			usr->state = User::Active;
			usr->deadtime = 0;
			usr->inMsg = 0;
			
			// remove fake timer
			utimer.erase(utimer.find(usr));
			
			// reply
			uQueueMsg(*usr, message_ref(&msg));
		}
		else // wrong message type
			uRemove(usr, protocol::UserInfo::Dropped);
		break;
	case User::LoginAuth:
		if (usr->inMsg->type == protocol::Message::Password)
		{
			const protocol::Password &msg = *static_cast<protocol::Password*>(usr->inMsg);
			
			hash.Update(reinterpret_cast<uint8_t*>(password), pw_len);
			hash.Update(reinterpret_cast<uint8_t*>(usr->seed), 4);
			hash.Final();
			char digest[protocol::password_hash_size];
			hash.GetHash(reinterpret_cast<uint8_t*>(digest));
			hash.Reset();
			
			if (memcmp(digest, msg.data, protocol::password_hash_size) == 0)
			{
				uQueueMsg(*usr, msgAck(msg.session_id, protocol::Message::Password)); // ACK
				uRegenSeed(*usr); // mangle seed
				usr->state = User::Login; // set state
				uQueueMsg(*usr, msgHostInfo()); // send hostinfo
			}
			else  // mismatch
				uRemove(usr, protocol::UserInfo::Dropped);
		}
		else // not a password
			uRemove(usr, protocol::UserInfo::Violation);
		break;
	case User::Init:
		if (usr->inMsg->type == protocol::Message::Identifier)
		{
			const protocol::Identifier &ident = *static_cast<protocol::Identifier*>(usr->inMsg);
			
			if (memcmp(ident.identifier, protocol::identifier_string, protocol::identifier_size) != 0)
			{
				#ifndef NDEBUG
				cerr << "- Protocol string mismatch" << endl;
				#endif
				
				uRemove(usr, protocol::UserInfo::Dropped);
			}
			else if (ident.revision != protocol::revision)
			{
				#ifndef NDEBUG
				cerr << "- Protocol revision mismatch; "
					<< "expected " << protocol::revision
					<< " instead of " << ident.revision << endl;
				#endif
				
				// TODO: Implement some compatible way of announcing incompatibility?
				
				uRemove(usr, protocol::UserInfo::Dropped);
			}
			else
			{
				if (!password) // no password set
				{
					usr->state = User::Login;
					uQueueMsg(*usr, msgHostInfo());
				}
				else
				{
					usr->state = User::LoginAuth;
					uQueueMsg(*usr, msgPWRequest(*usr, protocol::Global));
				}
				
				usr->level = ident.level; // feature level used by client
				usr->setCapabilities(ident.flags);
				usr->setExtensions(ident.extensions);
				usr->setAMode(default_user_mode);
				//if (!usr->ext_chat)
				//	usr->a_deaf = true;
			}
		}
		else
		{
			cerr << "- Invalid data from user #" << usr->id << endl;
			uRemove(usr, protocol::UserInfo::Dropped);
		}
		break;
	default:
		assert(!"user state was something strange");
		break;
	}
}

void Server::uLayerEvent(User*& usr) throw()
{
	assert(usr != 0);
	assert(usr->inMsg != 0);
	assert(usr->inMsg->type == protocol::Message::LayerEvent);
	
	#ifndef NDEBUG
	cout << "[Server] Handling layer event for session #" << usr->inMsg->session_id << endl;
	#endif
	
	protocol::LayerEvent &levent = *static_cast<protocol::LayerEvent*>(usr->inMsg);
	
	sessions_i si(sessions.find(levent.session_id));
	if (si == sessions.end()) // session not found
	{
		uQueueMsg(*usr, msgError(levent.session_id, protocol::error::UnknownSession));
		return;
	}
	
	Session *session = si->second;
	if (!isOwner(*usr, *session) and !usr->isAdmin)
	{
		uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
		return;
	}
	
	switch (levent.action)
	{
	case protocol::LayerEvent::Create:
		{
			// TODO: Figure out what to do with the layer ID
			uint8_t lastLayer = levent.layer_id;
			session->layers[lastLayer] = LayerData(lastLayer, levent.mode, levent.opacity);
		}
		break;
	case protocol::LayerEvent::Destroy:
		session->layers.erase(levent.layer_id);
		// TODO: Cleanup
		break;
	case protocol::LayerEvent::Alter:
		{
			session_layer_i sli = session->layers.find(levent.layer_id);
			if (sli == session->layers.end())
			{
				uQueueMsg(*usr, msgError(session->id, protocol::error::UnknownLayer));
				return;
			}
			sli->second = LayerData(levent.layer_id, levent.mode, levent.opacity);
		}
		break;
	default:
		cerr << "- Unknown layer event: " << static_cast<uint>(levent.action) << std::endl;
		uRemove(usr, protocol::UserInfo::Dropped);
		return;
	}
	
	levent.user_id = usr->id;
	
	Propagate(*session, message_ref(&levent), (usr->c_acks ? usr : 0));
	usr->inMsg = 0;
}

// Calls uQueueMsg
void Server::Propagate(const Session& session, message_ref msg, User* source, const bool toAll) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Propagating message to session #" << session.id
		<< " of type #" << static_cast<uint>(msg->type) << endl;
	#endif
	
	//the assert fails with messages that are session selected
	//assert(session.id == msg->session_id);
	
	// Send ACK for the message we're about to share..
	if (source != 0)
		uQueueMsg(*source, msgAck(session.id, msg->type));
	
	User *usr_ptr;
	for (session_usr_const_i ui(session.users.begin()); ui != session.users.end(); ++ui)
		if (ui->second != source)
		{
			usr_ptr = ui->second;
			uQueueMsg(*usr_ptr, msg);
		}
	
	// send to users in waitingSync list, too
	if (toAll)
		for (userlist_const_i wui(session.waitingSync.begin()); wui != session.waitingSync.end(); ++wui)
			if ((*wui) != source)
			{
				usr_ptr = *wui;
				uQueueMsg(*usr_ptr, msg);
			}
}

void Server::uQueueMsg(User& usr, message_ref msg) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Queuing message to user #" << usr.id
		<< " of type #" << static_cast<uint>(msg->type) << endl;
	protocol::msgName(msg->type);
	#endif
	
	switch (msg->type)
	{
	case protocol::Message::Chat:
		if (!usr.ext_chat)
			return;
		break;
	case protocol::Message::Palette:
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

void Server::SyncSession(Session* session) throw()
{
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Syncing clients for session #" << session->id << endl;
	#endif
	
	assert(session->syncCounter == 0);
	
	// TODO: Need better source user selection.
	const session_usr_const_i sui(session->users.begin());
	assert(sui != session->users.end());
	User* src(sui->second);
	
	// request raster
	message_ref ref(new protocol::Synchronize);
	ref->session_id = session->id;
	uQueueMsg(*src, ref);
	
	// Release clients from syncwait...
	Propagate(*session, msgAck(session->id, protocol::Message::SyncWait));
	
	#ifdef HAVE_SLIST
	__gnu_cxx::slist<message_ref> msg_queue;
	#else
	std::list<message_ref> msg_queue;
	#endif
	
	if (session->locked)
		msg_queue.insert(msg_queue.end(), message_ref(new protocol::SessionEvent(protocol::SessionEvent::Lock, protocol::null_user, 0)));
	
	// build msg_queue of the old users
	User *usr_ptr;
	for (session_usr_const_i old(session->users.begin()); old != session->users.end(); ++old)
	{
		// clear syncwait 
		usr_ptr = old->second;
		const usr_session_const_i usi(usr_ptr->sessions.find(session->id));
		assert(usi != usr_ptr->sessions.end());
		usi->second->syncWait = false;
		
		// add join
		msg_queue.insert(msg_queue.end(), msgUserEvent(*usr_ptr, session->id, protocol::UserInfo::Join));
		if (usr_ptr->session->id == session->id)
		{
			// add session select
			ref.reset(new protocol::SessionSelect);
			ref->user_id = usr_ptr->id;
			ref->session_id = session->id;
			msg_queue.insert(msg_queue.end(), ref);
			
			if (usr_ptr->layer != protocol::null_layer)
			{
				// add layer select
				ref.reset(new protocol::LayerSelect(usr_ptr->layer));
				ref->user_id = usr_ptr->id;
				ref->session_id = session->id;
				msg_queue.insert(msg_queue.end(), ref);
			}
		}
		
		if (usr_ptr->session == session)
		{
			if (usr_ptr->session_data->cachedToolInfo)
				msg_queue.insert(msg_queue.end(), message_ref(new protocol::ToolInfo(*usr_ptr->session_data->cachedToolInfo)));
		}
		else
		{
			if (usi->second->cachedToolInfo)
				msg_queue.insert(msg_queue.end(), message_ref(new protocol::ToolInfo(*usi->second->cachedToolInfo)));
		}
	}
	
	userlist_const_i n_user;
	
	// announce the new users
	for (n_user = session->waitingSync.begin(); n_user != session->waitingSync.end(); ++n_user)
		msg_queue.insert(msg_queue.end(), msgUserEvent(**n_user, session->id, protocol::UserInfo::Join));
	
	#ifdef HAVE_SLIST
	__gnu_cxx::slist<message_ref>::const_iterator m_iter;
	#else
	std::list<message_ref>::const_iterator m_iter;
	#endif
	for (n_user = session->waitingSync.begin(); n_user != session->waitingSync.end(); ++n_user)
	{
		// Send messages
		for (m_iter=msg_queue.begin(); m_iter != msg_queue.end(); ++m_iter)
			uQueueMsg(**n_user, *m_iter);
		
		// Create fake tunnel so the user can receive raster data
		tunnel.insert( std::make_pair(src, *n_user) );
		
		// add user to normal data propagation.
		session->users[(*n_user)->id] = *n_user;
	}
	
	session->waitingSync.clear();
}

void Server::uJoinSession(User* usr, Session* session) throw()
{
	assert(usr != 0);
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Attaching user #" << usr->id << " to session #" << session->id << endl;
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
		Propagate(*session, msgUserEvent(*usr, session->id, protocol::UserInfo::Join));
		
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
		
		uQueueMsg(*usr, raster_ref);
	}
}

// Calls sRemove, uQueueMsg, Propagate
void Server::uLeaveSession(User& usr, Session*& session, const protocol::UserInfo::uevent reason) throw()
{
	assert(session != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Detaching user #" << usr.id << " from session #" << session->id << endl;
	#endif
	
	session->users.erase(usr.id);
	
	const uint8_t session_id = session->id;
	
	if (usr.session == session)
		usr.session = 0;
	
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
				uQueueMsg(*usr_ptr, cancel_ref);
				
				tunnel.erase(t_iter);
				// iterator was invalidated
				t_iter = tunnel.begin();
			}
			else
				++t_iter;
		}
		
		// Tell session members the user left
		if (reason != protocol::UserInfo::None)
		{
			Propagate(*session, msgUserEvent(usr, session_id, reason));
			
			if (isOwner(usr, *session))
			{
				session->owner = protocol::null_user;
				
				// Announce owner disappearance.
				message_ref sev_ref(new protocol::SessionEvent(protocol::SessionEvent::Delegate, session->owner, 0));
				sev_ref->session_id = session_id;
				Propagate(*session, sev_ref);
			}
		}
	}
	
	// remove user session instance
	delete usr.sessions.find(session_id)->second;
	usr.sessions.erase(session_id);
}

void Server::uAdd(Socket sock) throw(std::bad_alloc)
{
	if (sock.fd() == INVALID_SOCKET)
	{
		#if defined(DEBUG_SERVER) and !defined(NDEBUG)
		cout << "- Invalid socket, aborting user creation." << endl;
		#endif
		
		return;
	}
	
	// Check duplicate connections (should be enabled with command-line switch instead)
	if (blockDuplicateConnections)
		for (users_const_i ui(users.begin()); ui != users.end(); ++ui)
			if (sock.matchAddress(ui->second->sock))
			{
				cerr << "- Duplicate connection from " << sock.address() << endl;
				return;
			}
	
	const uint id = getUserID();
	
	if (id == protocol::null_user)
	{
		#ifndef NDEBUG
		cerr << "- Server full, dropping connection from " << sock.address() << endl;
		#endif
		
		return;
	}
	
	cout << "+ New user #" << id << " [" << sock.address() << "]" << endl;
	
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
	
	usr->deadtime = current_time + time_limit;
	
	// re-schedule user culling
	if (next_timer > usr->deadtime)
		next_timer = usr->deadtime + 1;
	
	// add user to timer
	utimer.insert(utimer.end(), usr);
	
	const size_t nwbuffer = 4096*2;
	
	// these two don't count towards bufferResets because the buffers are empty at this point
	usr->input.setBuffer(new char[nwbuffer], nwbuffer);
	usr->output.setBuffer(new char[nwbuffer], nwbuffer);
	
	users[sock.fd()] = usr;
	
	sock.release(); // invalidate local copy so it won't be closed prematurely
}

// Calls uQueueMsg, uLeaveSession
void Server::breakSync(User& usr) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Breaking session sync with user #" << usr.id << endl;
	#endif
	
	uQueueMsg(usr, msgError(usr.syncing, protocol::error::SyncFailure));
	
	const sessions_i sui(sessions.find(usr.syncing));
	assert(sui != sessions.end());
	
	uLeaveSession(usr, sui->second);
	usr.syncing = protocol::Global;
}

void Server::uRemove(User*& usr, const protocol::UserInfo::uevent reason) throw()
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Removing user #" << usr->id << " [" << usr->sock.address() << "]" << endl;
	#endif
	
	usr->sock.shutdown(SHUT_RDWR);
	
	// Remove socket from event system
	ev.remove(usr->sock.fd());
	
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
	
	// limit transient mode's exit to valid users only
	bool tryExit = (usr->state == User::Active);
	
	delete usr;
	usr = 0;
	
	// Transient mode exit.
	if (Transient and tryExit and users.empty())
		state = Server::Exiting;
}

void Server::sRemove(Session*& session) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Removing session #" << session->id << endl;
	#endif
	
	freeSessionID(session->id);
	sessions.erase(session->id);
	delete session;
	session = 0;
}

bool Server::init() throw(std::bad_alloc)
{
	assert(state == Server::Dead);
	
	srand(time(0) - 513); // FIXME
	
	if (lsock.create() == INVALID_SOCKET)
	{
		cerr << "- Failed to create a socket." << endl;
		return false;
	}
	
	lsock.block(false); // nonblocking
	lsock.reuse(true); // reuse address
	
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
			goto resume;
	}
	cerr << "- Failed to bind to any port" << endl;
	return false;
	
	resume:
	if (lsock.listen() == SOCKET_ERROR)
		cerr << "- Failed to open listening port." << endl;
	else if (!ev.init())
		cerr << "- Event system initialization failed." << endl;
	else
	{
		cout << "+ Listening on port " << lsock.port() << endl << endl;
		
		// add listening socket to event system
		#ifdef EV_HAS_ACCEPT
		ev.add(lsock.fd(), ev.accept);
		#else
		ev.add(lsock.fd(), ev.read);
		#endif
		
		// set event timeout
		ev.timeout(30000);
		
		state = Server::Init;
		
		return true;
	}
	
	return false;
}

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

bool Server::validateSessionTitle(const char* name, const uint8_t len) const throw()
{
	assert(name != 0 and len > 0);
	
	// Session title is never unique if it's an empty string.
	if (len == 0)
		return false;
	
	for (sessions_const_i si(sessions.begin()); si != sessions.end(); ++si)
		if (len == si->second->title_len and (memcmp(name, si->second->title, len) == 0))
			return false;
	
	return true;
}

void Server::cullIdlers() throw()
{
	User *usr;
	for (userset_i tui(utimer.begin()); tui != utimer.end(); ++tui)
	{
		if ((*tui)->deadtime < current_time)
		{
			#ifndef NDEBUG
			cout << "- Dropping inactive user #" << (*tui)->id << endl;
			#endif
			
			usr = *tui;
			--tui;
			
			uRemove(usr, protocol::UserInfo::TimedOut);
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
	cout << "[Server] Entering main loop" << endl;
	#endif
	
	assert(state == Server::Init);
	state = Server::Active;
	
	User *usr;
	
	fd_t fd;
	Event::ev_t events;
	
	users_i ui;
	
	// main loop
	do
	{
		switch (ev.wait())
		{
		case -1:
			cerr << "- Error in event system: " << ev.getError() << endl;
			state = Server::Error;
			return -1;
		case 0:
			// do nothing
			break;
		default:
			current_time = time(0);
			while (ev.getEvent(fd, events))
			{
				#ifdef EV_HAS_ACCEPT
				if (fIsSet(events, ev.accept))
				#else
				if (fd == lsock.fd())
				#endif
				{
					uAdd( lsock.accept() );
					continue;
				}
				
				ui = users.find(fd);
				assert(ui != users.end());
				usr = ui->second;
				
				#ifdef EV_HAS_ERROR
				if (fIsSet(events, ev.error))
				{
					cerr << "- Broken pipe for user #" << usr->id << endl;
					uRemove(usr, protocol::UserInfo::BrokenPipe);
					continue;
				}
				#endif // EV_HAS_ERROR
				#ifdef EV_HAS_HANGUP
				if (fIsSet(events, ev.hangup))
				{
					cerr << "- Connection closed by user #" << usr->id << endl;
					uRemove(usr, protocol::UserInfo::Disconnect);
					continue;
				}
				#endif // EV_HAS_HANGUP
				if (fIsSet(events, ev.read))
				{
					uRead(usr);
					if (!usr) continue;
				}
				if (fIsSet(events, ev.write))
				{
					uWrite(usr);
					if (!usr) continue;
				}
			}
			
			// check timer
			if (next_timer < current_time)
			{
				if (utimer.empty())
					next_timer = current_time + 1800;
				else
					cullIdlers();
			}
			break;
		}
	}
	while (state == Server::Active);
	
	return 0;
}

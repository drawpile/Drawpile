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

#ifdef LINUX
	#include <cstdio>
#endif
#include <limits> // std::numeric_limits<T>
#include <iostream>

using std::cout;
using std::endl;
using std::cerr;

// iterators
typedef std::map<uint8_t, Session*>::iterator sessions_i;
typedef std::map<uint8_t, Session*>::const_iterator sessions_const_i;
typedef std::map<fd_t, User*>::iterator users_i;
typedef std::map<fd_t, User*>::const_iterator users_const_i;

// the only iterator in which we need the ->first
typedef std::multimap<User*, User*>::iterator tunnel_i;
typedef std::multimap<User*, User*>::const_iterator tunnel_const_i;

#include <list>
typedef std::list<User*>::iterator userlist_i;
typedef std::list<User*>::const_iterator userlist_const_i;

typedef std::set<User*>::iterator userset_i;
typedef std::set<User*>::const_iterator userset_const_i;

Server::Server() throw()
	: state(Server::Dead),
	user_limit(0),
	session_limit(1),
	max_subscriptions(1),
	name_len_limit(12),
	time_limit(srv_defaults::time_limit),
	current_time(0), next_timer(0),
	hi_port(protocol::default_port), lo_port(protocol::default_port),
	min_dimension(400),
	requirements(0),
	extensions(
		protocol::extensions::Chat|protocol::extensions::Palette
		#ifdef HAVE_ZLIB
		|protocol::extensions::Deflate
		#endif // HAVE_ZLIB
	),
	enforceUnique(false), wideStrings(false), noGlobalChat(false),
	extDeflate(
		#ifdef HAVE_ZLIB
		true
		#else
		false
		#endif
		),
	extPalette(true), extChat(true),
	default_user_mode(protocol::user_mode::None),
	Transient(false), LocalhostAdmin(false),
	blockDuplicateConnections(true)
{
	memset(user_ids, true, sizeof(user_ids));
	memset(session_ids, true, sizeof(session_ids));
	
	#ifndef NDEBUG
	cout << "? Event mechanism: " << event::system<EventSystem>::value << endl;
	#endif
}

#if 0
Server::~Server() throw()
{
	reset();
}
#endif

const uint8_t Server::getUserID() throw()
{
	static int index=0;
	
	for (int count=255; !user_ids[index]; --count)
	{
		if (count == 0) return protocol::null_user;
		if (++index == 255) index = 0;
	}
	
	const int ri = index+1;
	user_ids[index++] = false;
	if (index == 255) index = 0;
	return ri;
}

const uint8_t Server::getSessionID() throw()
{
	static int index=0;
	
	for (int count=255; !session_ids[index]; ++index, --count)
	{
		if (count == 0) return protocol::null_user;
		if (index == 255) index = -1;
	}
	
	const int ri = index+1;
	session_ids[index++] = false;
	if (index == 255) index = 0;
	return ri;
}

void Server::freeUserID(const uint8_t id) throw()
{
	assert(id != protocol::null_user);
	assert(user_ids[id-1] == false);
	user_ids[id-1] = true;
}

void Server::freeSessionID(const uint8_t id) throw()
{
	assert(id != protocol::Global);
	assert(session_ids[id-1] == false);
	session_ids[id-1] = true;
}

void Server::uRegenSeed(User& usr) const throw()
{
	#ifdef LINUX
	FILE stream = fopen("/dev/urandom", "r");
	assert(stream);
	if (!stream) throw std::exception;
	fread(usr.seed, 4, 1, stream);
	fclose(stream);
	#else
	usr.seed[0] = rand() % 256; // 0 - 255
	usr.seed[1] = rand() % 256;
	usr.seed[2] = rand() % 256;
	usr.seed[3] = rand() % 256;
	#endif
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
		session.title.size,
		new char[session.title.size]
	);
	
	nfo->session_id = session.id;
	
	memcpy(nfo->title, session.title.ptr, session.title.size);
	
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
		usr->flushQueue();
		
		#if defined(HAVE_ZLIB)
		if (usr->ext_deflate
			and usr->output.rpos[0] != protocol::Message::Raster
			and usr->output.canRead() > 300)
		{
			Deflate(usr->output, usr->output.canRead());
		}
		#endif // HAVE_ZLIB
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
				
				fClr(usr->events, event::write<EventSystem>::value);
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

void Server::Deflate(Buffer& buffer, size_t len) throw(std::bad_alloc)
{
	// len, size
	size_t size=buffer.size;
	
	char* temp;
	ulong buffer_len = len + 12;
	// make the potential new buffer generous in its size
	
	bool inBuffer;
	
	if (buffer.canWrite() < buffer_len)
	{ // can't write continuous stream of data in buffer
		assert(buffer.free() < buffer_len);
		size = buffer.size*2;
		temp = new char[size];
		inBuffer = false;
	}
	else
	{
		temp = buffer.wpos;
		inBuffer = true;
	}
	
	const int r = compress2(reinterpret_cast<uchar*>(temp), &buffer_len, reinterpret_cast<uchar*>(buffer.rpos), len, 5);
	
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
		
		buffer.read(len);
		
		if (inBuffer)
		{
			buffer.write(buffer_len);
			size = buffer.canWrite();
		}
		else
			buffer.setBuffer(temp, size, buffer_len);
		
		{
			const protocol::Deflate t_deflate(len, buffer_len, temp);
			
			// make sure we can write the whole message in
			if (buffer.canWrite() < (buffer_len + 9) <= buffer.free())
			{
				buffer.reposition();
				size = buffer.canWrite();
			}
			
			char *buf = t_deflate.serialize(len, buffer.wpos, size);
			
			if (buf != buffer.wpos)
			{
				#ifndef NDEBUG
				if (!inBuffer)
				{
					cout << "[Deflate] Pre-allocated buffer was too small!" << endl
						<< "Allocated: " << buffer_len*2+1024
						<< ", actually needed: " << size << endl;
				}
				else
				{
					cout << "[Deflate] Output buffer was too small!" << endl
						<< "Original size: " << buffer.canWrite()
						<< ", actually needed: " << size << endl;
				}
				#endif
				buffer.setBuffer(buf, size, len);
			}
			else
				buffer.write(len);
		}
		break;
	case Z_MEM_ERROR:
		goto cleanup;
	}
	
	cleanup:
	if (!inBuffer)
		delete [] temp;
}

// May delete User*
void Server::uRead(User*& usr) throw(std::bad_alloc)
{
	assert(usr != 0);
	
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Reading data from user #" << usr->id << endl;
	#endif
	
	if (usr->input.free() == 0) // buffer full
	{
		#ifndef NDEBUG
		cerr << "~ Input buffer full, increasing size by 4 kiB." << endl;
		#endif
		
		usr->input.resize(usr->input.size + 4096);
	}
	
	const int rb = usr->sock.recv( usr->input.wpos, usr->input.canWrite() );
	
	switch (rb)
	{
	case SOCKET_ERROR:
		switch (usr->sock.getError())
		{
		case EAGAIN:
		case EINTR: // retry later
			break;
		default:
			uRemove(usr, protocol::UserInfo::BrokenPipe);
			break;
		}
		break;
	case 0:
		// shouldn't happen if EV_HAS_HANGUP is defined
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
				cerr << "- Invalid data from user #" << usr->id << endl;
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
	
	const SessionData *sdata = usr.getConstSession(session_id);
	assert(sdata != 0);
	
	protocol::UserInfo *uevent = new protocol::UserInfo(
		sdata->getMode(),
		event,
		usr.name.size,
		(usr.name.size == 0 ? 0 : new char[usr.name.size])
	);
	
	uevent->user_id = usr.id;
	uevent->session_id = session_id;
	
	memcpy(uevent->name, usr.name.ptr, usr.name.size);
	
	return message_ref(uevent);
}

void Server::uHandleDrawing(User& usr) throw()
{
	assert(usr.inMsg != 0);
	
	// no session selected
	if (!usr.session)
	{
		#ifndef NDEBUG
		if (usr.strokes == 1)
			cerr << "? User #" << usr.id << " attempts to draw on null session." << endl;
		if (usr.strokes > 1000)
		{
			#ifndef NDEBUG
			cerr << "- User persist on drawing on null session, dropping." << endl;
			#endif
			User* usr_ptr = &usr;
			uRemove(usr_ptr, protocol::UserInfo::Dropped);
		}
		#endif
		return;
	}
	
	// user or session is locked
	if (usr.session->locked or usr.session_data->locked)
	{
		// TODO: Warn user?
		#ifndef NDEBUG
		if (usr.strokes == 1)
		{
			cerr << "- User #" << usr.id << " tries to draw, despite the lock." << endl;
			
			if (usr.session->locked)
				cerr << "  Clarification: Sesssion #" << usr.session->id << " is locked" << endl;
			else
				cerr << "  Clarification: User is locked in session #" << usr.session->id << endl;
		}
		#endif
	}
	else
	{
		#ifdef PERSISTENT_SESSIONS
		if (usr.session->persist and usr.session->raster_invalid == false)
		{
			#ifndef NDEBUG
			cout << "? Session raster invalidated." << endl;
			#endif
			// invalidate raster
			usr.session->raster_invalid = true;
			usr.session->raster_cached = false;
			delete usr.session->raster;
			usr.session->raster = 0;
		}
		#endif // PERSISTENT_SESSIONS
		
		#ifdef LAYER_SUPPORT
		if (usr.inMsg->type == protocol::Message::StrokeInfo
			and usr.layer == protocol::null_layer)
		{
			#ifndef NDEBUG
			if (usr.strokes == 1)
				cerr << "- User #" << usr.id << " is drawing on null layer!" << endl;
			if (usr.strokes > 1000)
			{
				cerr << "- User persists on drawing on null layer, dropping." << endl;
				uRemove(&usr, protocol::UserInfo::Dropped);
			}
			#endif
			
			return;
		}
		#endif
		
		// make sure the user id is correct
		Propagate(*usr.session, message_ref(usr.inMsg), (usr.c_acks ? &usr : 0));
		usr.inMsg = 0;
	}
}

// calls uQueueMsg, Propagate, uJoinSession
void Server::uHandlePassword(User& usr) throw()
{
	assert(usr.inMsg != 0);
	
	char *str=0;
	size_t len=0;
	
	Session *session=0;
	protocol::Password &msg = *static_cast<protocol::Password*>(usr.inMsg);
	if (msg.session_id == protocol::Global)
	{
		// Admin login
		if (admin_password.ptr == 0)
			uQueueMsg(usr, msgError(msg.session_id, protocol::error::PasswordFailure));
		else
		{
			str = admin_password.ptr;
			len = admin_password.size;
			goto dohashing;
		}
		return;
	}
	else
	{
		if (usr.syncing != protocol::Global) // already syncing
			uQueueMsg(usr, msgError(usr.inMsg->session_id, protocol::error::SyncInProgress));
		else
		{
			session = getSession(msg.session_id);
			if (session == 0) // session doesn't exist
				uQueueMsg(usr, msgError(msg.session_id, protocol::error::UnknownSession));
			else if (session->canJoin() == false)
				uQueueMsg(usr, msgError(usr.inMsg->session_id, protocol::error::SessionFull));
			else if (usr.getConstSession(msg.session_id) != 0) // already in session
				uQueueMsg(usr, msgError(msg.session_id, protocol::error::InvalidRequest));
			else
			{
				str = session->password.ptr;
				len = session->password.size;
				goto dohashing;
			}
		}
		return;
	}
	
	dohashing:
	if (!CheckPassword(msg.data, str, len, usr.seed)) // mismatch
		uQueueMsg(usr, msgError(msg.session_id, protocol::error::PasswordFailure));
	else
	{
		// ACK the password
		uQueueMsg(usr, msgAck(msg.session_id, protocol::Message::Password));
		
		if (msg.session_id == protocol::Global)
			usr.isAdmin = true;
		else
			uJoinSession(usr, *session);
	}
	
	// invalidate previous seed
	uRegenSeed(usr);
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
	
	usr->inMsg->user_id = usr->id;
	
	switch (usr->inMsg->type)
	{
	case protocol::Message::ToolInfo:
		usr->cacheTool(static_cast<protocol::ToolInfo*>(usr->inMsg));
		uHandleDrawing(*usr);
		break;
	case protocol::Message::StrokeInfo:
		++usr->strokes;
		if (usr->session_data->cachedToolInfo)
			uHandleDrawing(*usr);
		else
			uRemove(usr, protocol::UserInfo::Violation);
		break;
	case protocol::Message::StrokeEnd:
		usr->strokes = 0;
		uHandleDrawing(*usr);
		break;
	case protocol::Message::Acknowledgement:
		uHandleAck(usr);
		break;
	case protocol::Message::SessionSelect:
		if (usr->makeActive(usr->inMsg->session_id))
		{
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
		if (wideStrings and ((static_cast<protocol::Chat*>(usr->inMsg)->length % 2) != 0))
		{
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::InvalidData));
			break;
		}
		// TODO: Check session for deaf flag
	case protocol::Message::Palette:
		{
			const SessionData *sdata = usr->getConstSession(usr->inMsg->session_id);
			if (sdata == 0)
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			else
			{
				message_ref pmsg(usr->inMsg);
				Propagate(*sdata->session, pmsg, (usr->c_acks ? usr : 0));
				usr->inMsg = 0;
			}
		}
		break;
	case protocol::Message::Unsubscribe:
		{
			SessionData *sdata = usr->getSession(usr->inMsg->session_id);
			if (sdata == 0)
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::NotSubscribed));
			else
			{
				Session *session = sdata->session;
				#ifdef PERSISTENT_SESSIONS
				if (session->persist and !session->raster_cached and session->users.size() == 1)
				{
					uQueueMsg(*usr, msgError(session->id, protocol::error::RequestIgnored));
					message_ref ref(new protocol::Synchronize);
					ref->session_id = session->id;
					uQueueMsg(*usr, ref);
				}
				else
				#endif // PERSISTENT_SESSIONS
				{
					uQueueMsg(*usr, msgAck(usr->inMsg->session_id, protocol::Message::Unsubscribe));
					uLeaveSession(*usr, session);
				}
			}
		}
		break;
	case protocol::Message::Subscribe:
		if (usr->syncing == protocol::Global)
		{
			Session *session = getSession(usr->inMsg->session_id);
			if (session == 0)
			{
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
				break;
			}
			
			if (usr->getConstSession(usr->inMsg->session_id) == 0)
			{
				// Test userlimit
				if (session->canJoin() == false)
					uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::SessionFull));
				else if (session->level != usr->level)
					uQueueMsg(*usr, msgError(protocol::Global, protocol::error::ImplementationMismatch));
				else if (session->password.ptr != 0)
					uQueueMsg(*usr, msgPWRequest(*usr, session->id));
				else // join session
				{
					uQueueMsg(*usr, msgAck(usr->inMsg->session_id, protocol::Message::Subscribe));
					uJoinSession(*usr, *session);
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
			Session *session = getSession(usr->inMsg->session_id);
			if (session == 0)
				uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::UnknownSession));
			else
				uSessionEvent(session, usr);
		}
		break;
	#ifdef LAYER_SUPPORT
	case protocol::Message::LayerEvent:
		uLayerEvent(usr);
		break;
	case protocol::Message::LayerSelect:
		{
			protocol::LayerSelect &layer = *static_cast<protocol::LayerSelect*>(usr->inMsg);
			
			if (layer->layer_id == usr->session_data->layer // trying to select current layer
				or (usr->session_data->layer_lock != protocol::null_layer
				and usr->session_data->layer_lock != layer->layer_id)) // locked to another
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
					usr->session_data->layer = layer->layer_id; // set active layer
					
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
		uSetPassword(usr);
		break;
	case protocol::Message::Shutdown:
		if (usr->isAdmin)
			state = Server::Exiting;
		break;
	case protocol::Message::Authenticate:
		if (admin_password.ptr == 0) // no admin password set
			uQueueMsg(*usr, msgError(usr->inMsg->session_id, protocol::error::Unauthorized));
		else // request password
			uQueueMsg(*usr, msgPWRequest(*usr, protocol::Global));
		break;
	case protocol::Message::Password:
		uHandlePassword(*usr);
		break;
	default:
		cerr << "- Invalid data from user: #" << usr->id << " (dropping)" << endl;
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
		cerr << "- Invalid data from user #" << usr->id << ", dropping." << endl;
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
			SessionData *sdata = usr->getSession(ack.session_id);
			if (sdata == 0)
				uQueueMsg(*usr, msgError(ack.session_id, protocol::error::NotSubscribed));
			else if (sdata->syncWait) // duplicate syncwait
				uRemove(usr, protocol::UserInfo::Violation);
			else
			{
				sdata->syncWait = true;
				
				Session *session = sdata->session;
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
	
	SessionData *sdata = usr.getSession(raster->session_id);
	if (sdata == 0)
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
		#ifdef PERSISTENT_SESSIONS
		if (sdata->session->persist and !sdata->session->raster_cached)
		{
			#ifndef NDEBUG
			cout << "? Caching raster." << endl;
			#endif
			
			protocol::Raster *& sras = sdata->session->raster;
			if (sras == 0)
			{
				sdata->session->raster_invalid = false;
				sras = new protocol::Raster(0, raster->length, raster->size, 0);
				if (sras->length < sras->size)
				{
					sras->data = new char[sras->size];
					memcpy(sras->data, raster->data, raster->length);
				}
				else
				{
					sras->data = raster->data;
					raster->data = 0;
				}
			}
			else
			{
				assert(data->session->raster_invalid == false);
				assert(raster->size == sras->size);
				assert(raster->offset + raster->length <= sras->size);
				
				memcpy(sras->data+raster->offset, raster->data, raster->length);
				sras->length += raster->length;
			}
			
			if (sras->length == sras->size)
			{
				sdata->session->raster_cached = true;
				
				// Detach user from session now
				uQueueMsg(usr, msgAck(sdata->session_id, protocol::Message::Unsubscribe));
				uLeaveSession(usr, sdata->id);
			}
		}
		else
		#endif // PERSISTENT_SESSIONS
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
	}
	else
	{
		// ACK raster
		uQueueMsg(usr, msgAck(usr.inMsg->session_id, protocol::Message::Raster));
		
		tunnel_i ti(ft.first);
		// Forward raster data to users.
		
		message_ref raster_ref(raster);
		
		usr.inMsg = 0; // Avoid premature deletion of raster data.
		
		for (; ti != ft.second; ++ti)
		{
			uQueueMsg(*ti->second, raster_ref);
			if (last) ti->second->syncing = false;
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
				User &usr_ref = *sui->second;
				uLeaveSession(usr_ref, session, protocol::UserInfo::Kicked);
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
			User &usr_ref = *session_usr->second;
			
			// Find user's session instance (SessionData*)
			SessionData *sdata = usr_ref.getSession(session->id);
			if (sdata == 0)
				uQueueMsg(*usr, msgError(session->id, protocol::error::NotInSession));
			else if (event.aux == protocol::null_layer)
			{
				// lock completely
				sdata->locked = (event.action == protocol::SessionEvent::Lock);
				
				// Copy active session
				if (usr_ref.session->id == event.session_id)
					usr_ref.session_data->locked = sdata->locked;
			}
			else if (event.action == protocol::SessionEvent::Lock)
			{
				#ifndef NDEBUG
				cout << "? Lock ignored, limiting user to layer #"
					<< static_cast<uint>(event.aux) << endl;
				#endif
				
				sdata->layer_lock = event.aux;
				// copy to active session
				if (session->id == usr_ref.session->id)
					usr_ref.session_data->layer_lock = event.aux;
				
				// Null-ize the active layer if the target layer is not the active one.
				if (sdata->layer != sdata->layer_lock)
					sdata->layer = protocol::null_layer;
				if (usr_ref.session->id == event.session_id)
					usr_ref.layer = protocol::null_layer;
			}
			else // unlock
			{
				#ifndef NDEBUG
				cout << "? Lock ignored, releasing user from layer #"
					<< static_cast<uint>(event.aux) << endl;
				#endif
				
				sdata->layer_lock = protocol::null_layer;
				
				// copy to active session
				if (session->id == usr_ref.session->id)
					usr_ref.session_data->layer_lock = protocol::null_layer;
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
				SessionData *sdata = ui->second->getSession(session->id);
				if (sdata == 0) // user not in session
					uQueueMsg(*usr, msgError(session->id, protocol::error::NotInSession));
				else
				{
					// Set mode
					sdata->muted = (event.action == protocol::SessionEvent::Mute);
					
					// Copy to active session's mode, too.
					if (usr->session->id == event.target)
						usr->session_data->muted = sdata->muted;
					
					Propagate(*session, message_ref(&event), (usr->c_acks ? usr : 0));
					usr->inMsg = 0;
				}
			}
		}
		break;
	case protocol::SessionEvent::Persist:
		//#ifdef PERSISTENT_SESSIONS
		#ifndef NDEBUG
		cout << "+ Session #" << session->id << " persists." << endl;
		#endif
		session->persist = (event.aux != 0);
		// TODO: Send ACK
		//#else
		// TODO: Send RequestIgnored error
		//#endif
		break;
	default:
		cerr << "- Invalid data from user #" << usr->id <<  endl;
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
		if (!usr->isAdmin)
		{
			#ifndef NDEBUG
			cerr << "- Non-admin tries to create session; user #" << usr->id << endl;
			#endif
			break;
		}
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
			
			if (wideStrings and ((msg.title_len % 2) != 0))
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::InvalidData));
				break;
			}
			
			Array<char> title(msg.title, msg.title_len);
			if (fIsSet(requirements, static_cast<uint8_t>(protocol::requirements::EnforceUnique))
				and !validateSessionTitle(title))
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
					title
				);
				
				sessions[session->id] = session;
				
				#ifndef NDEBUG
				cout << "+ Session #" << session->id << " created by user #" << usr->id
					/*<< " [" << usr->sock.address() << "]"*/ << endl
					<< "  Size: " << session->width << "x" << session->height
					<< ", Limit: " << static_cast<uint>(session->limit)
					<< ", Mode: " << static_cast<uint>(session->mode)
					<< ", Level: " << static_cast<uint>(session->level) << endl
					;
				#endif
				
				msg.title = 0; // prevent title from being deleted
				
				uQueueMsg(*usr, msgAck(msg.session_id, protocol::Message::SessionInstruction));
			}
			title.ptr = 0;
		}
		return; // because session owner might've done this
	case protocol::SessionInstruction::Destroy:
		{
			Session *session = getSession(msg.session_id);
			if (session == 0)
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::UnknownSession));
				return;
			}
			
			// Check session ownership
			if (!usr->isAdmin and !isOwner(*usr, *session))
			{
				uQueueMsg(*usr, msgError(session->id, protocol::error::Unauthorized));
				break;
			}
			
			// tell users session was lost
			message_ref err = msgError(session->id, protocol::error::SessionLost);
			Propagate(*session, err, 0);
			for (userlist_i wui(session->waitingSync.begin()); wui != session->waitingSync.end(); ++wui)
			{
				uQueueMsg(**wui, err);
			}
			
			// clean session users
			session_usr_const_i sui(session->users.begin());
			User *usr_ptr;
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
			Session *session = getSession(msg.session_id);
			if (session == 0)
			{
				uQueueMsg(*usr, msgError(msg.session_id, protocol::error::UnknownSession));
				break;
			}
			
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
				cerr << "- Invalid data from user #" << usr->id << endl;
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
			if (msg.title_len != 0)
			{
				session->title.set(msg.title, msg.title_len);
				msg.title = 0;
			}
			else
				session->title.set(0,0);
			
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
		cerr << "- Invalid data from user #" << usr->id << endl;
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
		Session *session = getSession(setpw.session_id);
		if (session == 0)
		{
			uQueueMsg(*usr, msgError(setpw.session_id, protocol::error::UnknownSession));
			return;
		}
		
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
		
		session->password.set(setpw.password, setpw.password_len);
		setpw.password = 0;
		
		uQueueMsg(*usr, msgAck(setpw.session_id, protocol::Message::SetPassword));
	}
}

// Calls validateUserName, uQueueMsg
void Server::uLoginInfo(User& usr) throw()
{
	protocol::UserInfo &msg = *static_cast<protocol::UserInfo*>(usr.inMsg);
	
	if (msg.length > name_len_limit)
	{
		#ifndef NDEBUG
		cerr << "- Name too long: " << static_cast<uint>(msg.length)
			<< " > " << static_cast<uint>(name_len_limit) << endl;
		#endif
		
		uQueueMsg(usr, msgError(msg.session_id, protocol::error::TooLong));
		return;
	}
	
	if (wideStrings and ((msg.length % 2) != 0))
	{
		uQueueMsg(usr, msgError(msg.session_id, protocol::error::InvalidData));
		return;
	}
	
	// assign user their own name
	usr.name.set(msg.name, msg.length);
	
	if (fIsSet(requirements, static_cast<uint8_t>(protocol::requirements::EnforceUnique))
		and !validateUserName(&usr))
	{
		#ifndef NDEBUG
		cerr << "- Name not unique" << endl;
		#endif
		
		uQueueMsg(usr, msgError(msg.session_id, protocol::error::NotUnique));
		return;
	}
	
	// null the message's name information, so they don't get deleted
	msg.length = 0;
	msg.name = 0;
	
	if (LocalhostAdmin)
	{
		const std::string IPPort(usr.sock.address());
		const std::string::size_type ns(IPPort.find_last_of(":", IPPort.length()-1));
		assert(ns != std::string::npos);
		const std::string IP(IPPort.substr(0, ns));
		
		// Loopback address
		if (IP == std::string(
			#ifdef IPV6_SUPPORT
			Network::IPv6::Localhost
			#else
			Network::IPv4::Localhost
			#endif
		))
		{
			usr.isAdmin = true;
		}
	}
	
	msg.user_id = usr.id;
	msg.mode = 0;
	
	usr.state = User::Active;
	usr.deadtime = 0;
	usr.inMsg = 0;
	
	// remove fake timer
	utimer.erase(utimer.find(&usr));
	
	// reply
	uQueueMsg(usr, message_ref(&msg));
}

bool Server::CheckPassword(const char *hashdigest, const char *str, const size_t len, const char *seed) throw()
{
	assert(hashdigest != 0);
	assert(str != 0);
	assert(len > 0);
	assert(seed != 0);
	
	SHA1 hash;
	
	hash.Update(reinterpret_cast<const uchar*>(str), len);
	hash.Update(reinterpret_cast<const uchar*>(seed), 4);
	hash.Final();
	char digest[protocol::password_hash_size];
	hash.GetHash(reinterpret_cast<uchar*>(digest));
	
	return (memcmp(hashdigest, digest, protocol::password_hash_size) == 0);
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
			uLoginInfo(*usr);
		else // wrong message type
			uRemove(usr, protocol::UserInfo::Dropped);
		break;
	case User::LoginAuth:
		if (usr->inMsg->type == protocol::Message::Password)
		{
			const protocol::Password &msg = *static_cast<protocol::Password*>(usr->inMsg);
			
			if (CheckPassword(msg.data, password.ptr, password.size, usr->seed))
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
				if (password.ptr == 0) // no password set
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
			}
		}
		else
		{
			cerr << "- Invalid data from user #" << usr->id << endl;
			uRemove(usr, protocol::UserInfo::Dropped);
		}
		break;
	default:
		assert(0);
		uRemove(usr, protocol::UserInfo::None);
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
	
	Session *session = getSession(levent.session_id);
	if (session == 0) // not found
	{
		uQueueMsg(*usr, msgError(levent.session_id, protocol::error::UnknownSession));
		return;
	}
	
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
		cerr << "- Invalid data from user #" << usr->id << std::endl;
		uRemove(usr, protocol::UserInfo::Dropped);
		return;
	}
	
	Propagate(*session, message_ref(&levent), (usr->c_acks ? usr : 0));
	usr->inMsg = 0;
}

// Calls uQueueMsg
void Server::Propagate(const Session& session, message_ref msg, User* source) throw()
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
	
	if (!fIsSet(usr.events, event::write<EventSystem>::value))
	{
		fSet(usr.events, event::write<EventSystem>::value);
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
	
	// in case the users had been dropped
	if (session->waitingSync.size() == 0)
		return;
	
	std::list<message_ref> msg_queue;
	
	if (session->locked)
		msg_queue.insert(msg_queue.end(), message_ref(new protocol::SessionEvent(protocol::SessionEvent::Lock, protocol::null_user, 0)));
	
	// build msg_queue of the old users
	User *usr_ptr;
	for (session_usr_const_i old(session->users.begin()); old != session->users.end(); ++old)
	{
		// clear syncwait 
		usr_ptr = old->second;
		SessionData *sdata = usr_ptr->getSession(session->id);
		assert(sdata != 0);
		sdata->syncWait = false;
		
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
			if (sdata->cachedToolInfo)
				msg_queue.insert(msg_queue.end(), message_ref(new protocol::ToolInfo(*sdata->cachedToolInfo)));
		}
	}
	
	userlist_const_i n_user;
	
	// announce the new users
	for (n_user = session->waitingSync.begin(); n_user != session->waitingSync.end(); ++n_user)
		msg_queue.insert(msg_queue.end(), msgUserEvent(**n_user, session->id, protocol::UserInfo::Join));
	
	std::list<message_ref>::const_iterator m_iter;
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

void Server::uJoinSession(User& usr, Session& session) throw()
{
	#if defined(DEBUG_SERVER) and !defined(NDEBUG)
	cout << "[Server] Attaching user #" << usr.id << " to session #" << session.id << endl;
	#endif
	
	// Add session to users session list.
	SessionData *sdata = new SessionData(session);
	usr.sessions[session.id] = sdata;
	assert(usr.sessions[session.id]->session != 0);
	
	// Remove locked and mute, if the user is the session's owner.
	if (isOwner(usr, session))
	{
		sdata->locked = false;
		sdata->muted = false;
	}
	// Remove mute if the user is server admin.
	else if (usr.isAdmin)
		sdata->muted = false;
	
	if (session.users.size() != 0)
	{
		// Tell session members there's a new user.
		Propagate(session, msgUserEvent(usr, session.id, protocol::UserInfo::Join));
		
		// put user to wait sync list.
		//session->waitingSync.push_back(usr);
		session.waitingSync.insert(session.waitingSync.begin(), &usr);
		usr.syncing = session.id;
		
		// don't start new client sync if one is already in progress...
		if (session.syncCounter == 0)
		{
			// Put session to syncing state
			session.syncCounter = session.users.size();
			
			// tell session users to enter syncwait state.
			Propagate(session, msgSyncWait(session.id));
		}
	}
	else
	{
		// session is empty
		session.users[usr.id] = &usr;
		
		protocol::Raster *raster = new protocol::Raster(0, 0, 0, 0);
		raster->session_id = session.id;
		
		uQueueMsg(usr, message_ref(raster));
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
		if (!session->persist)
			sRemove(session);
	}
	else
	{
		// Cancel raster sending for this user
		tunnel_i t_iter(tunnel.begin());
		while (t_iter != tunnel.end())
		{
			if (t_iter->second == &usr)
			{
				message_ref cancel_ref(new protocol::Cancel);
				// TODO: Figure out which session it is from
				//cancel->session_id = session->id;
				uQueueMsg(*t_iter->first, cancel_ref);
				
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
			if (sock.getAddr() == ui->second->sock.getAddr())
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
	
	#ifndef NDEBUG
	cout << "+ New user #" << id << " [" << sock.address() << "]" << endl;
	#endif
	
	User* usr = new User(id, sock);
	
	fSet(usr->events, event::read<EventSystem>::value);
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
	cout << "[Server] Removing user #" << usr->id /*<< " [" << usr->sock.address() << "]"*/ << endl;
	#endif
	
	switch (reason)
	{
		#ifndef NDEBUG
		case protocol::UserInfo::BrokenPipe:
			cout << "- User #" << usr->id /*<< " [" << usr->sock.address() << "]" <<*/ << " lost (broken pipe)" << endl;
			break;
		case protocol::UserInfo::Disconnect:
			cout << "- User #" << usr->id /*<< " [" << usr->sock.address() << "]" <<*/ << " disconnected" << endl;
			break;
		default:
			// do nothing
			break;
		#endif
	}
	
	usr->sock.shutdown(SHUT_RDWR);
	
	// Remove socket from event system
	ev.remove(usr->sock.fd());
	
	// Clear the fake tunnel of any possible instance of this user.
	// We're the source...
	tunnel_i ti;
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
	
	if (usr->syncing != 0)
	{
		Session *session = getSession(usr->syncing);
		if (session != 0)
		{
			//session->waitingSync.erase(0); // FIXME
		}
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
	#ifndef NDEBUG
	cout << "- Session #" << session->id << " destroyed" << endl;
	#endif
	
	freeSessionID(session->id);
	sessions.erase(session->id);
	delete session;
	session = 0;
}

bool Server::init() throw(std::bad_alloc)
{
	assert(state == Server::Dead);
	
	#ifndef LINUX
	srand(time(0) - 513); // FIXME: Need better seed value
	#endif
	
	if (lsock.create() == INVALID_SOCKET)
	{
		cerr << "- Failed to create a socket." << endl;
		return false;
	}
	
	lsock.block(false); // nonblocking
	//lsock.linger(false, 0);
	//lsock.reuse_port(true); // reuse port
	#ifndef WIN32
	lsock.reuse_addr(true); // reuse address
	#endif
	
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
	else
	{
		#ifndef NDEBUG
		cout << "+ Listening on port " << lsock.port() << endl << endl;
		#endif
		
		// add listening socket to event system
		if (event::has_accept<EventSystem>::value)
			ev.add(lsock.fd(), event::accept<EventSystem>::value);
		else
			ev.add(lsock.fd(), event::read<EventSystem>::value);
		
		// set event timeout
		ev.timeout(5000);
		
		state = Server::Init;
		
		return true;
	}
	
	return false;
}

bool Server::validateUserName(User* usr) const throw()
{
	assert(usr != 0);
	assert(fIsSet(requirements, static_cast<uint8_t>(protocol::requirements::EnforceUnique)));
	
	if (usr->name.size == 0)
		return false;
	
	for (users_const_i ui(users.begin()); ui != users.end(); ++ui)
	{
		if (ui->second != usr // skip self
			and usr->name.size == ui->second->name.size // equal length
			and (memcmp(usr->name.ptr, ui->second->name.ptr, usr->name.size) == 0))
			return false;
	}
	
	return true;
}

bool Server::validateSessionTitle(const Array<char>& title) const throw()
{
	assert(title.ptr != 0 and title.size > 0);
	assert(fIsSet(requirements, static_cast<uint8_t>(protocol::requirements::EnforceUnique)));
	
	#ifdef STRICT_UNIQUENESS
	// Session title is never unique if it's an empty string.
	if (title.size == 0)
		return false;
	#endif
	
	for (sessions_const_i si(sessions.begin()); si != sessions.end(); ++si)
		if (title.size == si->second->title.size and (memcmp(title.ptr, si->second->title.ptr, title.size) == 0))
			return false;
	
	return true;
}

void Server::cullIdlers() throw()
{
	if (next_timer > current_time)
	{ /* do nothing */ }
	else if (utimer.empty())
		next_timer = current_time + 1800;
	else
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
	
	
	event::fd_type<EventSystem>::fd_t fd;
	event::ev_type<EventSystem>::ev_t events;
	
	users_const_i ui;
	
	// main loop
	do
	{
		switch (ev.wait())
		{
		case -1:
			cerr << "- Error in event system: "/* << ev.getError()*/ << endl;
			state = Server::Error;
			return -1;
		case 0:
			// check timer
			current_time = time(0);
			cullIdlers();
			break;
		default:
			current_time = time(0);
			while (ev.getEvent(fd, events))
			{
				assert(fd != event::invalid_fd<EventSystem>::value);
				if (fd == lsock.fd())
				{
					cullIdlers();
					uAdd( lsock.accept() );
					continue;
				}
				
				ui = users.find(fd);
				assert(ui != users.end());
				usr = ui->second;
				
				if (event::has_error<EventSystem>::value
					and fIsSet(events, event::error<EventSystem>::value))
				{
					uRemove(usr, protocol::UserInfo::BrokenPipe);
					continue;
				}
				if (event::has_hangup<EventSystem>::value and 
					fIsSet(events, event::hangup<EventSystem>::value))
				{
					uRemove(usr, protocol::UserInfo::Disconnect);
					continue;
				}
				if (fIsSet(events, event::read<EventSystem>::value))
				{
					uRead(usr);
					if (!usr) continue;
				}
				if (fIsSet(events, event::write<EventSystem>::value))
				{
					uWrite(usr);
					if (!usr) continue;
				}
			}
			break;
		}
	}
	while (state == Server::Active);
	
	return 0;
}

void Server::stop() throw()
{
	state = Server::Exiting;
}

void Server::reset() throw()
{
	lsock.close();
	
	User *usr;
	for (users_i ui(users.begin()); ui != users.end(); ui=users.begin())
	{
		usr = ui->second;
		uRemove(usr, protocol::UserInfo::None);
	}
	
	Session *session;
	for (sessions_i si(sessions.begin()); si != sessions.end(); si=sessions.begin())
	{
		session = si->second;
		sRemove(session);
	}
	
	tunnel.clear();
	utimer.clear();
	
	state = Server::Dead;
}

Session* Server::getSession(const uint8_t session_id) throw()
{
	const sessions_const_i si(sessions.find(session_id));
	return (si == sessions.end() ? 0 : si->second);
}

const Session* Server::getConstSession(const uint8_t session_id) const throw()
{
	const sessions_const_i si(sessions.find(session_id));
	return (si == sessions.end() ? 0 : si->second);
}

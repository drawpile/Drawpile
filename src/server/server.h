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

#ifndef Server_C_Included
#define Server_C_Included

#include <algorithm>
#include <ctime>

#include "../shared/SHA1.h"

#include "sockets.h"
#include "event.h"
#include "user.h"
#include "session.h"

#include "../shared/protocol.h"

#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<protocol::Message> message_ref;

#include <queue> // user and session ID queue

#if defined(HAVE_HASH_MAP)
	#include <ext/hash_map>
#endif

#include <map> // user and session maps
#include <set> // utimer set

#include <stdexcept> // std::exception class

namespace srv_defaults
{

const time_t time_limit(180);
const uint8_t name_len_limit(8);
const uint16_t min_dimension(400);
const uint8_t max_subscriptions(1);
const uint8_t session_limit(1);

}

//! Server
class Server
{
protected:
	/* data */
	
	// Server state
	uint8_t state;
	
	// Event interface
	Event ev;
	
	std::queue<uint8_t>
		// Free user IDs
		user_ids,
		// Free session IDs
		session_ids;
	
	// FD to user mapping
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<fd_t, User*> users;
	#else
	std::map<fd_t, User*> users;
	#endif
	
	// Session ID to session mapping
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<uint8_t, Session*> sessions;
	#else
	std::map<uint8_t, Session*> sessions;
	#endif
	
	// Fake tunnel between two users. Only used for passing raster, for now.
	// source_fd -> target_fd
	std::multimap<User*, User*> tunnel;
	
	// for killing users idling in login
	std::set<User*> utimer;
	
	// listening socket
	Socket lsock;
	
	char *password, *a_password;
	size_t pw_len, a_pw_len,
		user_limit,
		session_limit,
		max_subscriptions,
		name_len_limit;
	
	time_t
		time_limit,
		current_time,
		next_timer;
	
	uint16_t
		hi_port,
		lo_port,
		min_dimension;
	
	uint8_t
		requirements,
		extensions;
	
	bool
		enforceUnique,
		wideStrings,
		noGlobalChat,
		extDeflate,
		extPalette,
		extChat;
	
	uint8_t
		default_user_mode;
	
	bool Transient, LocalhostAdmin, DaemonMode, blockDuplicateConnections;
	
	/* statistics */
	
	#ifndef NDEBUG
	size_t
		// Number of times pre-allocation was under-estimated
		protocolReallocation,
		// Largest number of messages linked
		largestLinkList,
		// largest user output buffer
		largestOutputBuffer,
		// largest user input buffer
		largestInputBuffer,
		// number of times buffer.reposition() was called from server
		bufferRepositions,
		// number of times buffer.setBuffer() was called from server
		bufferResets,
		// number of times compressed data was discarded
		discardedCompressions,
		// smallest data set size with beneficial compression ratio
		smallestCompression,
		// bandwidth saved by deflate
		deflateSaved,
		// bandwidth saved by linking
		linkingSaved;
	#endif
	
	/* functions */
	
	// Frees user ID
	inline
	void freeUserID(const uint8_t id) throw();
	
	// Frees session ID
	inline
	void freeSessionID(const uint8_t id) throw();
	
	// Get free user ID
	inline
	const uint8_t getUserID() throw();
	
	// Get free session ID
	inline
	const uint8_t getSessionID() throw();
	
	/* *** Instances *** */
	
	SHA1 hash;
	
	/* *** Generate messages *** */
	
	inline
	message_ref msgHostInfo() const throw(std::bad_alloc);
	
	inline
	message_ref msgAuth(User& usr, const uint8_t session) const throw(std::bad_alloc);
	
	inline
	message_ref msgUserEvent(const User& usr, const uint8_t session_id, const uint8_t event) const throw(std::bad_alloc);
	
	inline
	message_ref msgError(const uint8_t session, const uint16_t errorCode) const throw(std::bad_alloc);
	
	inline
	message_ref msgAck(const uint8_t session, const uint8_t msgtype) const throw(std::bad_alloc);
	
	inline
	message_ref msgSyncWait(const uint8_t session_id) const throw(std::bad_alloc);
	
	inline
	message_ref msgSessionInfo(const Session& session) const throw(std::bad_alloc);
	
	/* *** Something else *** */
	
	// Write to user socket
	inline
	void uWrite(User*& usr) throw();
	
	// Read from user socket
	inline
	void uRead(User*& usr) throw(std::bad_alloc);
	
	// Process all read data.
	inline
	void uProcessData(User*& usr) throw();
	
	// Handle user message.
	inline
	void uHandleMsg(User*& usr) throw(std::bad_alloc);
	
	// Handle ACKs
	inline
	void uHandleAck(User*& usr) throw();
	
	// Forward raster to those expecting it.
	inline
	void uTunnelRaster(User& usr) throw();
	
	// Handle SessionEvent message
	inline
	void uSessionEvent(Session*& session, User*& usr) throw();
	
	// Handle instruction message
	inline
	void uHandleInstruction(User*& usr) throw(std::bad_alloc);
	
	// Handle user login.
	inline
	void uHandleLogin(User*& usr) throw(std::bad_alloc);
	
	// Send message to session
	inline
	void Propagate(const Session& session, message_ref msg, User* source=0, const bool toAll=false) throw();
	
	// Send message to user
	/*
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 */
	inline
	void uSendMsg(User& usr, message_ref msg) throw();
	
	// Begin synchronizing the session
	inline
	void SyncSession(Session* session) throw();
	
	// Break synchronization with user.
	inline
	void breakSync(User& usr) throw();
	
	//
	inline
	void uJoinSession(User* usr, Session* session) throw();
	
	// Needs session reference because it might get destroyed.
	inline
	void uLeaveSession(User& usr, Session*& session, const uint8_t reason=protocol::user_event::Leave) throw();
	
	// Adds user
	inline
	void uAdd(Socket sock) throw(std::bad_alloc);
	
	// Removes user and does cleaning..
	inline
	void uRemove(User*& usr, const uint8_t reason) throw();
	
	// Delete session and do some cleaning
	inline
	void sRemove(Session*& session) throw();
	
	// check user name uniqueness
	inline
	bool validateUserName(User* usr) const throw();
	
	// check session title uniqueness
	inline
	bool validateSessionTitle(const char* name, const uint8_t len) const throw();
	
	// Reprocesses deflated data stream
	inline
	void DeflateReprocess(User*& usr) throw(std::bad_alloc);
	
	// cull idle users
	inline
	void cullIdlers() throw();
	
	// regenerate password seed
	inline
	void uRegenSeed(User& usr) const throw();
	
	inline
	bool isOwner(const User& usr, const Session& session) const throw();
public:
	//! ctor
	Server() throw();
	
	//! dtor
	~Server() throw();
	
	//! Initializes anything that need to be done so.
	/**
	 * @return false on error, true otherwise
	 * @throw std::bad_alloc (only if EV_EPOLL is defined.)
	 */
	bool init() throw(std::bad_alloc);
	
	//! Set name length limit (default: 8)
	inline
	void setNameLengthLimit(const uint8_t limit) throw() { name_len_limit = limit; }
	
	//! Set server password
	inline
	void setPassword(char* pwstr, const uint8_t len) throw() { password = pwstr; pw_len = len; }
	
	//! Set admin server password
	inline
	void setAdminPassword(char* pwstr, const uint8_t len) throw() { a_password = pwstr; a_pw_len = len; }
	
	//! Set user limit
	inline
	void setUserLimit(const uint8_t ulimit) throw() { user_limit = ulimit; }
	
	//! Set listening port range
	inline
	void setPorts(const uint16_t lo, const uint16_t hi) throw()
	{
		lo_port = lo;
		hi_port = std::max(hi, lo);
	}
	
	//! Set operation mode
	inline
	void setTransient(const bool x) throw() { Transient = x; }
	inline
	void setLocalhostAdmin(const bool x) throw() { LocalhostAdmin = x; }
	inline
	void setDaemonMode(const bool x) throw() { DaemonMode = x; }
	
	//! Set client requirements
	inline
	void setRequirement(const uint8_t req) throw() { fSet(requirements, req); }
	
	//! Set minimum board dimension (width or height)
	inline
	void setMinDimension(const uint16_t mindim) throw() { min_dimension = mindim; }
	
	//! Set UTF-16 support
	inline
	void setUTF16(const bool x)
	{
		if (x)
			fSet(requirements, protocol::requirements::WideStrings );
		else
			fClr(requirements, protocol::requirements::WideStrings );
	}
	
	//! Set default user mode
	inline
	void setUserMode(const uint8_t x) { default_user_mode = x; }
	
	//! Set session limit on server
	inline
	void setSessionLimit(const uint8_t x) { session_limit = x; }
	
	//! Set per user subscription limit
	inline
	void setSubscriptionLimit(const uint8_t x) { max_subscriptions = x; }
	
	//! Allow/disallow duplicate connections from same address
	inline
	void blockDuplicateConnectsion(const bool x) { blockDuplicateConnections = x; }
	
	//! Enter main loop
	int run() throw();
	
	#ifndef NDEBUG
	//! Show statistics
	void stats() const throw();
	#endif
}; // class Server

#endif // Server_C_Included

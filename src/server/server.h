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

#include "common.h"

#include "../shared/SHA1.h"
#include "../shared/protocol.h"

#include "sockets.h"
#include "user.h"
#include "session.h"

#include "ev/event.h"

#include "message.h" // message_ref

#include <ctime> // time_t, time(0)
#include <queue>
#include <map> // tunnel
#include <set> // utimer

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
	enum State {
		Init,
		Active,
		Exiting,
		Dead,
		Error
	} state;
	
	EventSystem ev;
	
	std::queue<uint8_t>
		// Free user IDs
		user_ids,
		// Free session IDs
		session_ids;
	
	// FD to user mapping
	std::map<fd_t, User*> users;
	
	// Session ID to session mapping
	std::map<uint8_t, Session*> sessions;
	
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
	
	bool Transient, LocalhostAdmin, blockDuplicateConnections;
	
	/* functions */
	
	// Frees user ID
	void freeUserID(const uint8_t id) throw();
	
	// Frees session ID
	void freeSessionID(const uint8_t id) throw();
	
	// Get free user ID
	const uint8_t getUserID() throw();
	
	// Get free session ID
	const uint8_t getSessionID() throw();
	
	/* *** Instances *** */
	
	SHA1 hash;
	
	/* *** Generate messages *** */
	
	message_ref msgHostInfo() const throw(std::bad_alloc);
	
	message_ref msgPWRequest(User& usr, const uint8_t session) const throw(std::bad_alloc);
	
	message_ref msgUserEvent(const User& usr, const uint8_t session_id, const uint8_t event) const throw(std::bad_alloc);
	
	message_ref msgError(const uint8_t session, const uint16_t errorCode) const throw(std::bad_alloc);
	
	message_ref msgAck(const uint8_t session, const uint8_t msgtype) const throw(std::bad_alloc);
	
	message_ref msgSyncWait(const uint8_t session_id) const throw(std::bad_alloc);
	
	message_ref msgSessionInfo(const Session& session) const throw(std::bad_alloc);
	
	/* *** Something else *** */
	
	// Write to user socket
	void uWrite(User*& usr) throw();
	
	// Read from user socket
	void uRead(User*& usr) throw(std::bad_alloc);
	
	// Process all read data.
	void uProcessData(User*& usr) throw();
	
	// Process stroke info, stroke end and tool info
	void uHandleDrawing(User& usr) throw();
	
	void uHandlePassword(User& usr) throw();
	
	// Handle user message.
	void uHandleMsg(User*& usr) throw(std::bad_alloc);
	
	// Handle ACKs
	void uHandleAck(User*& usr) throw();
	
	// Forward raster to those expecting it.
	void uTunnelRaster(User& usr) throw();
	
	// Handle SessionEvent message
	void uSessionEvent(Session*& session, User*& usr) throw();
	
	// Handle instruction message
	void uSessionInstruction(User*& usr) throw(std::bad_alloc);
	
	void uSetPassword(User*& usr) throw();
	
	// Handle user login.
	void uHandleLogin(User*& usr) throw(std::bad_alloc);
	
	void uLayerEvent(User*& usr) throw();
	
	// Send message to session
	void Propagate(const Session& session, message_ref msg, User* source=0) throw();
	
	// Send message to user
	/*
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 */
	void uQueueMsg(User& usr, message_ref msg) throw();
	
	// Begin synchronizing the session
	void SyncSession(Session* session) throw();
	
	// Break synchronization with user.
	void breakSync(User& usr) throw();
	
	//
	void uJoinSession(User& usr, Session& session) throw();
	
	// Needs session reference because it might get destroyed.
	void uLeaveSession(User& usr, Session*& session, const protocol::UserInfo::uevent reason=protocol::UserInfo::Leave) throw();
	
	// Adds user
	void uAdd(Socket sock) throw(std::bad_alloc);
	
	// Removes user and does cleaning..
	void uRemove(User*& usr, const protocol::UserInfo::uevent reason) throw();
	
	// Delete session and do some cleaning
	void sRemove(Session*& session) throw();
	
	// check user name uniqueness
	bool validateUserName(User* usr) const throw();
	
	// check session title uniqueness
	bool validateSessionTitle(const char* name, const uint8_t len) const throw();
	
	// Reprocesses deflated data stream
	void DeflateReprocess(User*& usr) throw(std::bad_alloc);
	
	// cull idle users
	void cullIdlers() throw();
	
	// regenerate password seed
	void uRegenSeed(User& usr) const throw();
	
	bool isOwner(const User& usr, const Session& session) const throw();
	
	Session* getSession(const uint8_t session_id) throw();
	const Session* getConstSession(const uint8_t session_id) const throw();
	
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
	void setNameLengthLimit(const uint8_t limit) throw() { name_len_limit = limit; }
	
	//! Set server password
	void setPassword(char* pwstr, const uint8_t len) throw()
	{
		if (password != 0)
			delete [] password;
		password = pwstr;
		pw_len = len;
	}
	
	//! Set admin server password
	void setAdminPassword(char* pwstr, const uint8_t len) throw()
	{
		if (a_password != 0)
			delete [] a_password;
		a_password = pwstr;
		a_pw_len = len;
	}
	
	//! Set user limit
	void setUserLimit(const uint8_t ulimit) throw()
	{
		user_limit = ulimit;
	}
	
	//! Set listening port range
	void setPorts(const uint16_t lo, const uint16_t hi) throw()
	{
		lo_port = lo;
		hi_port = std::max(hi, lo);
	}
	
	//! Set operation mode
	void setTransient(const bool x) throw()
	{
		Transient = x;
	}
	
	//!
	void setLocalhostAdmin(const bool x) throw()
	{
		LocalhostAdmin = x;
	}
	
	//! Set client requirements
	void setRequirement(const uint8_t req) throw() { fSet(requirements, req); }
	
	//! Set minimum board dimension (width or height)
	void setMinDimension(const uint16_t mindim) throw() { min_dimension = mindim; }
	
	//! Set UTF-16 support
	void setUTF16(const bool x)
	{
		if (x)
			fSet(requirements, static_cast<uint8_t>(protocol::requirements::WideStrings));
		else
			fClr(requirements, static_cast<uint8_t>(protocol::requirements::WideStrings));
	}
	
	//! Set default user mode
	void setUserMode(const uint8_t x) { default_user_mode = x; }
	
	//! Set session limit on server
	void setSessionLimit(const uint8_t x) { session_limit = x; }
	
	//! Set per user subscription limit
	void setSubscriptionLimit(const uint8_t x) { max_subscriptions = x; }
	
	//! Allow/disallow duplicate connections from same address
	void blockDuplicateConnectsion(const bool x) { blockDuplicateConnections = x; }
	
	//! Enter main loop
	int run() throw();
	
	#ifndef NDEBUG
	//! Show statistics
	void stats() const throw();
	#endif
}; // class Server

#endif // Server_C_Included

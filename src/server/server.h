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
#include "../shared/protocol.errors.h"

#include "sockets.h"
#include "user.h"
#include "session.h"

#include "ev/event.h"

#include "message.h" // message_ref

#include <ctime> // time_t, time(0)
#include <map> // tunnel
#include <set> // utimer

//! Server defaults
namespace srv_defaults {

//! Time limit before culling user
const uint time_limit = 180;

}

//! Server
class Server
{
protected:
	/* data */
	
	//! Server state
	enum State {
		Init,
		Active,
		Exiting,
		Dead,
		Error
	} state;
	
	//! Event mechanism
	EventSystem ev;
	
	bool
		//! User identifiers
		user_ids[254],
		//! Session identifiers
		session_ids[254];
	
	//! FD to user mapping
	std::map<fd_t, User*> users;
	
	//! Session ID to session mapping
	std::map<uint8_t, Session*> sessions;
	
	//! Fake tunnel between two users. Only used for passing raster, for now.
	/** source_fd -> target_fd */
	std::multimap<User*, User*> tunnel;
	
	//! Users eligible for culling
	std::set<User*> utimer;
	
	//! Listening socket
	Socket lsock;
	
	Array<char>
		//! Server password
		password,
		//! Admin password
		admin_password;
	
	size_t
		//! User limit
		user_limit,
		//! Session limit
		session_limit,
		//! User session subscription limit
		max_subscriptions,
		//! Name length limit
		name_len_limit;
	
	time_t
		//! Wait time before culling
		time_limit,
		//! Substitute for time()
		current_time,
		//! The next time we cull idlers
		next_timer;
	
	uint16_t
		//! Listening port, upper bound
		hi_port,
		//! Listening port, lower bound
		lo_port,
		//! Minimum canvas dimension
		min_dimension;
	
	uint8_t
		//! Server requirements
		requirements,
		//! Supported extensions
		extensions;
	
	bool
		//! Enforce unique user and session names
		enforceUnique,
		//! UTF-16 instead of UTF-8
		wideStrings,
		//! Disallow global chatting
		noGlobalChat,
		//! Deflate extension
		extDeflate,
		//! Palette extension
		extPalette,
		//! Chat extension
		extChat;
	
	//! Default user mode
	uint8_t default_user_mode;
	
	bool
		//! Shutdown server once all users have left
		Transient,
		//! Auto-promote localhost connections to Admin status
		LocalhostAdmin,
		//! Block duplicate connections from same source IP
		blockDuplicateConnections;
	
	/* functions */
	
	//! Frees user ID
	void freeUserID(const uint8_t id) throw();
	
	//! Frees session ID
	void freeSessionID(const uint8_t id) throw();
	
	//! Get free user ID
	const uint8_t getUserID() throw();
	
	//! Get free session ID
	const uint8_t getSessionID() throw();
	
	/* *** Generate messages *** */
	
	//! Generate host info message
	message_ref msgHostInfo() const throw(std::bad_alloc);
	
	//! Generate password request message
	/**
	 * @param usr
	 * @param session
	 */
	message_ref msgPWRequest(User& usr, const uint8_t session) const throw(std::bad_alloc);
	
	//! Generate user event message
	/**
	 * @param usr
	 * @param session_id
	 * @param event
	 */
	message_ref msgUserEvent(const User& usr, const uint8_t session_id, const uint8_t event) const throw(std::bad_alloc);
	
	//! Generate error message
	/**
	 * @param session
	 * @param errorCode
	 */
	message_ref msgError(const uint8_t session, const uint16_t errorCode) const throw(std::bad_alloc);
	
	//! Generate ACK message
	/**
	 * @param session
	 * @param msgtype
	 */
	message_ref msgAck(const uint8_t session, const uint8_t msgtype) const throw(std::bad_alloc);
	
	//! Generate sync-wait message
	/**
	 * @param session_id
	 */
	message_ref msgSyncWait(const uint8_t session_id) const throw(std::bad_alloc);
	
	//! Generate session info message
	/**
	 * @param session
	 */
	message_ref msgSessionInfo(const Session& session) const throw(std::bad_alloc);
	
	/* *** Something else *** */
	
	//! Write to user socket
	/**
	 * @param usr
	 */
	void uWrite(User*& usr) throw();
	
	//! Read from user socket
	/**
	 * @param usr
	 */
	void uRead(User*& usr) throw(std::bad_alloc);
	
	//! Process all read data.
	/**
	 * @param usr
	 */
	void uProcessData(User*& usr) throw();
	
	//! Process stroke info, stroke end and tool info
	/**
	 * @param usr
	 */
	void uHandleDrawing(User& usr) throw();
	
	//! 
	/**
	 * @param usr
	 */
	void uHandlePassword(User& usr) throw();
	
	//! Handle user message.
	/**
	 * @param usr
	 */
	void uHandleMsg(User*& usr) throw(std::bad_alloc);
	
	//! Handle ACKs
	/**
	 * @param usr
	 */
	void uHandleAck(User*& usr) throw();
	
	//! Forward raster to those expecting it.
	/**
	 * @param usr Source for raster
	 */
	void uTunnelRaster(User& usr) throw();
	
	//! Handle SessionEvent message
	/**
	 * @param session 
	 * @param usr
	 */
	void uSessionEvent(Session*& session, User*& usr) throw();
	
	//! Handle instruction message
	/**
	 * @param usr user whose session instruction to handle
	 */
	void uSessionInstruction(User*& usr) throw(std::bad_alloc);
	
	//! Set server or session password
	/**
	 * @param usr user whose password set instruction to handle
	 */
	void uSetPassword(User*& usr) throw();
	
	//! Handle user login.
	/**
	 * @param usr user whose login to handle
	 */
	void uHandleLogin(User*& usr) throw(std::bad_alloc);
	
	//! Handle user info in login
	/**
	 * @param usr user whose login user info to handle
	 */
	void uLoginInfo(User& usr) throw();
	
	//! Handle layer event
	/**
	 * @param usr user whose layer event to handle
	 */
	void uLayerEvent(User*& usr) throw();
	
	//! Send message to all users in session
	/**
	 * @param session reference to session to which we're propagating data to
	 * @param msg shared_ptr of the message to be propagated
	 * @param source source user to whom the message will NOT be sent to
	 */
	void Propagate(const Session& session, message_ref msg, User* source=0) throw();
	
	//! Check password hash
	/**
	 * @param hashdigest Digest to compare against
	 * @param str string for hashing
	 * @param len string length
	 * @param seed password seed
	 * @return true if the digests match, false otherwise
	 */
	bool CheckPassword(const char *hashdigest, const char *str, const size_t len, const char *seed) throw();
	
	//! Queue message to user
	/**
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 *
	 * @param usr
	 * @param msg
	 */
	void uQueueMsg(User& usr, message_ref msg) throw();
	
	//! Begin synchronizing the session
	/**
	 * @param session
	 */
	void SyncSession(Session* session) throw();
	
	//! Break synchronization with user.
	/**
	 * @param usr
	 */
	void breakSync(User& usr) throw();
	
	//! Attach user to session and begin user synchronization if necessary
	/**
	 * @param usr
	 * @param session
	 */
	void uJoinSession(User& usr, Session& session) throw();
	
	//! Needs session reference because it might get destroyed.
	/**
	 * @param usr
	 * @param session
	 * @param reason
	 */
	void uLeaveSession(User& usr, Session*& session, const protocol::UserInfo::uevent reason=protocol::UserInfo::Leave) throw();
	
	//! Adds user
	/**
	 * @param sock
	 */
	void uAdd(Socket sock) throw(std::bad_alloc);
	
	//! Removes user and does cleaning..
	/**
	 * @param usr
	 * @param reason
	 */
	void uRemove(User*& usr, const protocol::UserInfo::uevent reason) throw();
	
	//! Delete session and do some cleaning
	/**
	 * @param session
	 */
	void sRemove(Session*& session) throw();
	
	//! Check user name uniqueness
	/**
	 * @param usr
	 */
	bool validateUserName(User* usr) const throw();
	
	//! Check session title uniqueness
	/**
	 * @param title
	 */
	bool validateSessionTitle(const Array<char>& title) const throw();
	
	//! Reprocesses deflated data stream
	/**
	 * @param usr
	 */
	void DeflateReprocess(User*& usr) throw(std::bad_alloc);
	
	//! Deflate outgoing data
	void Deflate(Buffer& buffer, size_t len) throw(std::bad_alloc);
	
	//! Cull idle users
	void cullIdlers() throw();
	
	//! Regenerate password seed
	/**
	 * @param usr
	 */
	void uRegenSeed(User& usr) const throw();
	
	//! Check if user is owner of session
	/**
	 * @param usr
	 * @param session
	 */
	bool isOwner(const User& usr, const Session& session) const throw();
	
	//! Get Session* pointer
	/**
	 * @param session_id
	 */
	Session* getSession(const uint8_t session_id) throw();
	
	//! Get const Session* pointer
	/**
	 * @param session_id
	 */
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
	/**
	 * @param limit
	 */
	void setNameLengthLimit(const uint8_t limit) throw() { name_len_limit = limit; }
	
	//! Set server password
	/**
	 * @param pwstr
	 * @param len
	 */
	void setPassword(char* pwstr, const uint8_t len) throw()
	{
		password.set(pwstr, len);
	}
	
	//! Set admin server password
	/**
	 * @param pwstr
	 * @param len
	 */
	void setAdminPassword(char* pwstr, const uint8_t len) throw()
	{
		admin_password.set(pwstr, len);
	}
	
	//! Set user limit
	/**
	 * @param ulimit
	 */
	void setUserLimit(const uint8_t ulimit) throw() { user_limit = ulimit; }
	
	//! Set listening port range
	/**
	 * @param lo
	 * @param hi
	 */
	void setPorts(const uint16_t lo, const uint16_t hi) throw()
	{
		lo_port = lo;
		hi_port = std::max(hi, lo);
	}
	
	//! Set operation mode
	/**
	 * @param x
	 */
	void setTransient(const bool x) throw() { Transient = x; }
	
	//!
	/**
	 * @param x
	 */
	void setLocalhostAdmin(const bool x) throw() { LocalhostAdmin = x; }
	
	//! Set client requirements
	/**
	 * @param req
	 */
	void setRequirement(const uint8_t req) throw() { fSet(requirements, req); }
	
	//! Set minimum board dimension (width or height)
	/**
	 * @param mindim
	 */
	void setMinDimension(const uint16_t mindim) throw() { min_dimension = mindim; }
	
	//! Set UTF-16 support
	/**
	 * @param x
	 */
	void setUTF16(const bool x)
	{
		if (x)
			fSet(requirements, static_cast<uint8_t>(protocol::requirements::WideStrings));
		else
			fClr(requirements, static_cast<uint8_t>(protocol::requirements::WideStrings));
	}
	
	//! Set default user mode
	/**
	 * @param x
	 */
	void setUserMode(const uint8_t x) { default_user_mode = x; }
	
	//! Set session limit on server
	/**
	 * @param x
	 */
	void setSessionLimit(const uint8_t x) { session_limit = x; }
	
	//! Set per user subscription limit
	/**
	 * @param x
	 */
	void setSubscriptionLimit(const uint8_t x) { max_subscriptions = x; }
	
	//! Allow/disallow duplicate connections from same address
	/**
	 * @param x
	 */
	void blockDuplicateConnectsion(const bool x) { blockDuplicateConnections = x; }
	
	//! Enter main loop
	int run() throw();
	
}; // class Server

#endif // Server_C_Included

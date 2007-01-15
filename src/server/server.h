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

#include "../shared/SHA1.h"

#include "sockets.h"
#include "event.h"
#include "user.h"
#include "session.h"

#include "../shared/protocol.h"

#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<protocol::Message> message_ref;

/* iterators */
typedef std::map<uint8_t, Session*>::iterator session_iterator;
typedef std::map<fd_t, User*>::iterator user_iterator;
typedef std::multimap<uint8_t, fd_t>::iterator tunnel_iterator;

//#include <sys/time.h>

#include <stdexcept>

#include <queue>
#include <map>
#include <list>

//! foo
namespace defaults
{

//! Hard limit for users and sessions.
const int hard_limit = 255;

} // namespace defaults

//! Server
class Server
{
protected:
	/* data */
	
	// Server state
	uint8_t state;
	
	// Event interface
	Event ev;
	
	// Used and free user IDs
	std::queue<uint8_t> user_ids;
	
	// Used and free session IDs
	std::queue<uint8_t> session_ids;
	
	// FD to user mapping
	std::map<fd_t, User*> users;
	
	// Session ID to session mapping
	std::map<uint8_t, Session*> sessions;
	
	// Fake tunnel between two users. Only used for passing raster, for now.
	// first->source, second->target
	std::multimap<uint8_t, fd_t> tunnel;
	
	// listening socket
	Socket lsock;
	
	char *password, *a_password;
	size_t pw_len, a_pw_len,
		user_limit,
		session_limit,
		max_subscriptions,
		name_len_limit;
	
	uint16_t
		hi_port,
		lo_port,
		min_dimension;
	
	uint8_t
		requirements,
		extensions;
	
	uint8_t
		default_user_mode;
	
	uint8_t
		opmode;
	
	/* functions */
	
	// Frees user ID
	void freeUserID(uint8_t id) throw();
	
	// Frees session ID
	void freeSessionID(uint8_t id) throw();
	
	// Cleanup anything that's left.
	inline
	void cleanup() throw();
	
	// Get free user ID
	inline
	uint8_t getUserID() throw();
	
	// Get free session ID
	inline
	uint8_t getSessionID() throw();
	
	/* *** Instances *** */
	
	SHA1 hash;
	
	/* *** Generate messages *** */
	
	inline
	message_ref msgHostInfo() const throw(std::bad_alloc);
	
	inline
	message_ref msgAuth(User*& usr, uint8_t session) const throw(std::bad_alloc);
	
	inline
	message_ref uCreateEvent(User*& usr, Session*& session, uint8_t event) const throw(std::bad_alloc);
	
	inline
	message_ref msgError(uint8_t session, uint16_t errorCode) const throw(std::bad_alloc);
	
	inline
	message_ref msgAck(uint8_t session, uint8_t msgtype) const throw(std::bad_alloc);
	
	inline
	message_ref msgSyncWait(Session*& session) const throw(std::bad_alloc);
	
	/* *** Something else *** */
	
	// Write to user socket
	void uWrite(User*& usr) throw();
	
	// Read from user socket
	void uRead(User*& usr) throw(std::bad_alloc);
	
	// Process all read data.
	void uProcessData(User*& usr) throw();
	
	// Handle user message.
	void uHandleMsg(User*& usr) throw(std::bad_alloc);
	
	// Handle ACKs
	void uHandleAck(User*& usr) throw();
	
	// Forward raster to those expecting it.
	void uTunnelRaster(User*& usr) throw();
	
	// Handle SessionEvent message
	void uSessionEvent(Session*& session, User*& usr) throw();
	
	// Handle instruction message
	void uHandleInstruction(User*& usr) throw(std::bad_alloc);
	
	// Handle user login.
	void uHandleLogin(User*& usr) throw(std::bad_alloc);
	
	// Send message to session
	void Propagate(Session*& session, message_ref msg, uint8_t source=protocol::null_user, bool toAll=false) throw();
	
	// Send message to user
	/*
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 */
	void uSendMsg(User*& usr, message_ref msg) throw();
	
	// Begin synchronizing the session
	void SyncSession(Session*& session) throw();
	
	// Break synchronization with user.
	void breakSync(User*& usr) throw();
	
	// Cancel raster request.
	void cancelSync(User*& usr) throw();
	
	//
	void uJoinSession(User*& usr, Session*& session) throw();
	
	//
	void uLeaveSession(User*& usr, Session*& session, uint8_t reason=protocol::user_event::Leave) throw();
	
	// Adds user
	void uAdd(Socket* sock) throw(std::bad_alloc);
	
	// Removes user and does cleaning..
	void uRemove(User*& usr, uint8_t reason) throw();
	
	// Tests if session exists
	inline
	bool sessionExists(uint8_t session) const throw();
	
	// Tests if user is in session
	inline
	bool uInSession(User*& usr, uint8_t session) const throw();
	
	// check name/title uniqueness
	inline
	bool validateUserName(User*& usr) const throw();
	
	inline
	bool validateSessionTitle(Session*& session) const throw();
	
	void uRegenSeed(User*& usr) const throw();
public:
	//! ctor
	Server() throw();
	
	//! dtor
	~Server() throw();
	
	//! Initializes anything that need to be done so.
	/**
	 * @throw std::bad_alloc (only if EV_EPOLL is defined.)
	 */
	int init() throw(std::bad_alloc);
	
	//! Set server password
	void setPassword(char* pwstr, uint8_t len) throw() { password = pwstr; pw_len = len; }
	
	//! Set admin server password
	void setAdminPassword(char* pwstr, uint8_t len) throw() { a_password = pwstr; a_pw_len = len; }
	
	//! Set user limit
	void setUserLimit(uint8_t ulimit) throw() { user_limit = ulimit; }
	
	//! Set listening port range
	void setPorts(uint16_t lo, uint16_t hi) throw()
	{
		lo_port = lo;
		hi_port = (hi < lo ? lo : hi);
	}
	
	//! Set operation mode
	void setMode(uint8_t mode) throw() { fSet(opmode, mode); }
	
	//! Set client requirements
	void setRequirement(uint8_t req) throw() { fSet(requirements, req); }
	
	//! Set minimum board dimension (width or height)
	void setMinDimension(uint16_t mindim) throw() { min_dimension = mindim; }
	
	//! Set UTF-16 support
	void setUTF16(bool x)
	{
		if (x)
			fSet(requirements, protocol::requirements::WideStrings );
		else
			fClr(requirements, protocol::requirements::WideStrings );
	}
	
	//! Set default user mode
	bool setUserMode(uint8_t x)
	{
		default_user_mode = x;
		return true;
	}
	
	//! Enter main loop
	int run() throw();
}; // class Server

#endif // Server_C_Included

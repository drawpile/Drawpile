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

#include "sockets.h"
#include "event.h"
#include "user.h"
#include "session.h"

#include "../shared/protocol.h"

#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<User> user_ref;
typedef boost::shared_ptr<Session> session_ref;
typedef boost::shared_ptr<protocol::Message> message_ref;

//#include <sys/time.h>

#include <stdexcept>

#include <bitset>
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
	
	// Event interface
	Event ev;
	
	// Used and free user IDs
	std::bitset<defaults::hard_limit> user_ids;
	
	// Used and free session IDs
	std::bitset<defaults::hard_limit> session_ids;
	
	// FD to user mapping
	std::map<int, user_ref> users;
	
	// User ID to user mapping
	std::map<uint8_t, user_ref> user_id_map;
	
	// Session ID to session mapping
	std::map<uint8_t, session_ref> session_id_map;
	
	// listening socket
	Socket lsock;
	
	char* password;
	size_t pw_len,
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
	
	bool localhost_admin;
	
	/* functions */
	
	// Frees user ID
	void freeUserID(uint8_t id) throw();
	
	// Frees session ID
	void freeSessionID(uint8_t id) throw();
	
	// Cleanup anything that's left.
	inline
	void cleanup() throw();
	
	// Get free user ID
	uint8_t getUserID() throw();
	
	// Get free session ID
	uint8_t getSessionID() throw();
	
	// Generate messages
	message_ref msgHostInfo() throw(std::bad_alloc);
	message_ref msgAuth(user_ref& usr, uint8_t session) throw(std::bad_alloc);
	
	// Write to user socket
	void uWrite(user_ref& usr) throw();
	
	// Read from user socket
	void uRead(user_ref usr) throw(std::bad_alloc);
	
	// Process all read data.
	void uProcessData(user_ref& usr) throw();
	
	// create user info for event
	message_ref uCreateEvent(user_ref& usr, session_ref session, uint8_t event);
	
	// Handle user message.
	void uHandleMsg(user_ref& usr) throw(std::bad_alloc);
	
	// Handle instruction message
	void uHandleInstruction(user_ref& usr) throw();
	
	// Handle user login.
	void uHandleLogin(user_ref& usr) throw(std::bad_alloc);
	
	// Send message to session
	void Propagate(uint8_t session_id, message_ref msg) throw();
	
	// Send message to user
	/*
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 */
	void uSendMsg(user_ref& usr, message_ref msg) throw();
	
	// Begin synchronizing the session
	void uSyncSession(user_ref& usr, session_ref& session) throw();
	
	//
	void uJoinSession(user_ref& usr, session_ref& session) throw();
	
	//
	void uLeaveSession(user_ref& usr, session_ref& session) throw();
	
	// Adds user
	void uAdd(Socket* sock) throw(std::bad_alloc);
	
	// Removes user and does cleaning..
	void uRemove(user_ref& usr) throw();
	
	// check name/title uniqueness
	bool validateUserName(user_ref& usr) const throw();
	bool validateSessionTitle(session_ref& session) const throw();
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
	void setPassword(char* pwstr, uint8_t len) { password = pwstr; pw_len = len; }
	
	//! Set user limit
	void setUserLimit(uint8_t ulimit) { user_limit = ulimit; }
	
	//! Set listening port range
	void setPorts(uint16_t lo, uint16_t hi)
	{
		lo_port = lo;
		hi_port = (hi < lo ? lo : hi);
	}
	
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

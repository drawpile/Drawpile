/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

#include "../shared/protocol.h"

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
	std::map<int, User*> users;
	std::map<uint8_t, User*> user_id_map;
	
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
		lo_port;
	
	uint8_t
		requirements,
		extensions;
	
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
	protocol::HostInfo* msgHostInfo() throw(std::bad_alloc);
	protocol::Authentication* msgAuth(User* usr, uint8_t session) throw(std::bad_alloc);
	
	// Write to user socket
	void uWrite(User* usr) throw();

	// Read from user socket
	void uRead(User* usr) throw(std::bad_alloc);
	
	// Handle user message.
	void uHandleMsg(User* usr, protocol::Message* msg) throw(std::bad_alloc);
	
	// Send message to user
	/*
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 */
	void uSendMsg(User* usr, protocol::Message* msg) throw();
	
	// Adds user
	void uAdd(Socket* sock) throw(std::bad_alloc);
	
	// Removes user and does cleaning..
	void uRemove(User* usr) throw();
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
	void setPassword(char* pwstr, size_t len) { password = pwstr; pw_len = len; }
	void setUserLimit(size_t ulimit) { user_limit = ulimit; }
	void setPorts(uint16_t lo, uint16_t hi) { lo_port = lo; hi_port = hi; }
	
	//! Enter main loop
	int run() throw();
}; // class Server

#endif // Server_C_Included

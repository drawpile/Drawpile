/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef ServerUser_INCLUDED
#define ServerUser_INCLUDED

#ifdef DEBUG_USER
	#ifndef NDEBUG
		#include <iostream>
	#endif // NDEBUG
#endif // DEBUG_USER


#include <stdint.h>
#include <map>
#include <deque>

#include "buffer.h"

struct User;
#include "session.h"

#include "../shared/memstack.h"

#include "../shared/protocol.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"

#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<protocol::Message> message_ref;

/* iterators */
struct SessionData;
typedef std::map<uint8_t, SessionData>::iterator usr_session_iterator;

#include "sockets.h"

// User session data
struct SessionData
{
	SessionData(uint8_t id=protocol::null_user, Session *s=0) throw()
		: session(s),
		layer(protocol::null_layer),
		layer_lock(protocol::null_layer),
		mode(s->mode),
		syncWait(false)
	{
	}
	
	~SessionData() throw()
	{
	}
	
	// Session reference
	Session *session;
	
	uint8_t
		// Active layer
		layer,
		// Layer to which the user is locked to
		layer_lock;
	
	// User mode within session
	uint8_t mode;
	
	bool syncWait;
};

// User states
namespace uState
{

const uint8_t 
	// When user has just connected
	init = 3,
	
	// User has been verified to be using correct protocol.
	verified = 2,
	
	// Waiting for proper user info
	login = 1,
	
	// Waiting for password
	login_auth = 9,
	
	// Dead
	dead = 70,
	
	// Normal operation
	active = 0;
}

#ifdef CHECK_VIOLATIONS
//! Client tracking flags
namespace uTag
{

const uint8_t
	None = 0,
	CanChange = 0x01,
	HaveTool = 0x02;

}
#endif // CHECK_VIOLATIONS

// User information
struct User
	//: MemoryStack<User>
{
	User(uint8_t _id=protocol::null_user, Socket* nsock=0) throw()
		: sock(nsock),
		session(0),
		id(_id),
		events(0),
		#ifdef CHECK_VIOLATIONS
		tags(0),
		#endif // CHECK_VIOLATIONS
		state(uState::init),
		layer(protocol::null_layer),
		a_mode(protocol::user_mode::None),
		mode(protocol::user_mode::None),
		syncing(protocol::Global),
		caps(protocol::client::None),
		extensions(protocol::extensions::None),
		inMsg(0),
		deadtime(0),
		nlen(0),
		name(0),
		strokes(0)
	{
		#if defined(DEBUG_USER) and !defined(NDEBUG)
		std::cout << "User::User(" << static_cast<int>(_id)
			<< ", " << sock->fd() << ")" << std::endl;
		#endif
	}
	
	~User() throw()
	{
		#if defined(DEBUG_USER) and !defined(NDEBUG)
		std::cout << "User::~User()" << std::endl;
		#endif
		
		delete [] name,
		delete sock,
		delete inMsg;
		
		sessions.clear();
		
		queue.clear();
	}
	
	// Socket
	Socket *sock;
	
	// Currently active session
	Session *session;
	
	// User identifier
	uint8_t id;
	
	// Event I/O registered events.
	uint32_t events;
	
	uint8_t
		#ifdef CHECK_VIOLATIONS
		// Tags for protocol violation checking
		tags,
		#endif
		// User state
		state,
		// Active layer in session
		layer,
		// User mode
		a_mode,
		// Session we're currently syncing.
		mode,
		// Selected session's mode
		syncing,
		// Client capabilities
		caps,
		// Client's supported extensions
		extensions;
	
	// Subscribed sessions
	std::map<uint8_t, SessionData> sessions;
	
	// Output queue
	std::deque<message_ref> queue;
	
	// Input/output buffer
	Buffer input, output;
	
	// Currently incoming message.
	protocol::Message *inMsg;
	
	// for storing the password seed associated with this user.
	char seed[4];
	
	// Last touched.
	time_t deadtime;
	
	// Name length
	uint8_t nlen;
	
	// User name
	char* name;
	
	/* counters */
	
	u_long strokes;
};
#endif // ServerUser_INCLUDED

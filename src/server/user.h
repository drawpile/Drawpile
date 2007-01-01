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

#include <stdint.h>
#include <map>
#include <queue>

#include "buffer.h"

struct User;
#include "session.h"

#include "../shared/memstack.h"

#include "../shared/protocol.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"

#include <boost/shared_ptr.hpp>
typedef boost::shared_ptr<protocol::Message> message_ref;
typedef boost::shared_ptr<Session> session_ref;

#include "sockets.h"

// User session data
struct SessionData
{
	SessionData(uint8_t id=protocol::null_user, session_ref s=session_ref()) throw()
		: user(id),
		session(s),
		mode(s->mode)
	{
		
	}
	
	~SessionData() throw()
	{
	}
	
	// user who this describes
	uint8_t user;
	
	// Session reference
	session_ref session;
	
	// User mode within session
	uint8_t mode;
};

// User states
namespace uState
{
	// When user has just connected
	const uint8_t init = 3;
	
	// User has been verified to be using correct protocol.
	const uint8_t verified = 2;
	
	// Waiting for proper user info
	const uint8_t login = 1;
	
	// Waiting for password
	const uint8_t login_auth = 9;
	const uint8_t lobby_auth = 8;
	
	// Normal operation
	const uint8_t active = 0;
}

// User information
struct User
	//: MemoryStack<User>
{
	User(uint8_t _id=protocol::null_user, Socket* nsock=0) throw()
		: sock(nsock),
		id(_id),
		nlen(0),
		name(0),
		state(uState::init),
		inMsg(0)
	{
		#ifndef NDEBUG
		std::cout << "User::User(" << static_cast<int>(_id)
			<< ", " << sock->fd() << ")" << std::endl;
		#endif
	}
	
	~User() throw()
	{
		#ifndef NDEBUG
		std::cout << "User::~User()" << std::endl;
		#endif
		
		delete [] name,
		delete sock,
		delete inMsg;
		
		sessions.clear();
		
		while (!queue.empty())
			queue.pop();
	}
	
	// Socket
	Socket *sock;
	
	// User identifier
	uint8_t id;
	
	// Name length
	uint8_t nlen;
	
	// User name
	char* name;
	
	// Currently active session
	uint8_t session;
	
	// User mode
	uint8_t mode;
	
	// User state
	uint8_t state;
	
	// Subscribed sessions
	std::map<uint8_t, SessionData> sessions;
	
	// Output queue
	std::queue<message_ref> queue;
	
	// Event I/O registered events.
	int events;
	
	// Input/output buffer
	Buffer input, output;
	
	// Currently incoming message.
	protocol::Message *inMsg;
};
#endif // ServerUser_INCLUDED

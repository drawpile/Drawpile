/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

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

#include "../shared/memstack.h"

#include "../shared/protocol.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"

#include "sockets.h"

// User session data
struct UserData
{
	UserData(uint8_t s=protocol::Global, uint8_t m=protocol::user::None) throw()
		: session(s),
		mode(m)
	{
	}
	
	~UserData() throw()
	{
	}
	
	// Session identifier
	uint8_t session;
	
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
		
		while (buffers.size() != 0)
		{
			delete buffers.front();
			buffers.pop();
		}
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
	std::map<uint8_t, UserData> sessions;
	
	// Output buffers
	std::queue<Buffer*> buffers;
	//std::queue<Message*> messages;
	
	// Event I/O registered events.
	int events;
	
	// Input buffer
	Buffer input;
	
	// Currently incoming message.
	protocol::Message *inMsg;
};

#endif // ServerUser_INCLUDED

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

#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"

#include "sockets.h"

//! User session data
/**  */
struct UserData
{
	//! ctor
	UserData()
		: session(protocol::Global),
		mode(protocol::user::None),
		owner(false)
	{
	}
	
	/*
	UserData(const UserData& ud)
		: session(ud.session),
		mode(ud.mode),
		owner(ud.owner)
	{
	}
	*/
	
	//! dtor
	~UserData()
	{
	}
	
	//! Session identifier
	uint8_t session;
	
	//! User mode within session
	uint8_t mode;
	
	//! Is the owner of the session
	bool owner;
};

//! User information
/**  */
struct User
{
	//! ctor
	User()
		: id(0),
		nlen(0),
		name(0),
		state(0)
	{
		
	}
	
	/*
	User(const User& nu)
		: s(nu.s),
		nlen(nu.nlen),
		name(nu.name),
		session(nu.session),
		u(nu.u),
		state(nu.state),
		sessions(nu.sessions)
	{
		
	}
	*/
	
	//! dtor
	~User()
	{
		delete [] name,
		delete [] s;
		s = 0;
		name = 0;
		
		// TODO: clean sessions
	}
	
	//! Socket
	Socket *s;
	
	//! User identifier
	uint8_t id;
	
	//! Name length
	uint8_t nlen;
	
	//! User name
	char* name;
	
	//! Currently active session
	uint8_t session;
	
	//! Global user data
	UserData u;
	
	//! User state
	uint8_t state;
	
	//! Subscribed sessions
	std::map<uint8_t, UserData> sessions;
	
	//! Output buffers
	std::queue<Buffer> buffers;
	
	//! Event I/O registered events.
	int events;
	
	//! Input buffer
	Buffer input;
};

#endif // ServerUser_INCLUDED

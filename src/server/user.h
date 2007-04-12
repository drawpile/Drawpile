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

#include "../shared/templates.h" // f~()

#include <stdint.h> // [u]int##_t
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

struct SessionData; // forward declaration

/* iterators */
#if defined(HAVE_HASH_MAP)
#include <ext/hash_map>
typedef __gnu_cxx::hash_map<uint8_t, SessionData*>::iterator usr_session_i;
typedef __gnu_cxx::hash_map<uint8_t, SessionData*>::const_iterator usr_session_const_i;
#else
#include <map>
typedef std::map<uint8_t, SessionData*>::iterator usr_session_i;
typedef std::map<uint8_t, SessionData*>::const_iterator usr_session_const_i;
#endif

#include "sockets.h"

// User session data
struct SessionData
{
	SessionData(Session *s=0) throw()
		: session(s),
		layer(protocol::null_layer),
		layer_lock(protocol::null_layer),
		locked(fIsSet(s->mode, protocol::user_mode::Locked)),
		muted(fIsSet(s->mode, protocol::user_mode::Mute)),
		deaf(fIsSet(s->mode, protocol::user_mode::Deaf)),
		syncWait(false)
	{
		assert(session != 0);
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
	
	// user mode
	bool locked, muted, deaf;
	
	uint8_t getMode() const throw()
	{
		return (locked?(protocol::user_mode::Locked):0)
			+ (muted?(protocol::user_mode::Mute):0)
			+ (deaf?(protocol::user_mode::Deaf):0);
	}
	
	void setMode(const uint8_t flags) throw()
	{
		locked = fIsSet(flags, protocol::user_mode::Locked);
		muted = fIsSet(flags, protocol::user_mode::Mute);
		deaf = fIsSet(flags, protocol::user_mode::Deaf);
	}
	
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

// User information
struct User
	//: MemoryStack<User>
{
	User(const uint8_t _id=protocol::null_user, Socket* nsock=0) throw()
		: sock(nsock),
		session(0),
		id(_id),
		events(0),
		state(uState::init),
		layer(protocol::null_layer),
		syncing(protocol::Global),
		isAdmin(false),
		// client caps
		c_acks(false),
		// extensions
		ext_deflate(false),
		ext_chat(false),
		ext_palette(false),
		// active session
		a_layer(protocol::null_layer),
		a_layer_lock(protocol::null_layer),
		a_locked(false),
		a_muted(false),
		a_deaf(false),
		// other
		inMsg(0),
		level(0),
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
		
		for (usr_session_i usi(sessions.begin()); usi != sessions.end(); ++usi)
			delete usi->second;
		
		sessions.clear();
		
		queue.clear();
	}
	
	inline
	bool makeActive(uint8_t session_id) throw()
	{
		const usr_session_const_i usi(sessions.find(session_id));
		if (usi != sessions.end())
		{
			session = usi->second->session;
			
			a_layer = usi->second->layer;
			a_layer_lock = usi->second->layer_lock;
			
			a_locked = usi->second->locked;
			a_deaf = usi->second->deaf;
			a_muted = usi->second->muted;
			
			return true;
		}
		else
			return false;
	}
	
	inline
	bool inSession(uint8_t session_id) const throw()
	{
		return sessions.find(session_id) != sessions.end();
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
		// User state
		state,
		// Active layer in session
		layer,
		// Session we're currently syncing.
		syncing;
	
	// is the user server admin?
	bool isAdmin;
	
	// client capabilites
	bool c_acks;
	
	uint8_t getCapabilities() const throw()
	{
		return (c_acks?(protocol::client::AckFeedback):0);
	}
	
	void setCapabilities(const uint8_t flags) throw()
	{
		c_acks = fIsSet(flags, protocol::client::AckFeedback);
	}
	
	// extensions
	bool ext_deflate, ext_chat, ext_palette;
	
	uint8_t getExtensions() const throw()
	{
		return (ext_deflate?(protocol::extensions::Deflate):0)
			+ (ext_chat?(protocol::extensions::Chat):0)
			+ (ext_palette?(protocol::extensions::Palette):0);
	}
	
	void setExtensions(const uint8_t flags) throw()
	{
		ext_deflate = fIsSet(flags, protocol::extensions::Deflate);
		ext_chat = fIsSet(flags, protocol::extensions::Chat);
		ext_palette = fIsSet(flags, protocol::extensions::Palette);
	}
	
	uint8_t
		// active layer in current session
		a_layer,
		// locked to this layer in current session
		a_layer_lock;
	
	// user mode
	bool a_locked, a_muted, a_deaf;
	
	uint8_t getAMode() const throw()
	{
		return (a_locked?(protocol::user_mode::Locked):0)
			+ (a_muted?(protocol::user_mode::Mute):0)
			+ (a_deaf?(protocol::user_mode::Deaf):0);
	}
	
	void setAMode(const uint8_t flags) throw()
	{
		a_locked = fIsSet(flags, protocol::user_mode::Locked);
		a_muted = fIsSet(flags, protocol::user_mode::Mute);
		a_deaf = fIsSet(flags, protocol::user_mode::Deaf);
	}
	
	// Subscribed sessions
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<uint8_t, SessionData*> sessions;
	#else
	std::map<uint8_t, SessionData*> sessions;
	#endif
	
	// Output queue
	std::deque<message_ref> queue;
	
	// Input/output buffer
	Buffer input, output;
	
	// Currently incoming message.
	protocol::Message *inMsg;
	
	// Feature level used by client
	uint16_t level;
	
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

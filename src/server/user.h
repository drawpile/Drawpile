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

#include "common.h"
#include "buffer.h" // Buffer
#include "message.h" // message_ref
#include "ev/event.h"

struct Session; // defined elsewhere
#ifndef ServerSession_INCLUDED
	#include "session.h"
#endif

#include "../shared/protocol.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.flags.h"

class Socket; // defined elsewhere

#ifndef NDEBUG
	#include <iostream>
#endif

#include <deque>

struct SessionData; // forward declaration

/* iterators */
#include <map>
typedef std::map<uint8_t, SessionData*>::iterator usr_session_i;
typedef std::map<uint8_t, SessionData*>::const_iterator usr_session_const_i;

typedef std::deque<message_ref>::iterator usr_message_i;
typedef std::deque<message_ref>::const_iterator usr_message_const_i;

// User session data
struct SessionData
{
	SessionData(Session *s) throw()
		: session(s),
		layer(protocol::null_layer),
		layer_lock(protocol::null_layer),
		locked(fIsSet(s->mode, static_cast<uint8_t>(protocol::user_mode::Locked))),
		muted(fIsSet(s->mode, static_cast<uint8_t>(protocol::user_mode::Mute))),
		deaf(fIsSet(s->mode, static_cast<uint8_t>(protocol::user_mode::Deaf))),
		syncWait(false),
		cachedToolInfo(0)
	{
		assert(session != 0);
	}
	
	~SessionData() throw()
	{
		delete cachedToolInfo;
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
		locked = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Locked));
		muted = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Mute));
		deaf = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Deaf));
	}
	
	bool syncWait;
	
	/* cached messages */
	
	protocol::ToolInfo *cachedToolInfo;
};

// User information
struct User
	//: MemoryStack<User>
{
	User(const uint8_t _id, const Socket& nsock) throw()
		: sock(nsock),
		session(0),
		id(_id),
		events(0),
		state(Init),
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
		name_len(0),
		name(0),
		session_data(0),
		strokes(0)
	{
		#if defined(DEBUG_USER) and !defined(NDEBUG)
		std::cout << "User::User(" << static_cast<int>(_id)
			<< ", " << sock.fd() << ")" << std::endl;
		#endif
		assert(_id != protocol::null_user);
	}
	
	~User() throw()
	{
		#if defined(DEBUG_USER) and !defined(NDEBUG)
		std::cout << "User::~User()" << std::endl;
		#endif
		
		delete [] name,
		//delete sock,
		delete inMsg;
		
		for (usr_session_i usi(sessions.begin()); usi != sessions.end(); ++usi)
			delete usi->second;
	}
	
	inline
	bool makeActive(uint8_t session_id) throw()
	{
		SessionData *sdata = getSession(session_id);
		if (sdata != 0)
		{
			if (session != 0)
				session_data->layer = a_layer;
			
			session_data = sdata;
			
			session = session_data->session;
			
			a_layer = session_data->layer;
			a_layer_lock = session_data->layer_lock;
			
			a_locked = session_data->locked;
			//a_deaf = session_data->deaf;
			a_muted = session_data->muted;
			
			return true;
		}
		else
			return false;
	}
	
	SessionData* getSession(uint8_t session_id) throw()
	{
		const usr_session_const_i usi(sessions.find(session_id));
		return (usi == sessions.end() ? 0 : usi->second);
	}
	
	const SessionData* getConstSession(uint8_t session_id) const throw()
	{
		const usr_session_const_i usi(sessions.find(session_id));
		return (usi == sessions.end() ? 0 : usi->second);
	}
	
	inline
	void cacheTool(protocol::ToolInfo* ti) throw(std::bad_alloc)
	{
		assert(ti != session_data->cachedToolInfo); // attempted to re-cache same tool
		assert(ti != 0);
		
		#if defined(DEBUG_USER) and !defined(NDEBUG)
		std::cout << "Caching Tool Info for user #" << static_cast<int>(id) << std::endl;
		#endif
		
		delete session_data->cachedToolInfo;
		session_data->cachedToolInfo = new protocol::ToolInfo(*ti); // use copy-ctor
	}
	
	// Socket
	Socket sock;
	
	// Currently active session
	Session *session;
	
	// User identifier
	uint id;
	
	// Event I/O registered events.
	// EventSystem::ev_t // inaccessible for some reason
	event_type<EventSystem>::ev_t events;
	//int events;
	
	// User state
	enum State
	{
		// When user has just connected
		Init,
		
		// User has been verified to be using correct protocol.
		Verified,
		
		// Waiting for proper user info
		Login,
		
		// Waiting for password
		LoginAuth,
		
		// Normal operation
		Active
	} state;
	
	uint
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
		c_acks = fIsSet(flags, static_cast<uint8_t>(protocol::client::AckFeedback));
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
		ext_deflate = fIsSet(flags, static_cast<uint8_t>(protocol::extensions::Deflate));
		ext_chat = fIsSet(flags, static_cast<uint8_t>(protocol::extensions::Chat));
		ext_palette = fIsSet(flags, static_cast<uint8_t>(protocol::extensions::Palette));
	}
	
	uint
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
		a_locked = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Locked));
		a_muted = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Mute));
		a_deaf = fIsSet(flags, static_cast<uint8_t>(protocol::user_mode::Deaf));
	}
	
	// Subscribed sessions
	std::map<uint8_t, SessionData*> sessions;
	
	// Output queue
	std::deque<message_ref> queue;
	
	// Input/output buffer
	Buffer input, output;
	
	// Currently incoming message.
	protocol::Message *inMsg;
	
	// Feature level used by client
	uint level;
	
	// for storing the password seed associated with this user.
	char seed[4];
	
	// Last touched.
	time_t deadtime;
	
	// Name length
	uint name_len;
	
	// User name
	char* name;
	
	/* cached messages */
	
	SessionData* session_data;
	
	/* counters */
	
	u_long strokes;
};

#endif // ServerUser_INCLUDED

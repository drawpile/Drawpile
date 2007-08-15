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

#include "ev/event.h" // event::~

#include "socket.h" // Socket class
#include "message_ref.h" // message_ref
#include "buffer.h" // Buffer
#include "array.h" // Array<>
#include "types.h" // octet, etc.

#include <deque> // std::deque
#include <map> // std::map

class Socket;
class SessionData;
class Session;

namespace protocol
{
	struct Message;
	struct ToolInfo;
}

//! User/Client information
class User
{
public:
	//! Default constructor
	/**
	 * @param[in] _id User identifier
	 * @param[in] nsock Socket to associate with User
	 */
	User(const octet _id, const Socket& nsock) __attribute__ ((nothrow));
	
	//! Destructor
	~User() __attribute__ ((nothrow));
	
	//! Change active session
	/**
	 * @param[in] session_id The session which to activate
	 */
	bool makeActive(octet session_id) __attribute__ ((nothrow));
	
	//! Fetch SessionData* pointer
	/**
	 * @param[in] session_id Which session to fetch
	 */
	SessionData* getSession(octet session_id) __attribute__ ((nothrow,warn_unused_result));
	
	//! Fetch const SessionData* pointer
	/**
	 * @param[in] session_id Which session to fetch
	 */
	const SessionData* getConstSession(octet session_id) const __attribute__ ((nothrow,warn_unused_result));
	
	//! Cache tool info
	/**
	 * @param[in] ti Tool info message to cache
	 *
	 * @throw std::bad_alloc If it can't allocate local copy of the tool info
	 */
	void cacheTool(protocol::ToolInfo* ti);
	
	//! Socket
	Socket sock;
	
	//! Currently active session
	Session *session;
	
	//! User identifier
	uint id;
	
	//! Event I/O : registered events.
	// EventSystem::ev_t // inaccessible for some reason
	event::ev_type<EventSystem>::ev_t events;
	//int events;
	
	//! User state
	enum State
	{
		//! When user has just connected
		Init,
		
		//! User has been verified to be using correct protocol.
		Verified,
		
		//! Waiting for proper user info
		Login,
		
		//! Waiting for password
		LoginAuth,
		
		//! Normal operation
		Active
	} state;
	
	uint
		//! Active layer in session
		layer,
		//! Session we're currently syncing.
		syncing;
	
	//! Is the user server admin?
	bool isAdmin;
	
	//! Client can live with ACKs alone
	bool c_acks;
	
	//! Get client capabilities
	/**
	 * @return Flags for use with the network protocol
	 */
	octet getCapabilities() const __attribute__ ((nothrow,warn_unused_result));
	
	//! Set client capabilities
	/**
	 * @param[in] flags as used in the network protocol
	 */
	void setCapabilities(const octet flags) __attribute__ ((nothrow));
	
	bool
		//! Deflate extension
		ext_deflate,
		//! Chat extension
		ext_chat,
		//! Palette extension
		ext_palette;
	
	//! Get extensions
	/**
	 * @return Flags as used in the network protocol
	 */
	octet getExtensions() const __attribute__ ((nothrow,warn_unused_result));
	
	//! Set extensions
	/**
	 * @param[in] flags as used in the network protocol
	 */
	void setExtensions(const octet flags) __attribute__ ((nothrow));
	
	//! Subscribed sessions
	std::map<octet, SessionData> sessions;
	
	//! Output queue
	std::deque<message_ref> queue;
	
	Buffer
		//! Input buffer
		input,
		//! Output buffer
		output;
	
	//! Currently incoming message.
	protocol::Message *inMsg;
	
	//! Feature level used by client
	uint level;
	
	//! Password seed associated with this user.
	char seed[4];
	
	//! Last touched.
	time_t touched;
	
	//! User name
	Array<char> name;
	
	//! Active session data
	SessionData* session_data;
	
	//! Stroke counter
	u_long strokes;
	
	//! Gets next message in input buffer
	/**
	 * @retval true operation completed
	 * @retval false if inMsg != 0 we need more data.. invalid data otherwise
	 *
	 * @throw std::bad_alloc thrown indirectly from protocol::getMessage()
	 */
	bool getMessage();
	
	//! 'Flushes' queue to output buffer
	/**
	 * @throw std::bad_alloc thrown indirectly from Buffer
	 */
	uint flushQueue();
};

#endif // ServerUser_INCLUDED

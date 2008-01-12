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

#pragma once

#ifndef ServerUser_INCLUDED
#define ServerUser_INCLUDED

#include "ev/event.h" // event::~

#include "common.h"
#include "socket.h" // Socket class
#include "buffer.h" // Buffer
#include "array.h" // Array<>
#include "types.h" // octet, etc.

#include <deque> // std::deque
#include <map> // std::map
#include <list> // std::list

#include "shared/protocol.fwd.h"

//! User/Client information
class User
{
public:
	//! Default constructor
	/**
	 * @param[in] _id User identifier
	 * @param[in] nsock Socket to associate with User
	 */
	User(octet _id, const Socket& nsock) NOTHROW;
	
	//! Destructor
	~User() NOTHROW;
	
	//! Change active session
	/**
	 * @param[in] session_id The session which to activate
	 */
	bool makeActive(octet session_id) NOTHROW;
	
	//! Fetch SessionData* pointer
	/**
	 * @param[in] session_id Which session to fetch
	 */
	SessionData* getSession(octet session_id) NOTHROW;
	
	//! Fetch const SessionData* pointer
	/**
	 * @param[in] session_id Which session to fetch
	 */
	const SessionData* getSession(octet session_id) const NOTHROW;
	
	//! Cache tool info
	/**
	 * @param[in] ti Tool info message to cache
	 *
	 * @throw std::bad_alloc If it can't allocate local copy of the tool info
	 */
	void cacheTool(protocol::ToolInfo* ti) NONNULL(1);
	
	//! Socket
	Socket sock;
	
	//! Currently active session
	Session *session;
	
	//! User identifier
	uint id;
	
	//! Event I/O : registered events.
	// EventSystem::ev_t // inaccessible for some reason
	EventSystem::ev_t events;
	//int events;
	
	//! User state
	enum State
	{
		//! When user has just connected
		Init=3,
		
		//! Waiting for proper user info
		Login=1,
		
		//! Waiting for password
		LoginAuth=2,
		
		//! Normal operation
		Active=0
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
	octet getCapabilities() const NOTHROW;
	
	//! Set client capabilities
	/**
	 * @param[in] flags as used in the network protocol
	 */
	void setCapabilities(octet flags) NOTHROW;
	
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
	octet getExtensions() const NOTHROW;
	
	//! Set extensions
	/**
	 * @param[in] flags as used in the network protocol
	 */
	void setExtensions(octet flags) NOTHROW;
	
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
	uint32_t seed;
	
	//! User dead time.
	time_t deadtime;
	
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
	 * @note May scrap current output buffer so don't call unless the buffer is empty.
	 * @throw std::bad_alloc thrown indirectly from Buffer or serialize()
	 */
	uint flushQueue();
	
	//! Delete and null inMsg
	void freeMsg() NOTHROW;
	
	//! Raster targets
	std::list<User*> targets;
	//! Raster source
	User *source;
};

#endif // ServerUser_INCLUDED

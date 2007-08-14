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

#ifndef Server_Class_Included
#define Server_Class_Included

class User;
class Session;
class Buffer;

#include "ev/event.h" // EventSystem
#include "socket.h" // Socket class
#include "array.h" // Array<>
#include "message_ref.h" // message_ref

#include "../shared/protocol.h" // protocol::UserInfo::reason
#include "types.h"
#include "socket.types.h" // fd_t
#include "statistics.h" // Statistics struct

#include <ctime> // time_t, time(0)
#include <map> // std::map
#include <list> // std::list

//! Server defaults
namespace srv_defaults {

//! Time limit before culling user
const uint time_limit = 180;

}

//! Server
class Server
{
protected:
	/* data */
	
	Statistics stats;
	
	//! Server state
	enum State {
		Init,
		Active,
		Exiting,
		Dead,
		Error
	} state;
	
	//! Event mechanism
	EventSystem ev;
	
	bool
		//! User identifiers
		user_ids[254],
		//! Session identifiers
		session_ids[254];
	
	//! FD to user mapping
	std::map<fd_t, User*> users;
	
	//! Session ID to session mapping
	std::map<octet, Session*> sessions;
	
	//! Fake tunnel between two users. Only used for passing raster, for now.
	/** source_fd -> target_fd */
	std::multimap<User*, User*> tunnel;
	
	//! Users eligible for culling
	std::list<User*> utimer;
	
	//! Listening socket
	Socket lsock;
	
	Array<char>
		//! Server password
		password,
		//! Admin password
		admin_password;
	
	size_t
		//! User limit
		user_limit,
		//! Session limit
		session_limit,
		//! User session subscription limit
		max_subscriptions,
		//! Name length limit
		name_len_limit;
	
	time_t
		//! Wait time before culling
		time_limit,
		//! Substitute for time()
		current_time,
		//! The next time we cull idlers
		next_timer;
	
	uint16_t
		//! Minimum canvas dimension
		min_dimension;
	
	octet
		//! Supported extensions
		extensions;
	
	bool
		//! Enforce unique user and session names
		enforceUnique,
		//! UTF-16 instead of UTF-8
		wideStrings,
		//! Disallow global chatting
		noGlobalChat,
		//! Deflate extension
		extDeflate,
		//! Palette extension
		extPalette,
		//! Chat extension
		extChat;
	
	//! Default user mode
	octet default_user_mode;
	
	bool
		//! Shutdown server once all users have left
		Transient,
		//! Auto-promote localhost connections to Admin status
		LocalhostAdmin,
		//! Block duplicate connections from same source IP
		blockDuplicateConnections;
	
	/* functions */
	
	//! Frees user ID
	/**
	 * @param[in] id User ID to free
	 */
	void freeUserID(const octet id) __attribute__ ((nothrow));
	
	//! Frees session ID
	/**
	 * @param[in] id Session ID to free
	 */
	void freeSessionID(const octet id) __attribute__ ((nothrow));
	
	//! Get free user ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getUserID() __attribute__ ((nothrow,warn_unused_result));
	
	//! Get free session ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getSessionID() __attribute__ ((nothrow,warn_unused_result));
	
	/* *** Generate messages *** */
	
	//! Generate host info message
	/**
	 * @return shared_ptr to HostInfo message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgHostInfo() const;
	
	//! Generate password request message
	/**
	 * @param[in] usr User to whom the message is for
	 * @param[in] session Session ID to which this request pertains to
	 *
	 * @return shared_ptr to Authentication message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgPWRequest(User& usr, const octet session) const;
	
	//! Generate user event message
	/**
	 * @param[in] usr User to whom the message refers to
	 * @param[in] session_id Session ID to which the message pertains to
	 * @param[in] event protocol::UserInfo::uevent
	 *
	 * @return shared_ptr to UserEvent message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgUserEvent(const User& usr, const octet session_id, const octet event) const;
	
	//! Generate error message
	/**
	 * @param[in] session Session ID to which the message refers to
	 * @param[in] errorCode protocol::error
	 *
	 * @return shared_ptr to Error message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgError(const octet session, const uint16_t errorCode) const;
	
	//! Generate ACK message
	/**
	 * @param[in] session Session ID to which the ACK refers to
	 * @param[in] msgtype Message type (protocol::Message::msgtype) to which the ACK refers to
	 *
	 * @return shared_ptr to Acknowledgement message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgAck(const octet session, const octet msgtype) const;
	
	//! Generate sync-wait message
	/**
	 * @param[in] session_id Session ID for the session which is to enter Sync-Wait state
	 *
	 * @return shared_ptr to SyncWait message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgSyncWait(const octet session_id) const;
	
	//! Generate session info message
	/**
	 * @param[in] session Source Session reference from which to gather required information
	 *
	 * @return shared_ptr to SessionInfo message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgSessionInfo(const Session& session) const;
	
	/* *** Something else *** */
	
	//! Write to user socket
	/**
	 * @param[in,out] usr Target User to write data to
	 */
	void uWrite(User*& usr);
	
	//! Read from user socket
	/**
	 * @param[in,out] usr Source User to read data from
	 *
	 * @throw std::bad_alloc
	 */
	void uRead(User*& usr);
	
	//! Process all read data.
	/**
	 * @param[in,out] usr User whose input buffer to process
	 */
	void uProcessData(User*& usr);
	
	//! Process stroke info, stroke end and tool info
	/**
	 * @param[in] usr User whose drawing commands to process
	 */
	void uHandleDrawing(User& usr);
	
	//! Handle received password
	/**
	 * @param[in] usr User whose password to handle
	 */
	void uHandlePassword(User& usr) __attribute__ ((nothrow));
	
	//! Handle user message.
	/**
	 * @param[in,out] usr User whose message to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleMsg(User*& usr);
	
	//! Handle ACKs
	/**
	 * @param[in,out] usr User whose ACK message to handle
	 */
	void uHandleAck(User*& usr) __attribute__ ((nothrow));
	
	//! Forward raster to those expecting it.
	/**
	 * @param[in] usr Source User for raster
	 */
	void uTunnelRaster(User& usr);
	
	//! Handle SessionEvent message
	/**
	 * @param[in,out] session Session for which to handle the event for
	 * @param[in,out] usr User whose event is in question
	 */
	void uSessionEvent(Session*& session, User*& usr) __attribute__ ((nothrow));
	
	//! Handle instruction message
	/**
	 * @param[in,out] usr User whose session instruction to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uSessionInstruction(User*& usr);
	
	//! Set server or session password
	/**
	 * @param[in,out] usr User whose set password instruction to handle
	 */
	void uSetPassword(User*& usr) __attribute__ ((nothrow));
	
	//! Handle user login.
	/**
	 * @param[in,out] usr User whose login to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleLogin(User*& usr);
	
	//! Handle user info in login
	/**
	 * @param[in] usr User whose login user info to handle
	 */
	void uLoginInfo(User& usr) __attribute__ ((nothrow));
	
	#ifdef LAYER_SUPPORT
	//! Handle layer event
	/**
	 * @param[in,out] usr User whose layer event to handle
	 */
	void uLayerEvent(User*& usr) __attribute__ ((nothrow));
	#endif
	
	//! Send message to all users in session
	/**
	 * @param[in] session Target session for propagation
	 * @param[in] msg Shared_ptr of the message to be propagated
	 * @param[in] source (Optional) Source User to whom the message will NOT be sent to
	 */
	void Propagate(const Session& session, message_ref msg, User* source=0) __attribute__ ((nothrow));
	
	//! Check password hash
	/**
	 * @param[in] hashdigest Digest to compare against
	 * @param[in] str String for hashing
	 * @param[in] len String length
	 * @param[in] seed Password seed
	 *
	 * @retval true if the digests match
	 * @retval false otherwise
	 */
	bool CheckPassword(const char *hashdigest, const char *str, const size_t len, const char seed[4]) __attribute__ ((nothrow,warn_unused_result));
	
	//! Queue message to user
	/**
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 *
	 * @param[in] usr User to whom to queue the message to
	 * @param[in] msg Shared_ptr to message to queue
	 */
	void uQueueMsg(User& usr, message_ref msg) __attribute__ ((nothrow));
	
	//! Begin synchronizing the session
	/**
	 * @param[in] session Session to synchronize
	 *
	 * @throw std::bad_alloc
	 */
	void SyncSession(Session* session);
	
	//! Break synchronization with user.
	/**
	 * @param[in] usr Target User
	 */
	void breakSync(User& usr) __attribute__ ((nothrow));
	
	//! Attach user to session and begin user synchronization if necessary
	/**
	 * @param[in] usr User who's joining the session
	 * @param[in] session Target session
	 *
	 * @throw std::bad_alloc
	 */
	void uJoinSession(User& usr, Session& session);
	
	//! Needs session reference because it might get destroyed.
	/**
	 * @param[in] usr User who's leaving
	 * @param[in,out] session Session to leave from
	 * @param[in] reason (Optional) Reason for leaving session (protocol::UserInfo::uevent)
	 *
	 * @throw std::bad_alloc
	 */
	void uLeaveSession(User& usr, Session*& session, const protocol::UserInfo::uevent reason=protocol::UserInfo::Leave);
	
	//! Adds user
	/**
	 * @throw std::bad_alloc
	 */
	void uAdd();
	
	//! Removes user and does cleaning..
	/**
	 * @param[in,out] usr User who's being removed
	 * @param[in] reason Reason for removal (protocol::UserInfo::uevent)
	 */
	void uRemove(User*& usr, const protocol::UserInfo::uevent reason) __attribute__ ((nothrow));
	
	//! Delete session and do some cleaning
	/**
	 * @param[in,out] session Session to delete
	 */
	void sRemove(Session*& session) __attribute__ ((nothrow));
	
	//! Check user name uniqueness
	/**
	 * @param[in] usr User whose name to check
	 */
	bool validateUserName(User* usr) const __attribute__ ((nothrow,warn_unused_result));
	
	//! Check session title uniqueness
	/**
	 * @param[in] title Session title to check
	 */
	bool validateSessionTitle(const Array<char>& title) const __attribute__ ((nothrow,warn_unused_result));
	
	#if defined(HAVE_ZLIB)
	//! Reprocesses deflated data stream
	/**
	 * @param[in,out] usr User whose input to reprocess
	 *
	 * @throw std::bad_alloc
	 */
	void UncompressAndReprocess(User*& usr);
	
	//! Deflate (compress) outgoing data
	/**
	 * @param[in,out] buffer 
	 *
	 * @throw std::bad_alloc
	 */
	void Compress(Buffer& buffer);
	#endif
	
	//! Cull idle users
	void cullIdlers();
	
	//! Regenerate password seed
	/**
	 * @param[in] usr User whose password seed to regen
	 */
	void uRegenSeed(User& usr) const __attribute__ ((nothrow));
	
	//! Check if user is owner of session
	/**
	 * @param[in] usr User to test
	 * @param[in] session Target Session
	 */
	bool isOwner(const User& usr, const Session& session) const __attribute__ ((nothrow,warn_unused_result));
	
	//! Get Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 *
	 * @retval NULL if no session is found
	 */
	Session* getSession(const octet session_id) __attribute__ ((nothrow,warn_unused_result));
	
	//! Get const Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 *
	 * @retval NULL if no session was found
	 */
	const Session* getConstSession(const octet session_id) const __attribute__ ((nothrow,warn_unused_result));
	
public:
	//! Constructor
	Server() __attribute__ ((nothrow));
	
	#if 0
	//! Destructor
	virtual ~Server() __attribute__ ((nothrow));
	#endif
	
	//! Initializes anything that need to be done so.
	/**
	 * @retval false on error
	 * @retval true otherwise
	 *
	 * @throw std::bad_alloc
	 */
	bool init();
	
	/** set behaviour **/
	
	//! Set name length limit (default: 8)
	/**
	 * @param[in] limit New limit
	 */
	void setNameLengthLimit(const octet limit=16) __attribute__ ((nothrow));
	uint getNameLengthLimit() const __attribute__ ((nothrow));
	
	//! Set server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setPassword(char* pwstr=0, const octet len=0) __attribute__ ((nothrow));
	bool haveServerPassword() const __attribute__ ((nothrow));
	
	//! Set admin server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setAdminPassword(char* pwstr=0, const octet len=0) __attribute__ ((nothrow));
	bool haveAdminPassword() const __attribute__ ((nothrow));
	
	//! Set user limit
	/**
	 * @param[in] ulimit New limit
	 */
	void setUserLimit(const octet ulimit=10) __attribute__ ((nothrow));
	uint getUserLimit() const __attribute__ ((nothrow));
	
	//! Set listening port
	/**
	 * @param[in] _port port to bind to
	 */
	void setPort(const ushort _port=27750) __attribute__ ((nothrow));
	ushort getPort() const __attribute__ ((nothrow));
	
	//! Set operation mode
	/**
	 * @param[in] _enable transient mode
	 */
	void setTransient(const bool _enable=true) __attribute__ ((nothrow));
	
	//! Set auto-localhost admin promotion
	/**
	 * @param _enable auto-promotion
	 */
	void setLocalhostAdmin(const bool _enable=true) __attribute__ ((nothrow));
	
	//! Get requirement flags
	/**
	 * @see protocol::requirements
	 */
	octet getRequirements() const __attribute__ ((nothrow));
	
	//! Get active extensions
	/**
	 * @see protocol::extensions
	 */
	octet getExtensions() const __attribute__ ((nothrow));
	
	//! Set unique name enforcing
	void setUniqueNameEnforcing(bool _enabled=true) __attribute__ ((nothrow));
	//! Get unique name enforcing
	bool getUniqueNameEnforcing() const __attribute__ ((nothrow));
	
	//! Set minimum board dimension (width or height)
	/**
	 * @param[in] mindim Minimum dimension in pixels
	 */
	void setMinDimension(const uint16_t mindim=400) __attribute__ ((nothrow));
	uint getMinDimension() const __attribute__ ((nothrow));
	
	//! Set UTF-16 requirement
	/**
	 * @param[in] _enabled Enable UTF-16 requirement
	 */
	void setUTF16Requirement(const bool _enabled=true) __attribute__ ((nothrow));
	bool getUTF16Requirement() const __attribute__ ((nothrow));
	
	//! Set default user mode
	/**
	 * @param[in] _mode User mode flags
	 */
	void setUserMode(const octet _mode=0) __attribute__ ((nothrow));
	uint getUserMode() const __attribute__ ((nothrow));
	
	//! Set session limit on server
	/**
	 * @param[in] _limit Session limit
	 */
	void setSessionLimit(const octet _limit=1) __attribute__ ((nothrow));
	uint getSessionLimit() const __attribute__ ((nothrow));
	
	//! Set per user subscription limit
	/**
	 * @param[in] _slimit Subscrption limit
	 */
	void setSubscriptionLimit(const octet _slimit=1) __attribute__ ((nothrow));
	uint getSubscriptionLimit() const __attribute__ ((nothrow));
	
	//! Allow/disallow duplicate connections from same address
	/**
	 * @param[in] _block duplicate connections
	 * @todo Needs shorter name
	 */
	void setDuplicateConnectionBlocking(const bool _block=true) __attribute__ ((nothrow));
	//! Get current duplicate connection blocking state
	bool getDuplicateConnectionBlocking() const __attribute__ ((nothrow));
	
	/** Control functions **/
	
	//! Enter main loop
	int run();
	
	//! Set server state to 'Exiting'
	void stop() __attribute__ ((nothrow));
	
	//! Clean-up users, sessions and anything else except config.
	void reset() __attribute__ ((nothrow));
	
	/** Status and information retrieval **/
	
	Statistics getStats() const __attribute__ ((nothrow,warn_unused_result));
	
	#if 0
private:
	virtual void eventNotify() const __attribute__ ((nothrow));
	#endif
}; // class Server

#endif // Server_Class_Included

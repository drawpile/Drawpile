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

struct User;
struct Session;
struct Buffer;

#include "ev/event.h" // EventSystem
#include "socket.h" // Socket class
#include "array.h" // Array<>
#include "message_ref.h" // message_ref

#include "../shared/protocol.h" // protocol::UserInfo::reason
#include "types.h"
#include "socket.types.h" // fd_t

#include <ctime> // time_t, time(0)
#include <map> // std::map
#include <set> // std::set

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
	std::set<User*> utimer;
	
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
	void freeUserID(const octet id) throw();
	
	//! Frees session ID
	/**
	 * @param[in] id Session ID to free
	 */
	void freeSessionID(const octet id) throw();
	
	//! Get free user ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getUserID() throw();
	
	//! Get free session ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getSessionID() throw();
	
	/* *** Generate messages *** */
	
	//! Generate host info message
	/**
	 * @return shared_ptr to HostInfo message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgHostInfo() const throw(std::bad_alloc);
	
	//! Generate password request message
	/**
	 * @param[in] usr User to whom the message is for
	 * @param[in] session Session ID to which this request pertains to
	 *
	 * @return shared_ptr to Authentication message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgPWRequest(User& usr, const octet session) const throw(std::bad_alloc);
	
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
	message_ref msgUserEvent(const User& usr, const octet session_id, const octet event) const throw(std::bad_alloc);
	
	//! Generate error message
	/**
	 * @param[in] session Session ID to which the message refers to
	 * @param[in] errorCode protocol::error
	 *
	 * @return shared_ptr to Error message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgError(const octet session, const uint16_t errorCode) const throw(std::bad_alloc);
	
	//! Generate ACK message
	/**
	 * @param[in] session Session ID to which the ACK refers to
	 * @param[in] msgtype Message type (protocol::Message::msgtype) to which the ACK refers to
	 *
	 * @return shared_ptr to Acknowledgement message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgAck(const octet session, const octet msgtype) const throw(std::bad_alloc);
	
	//! Generate sync-wait message
	/**
	 * @param[in] session_id Session ID for the session which is to enter Sync-Wait state
	 *
	 * @return shared_ptr to SyncWait message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgSyncWait(const octet session_id) const throw(std::bad_alloc);
	
	//! Generate session info message
	/**
	 * @param[in] session Source Session reference from which to gather required information
	 *
	 * @return shared_ptr to SessionInfo message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgSessionInfo(const Session& session) const throw(std::bad_alloc);
	
	/* *** Something else *** */
	
	//! Write to user socket
	/**
	 * @param[in,out] usr Target User to write data to
	 */
	void uWrite(User*& usr) throw();
	
	//! Read from user socket
	/**
	 * @param[in,out] usr Source User to read data from
	 *
	 * @throw std::bad_alloc
	 */
	void uRead(User*& usr) throw(std::bad_alloc);
	
	//! Process all read data.
	/**
	 * @param[in,out] usr User whose input buffer to process
	 */
	void uProcessData(User*& usr) throw();
	
	//! Process stroke info, stroke end and tool info
	/**
	 * @param[in] usr User whose drawing commands to process
	 */
	void uHandleDrawing(User& usr) throw();
	
	//! Handle received password
	/**
	 * @param[in] usr User whose password to handle
	 */
	void uHandlePassword(User& usr) throw();
	
	//! Handle user message.
	/**
	 * @param[in,out] usr User whose message to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleMsg(User*& usr) throw(std::bad_alloc);
	
	//! Handle ACKs
	/**
	 * @param[in,out] usr User whose ACK message to handle
	 */
	void uHandleAck(User*& usr) throw();
	
	//! Forward raster to those expecting it.
	/**
	 * @param[in] usr Source User for raster
	 */
	void uTunnelRaster(User& usr) throw();
	
	//! Handle SessionEvent message
	/**
	 * @param[in,out] session Session for which to handle the event for
	 * @param[in,out] usr User whose event is in question
	 */
	void uSessionEvent(Session*& session, User*& usr) throw();
	
	//! Handle instruction message
	/**
	 * @param[in,out] usr User whose session instruction to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uSessionInstruction(User*& usr) throw(std::bad_alloc);
	
	//! Set server or session password
	/**
	 * @param[in,out] usr User whose set password instruction to handle
	 */
	void uSetPassword(User*& usr) throw();
	
	//! Handle user login.
	/**
	 * @param[in,out] usr User whose login to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleLogin(User*& usr) throw(std::bad_alloc);
	
	//! Handle user info in login
	/**
	 * @param[in] usr User whose login user info to handle
	 */
	void uLoginInfo(User& usr) throw();
	
	//! Handle layer event
	/**
	 * @param[in,out] usr User whose layer event to handle
	 */
	void uLayerEvent(User*& usr) throw();
	
	//! Send message to all users in session
	/**
	 * @param[in] session Target session for propagation
	 * @param[in] msg Shared_ptr of the message to be propagated
	 * @param[in] source (Optional) Source User to whom the message will NOT be sent to
	 */
	void Propagate(const Session& session, message_ref msg, User* source=0) throw();
	
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
	bool CheckPassword(const char *hashdigest, const char *str, const size_t len, const char *seed) throw();
	
	//! Queue message to user
	/**
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 *
	 * @param[in] usr User to whom to queue the message to
	 * @param[in] msg Shared_ptr to message to queue
	 */
	void uQueueMsg(User& usr, message_ref msg) throw();
	
	//! Begin synchronizing the session
	/**
	 * @param[in] session Session to synchronize
	 */
	void SyncSession(Session* session) throw();
	
	//! Break synchronization with user.
	/**
	 * @param[in] usr Target User
	 */
	void breakSync(User& usr) throw();
	
	//! Attach user to session and begin user synchronization if necessary
	/**
	 * @param[in] usr User who's joining the session
	 * @param[in] session Target session
	 */
	void uJoinSession(User& usr, Session& session) throw();
	
	//! Needs session reference because it might get destroyed.
	/**
	 * @param[in] usr User who's leaving
	 * @param[in,out] session Session to leave from
	 * @param[in] reason (Optional) Reason for leaving session (protocol::UserInfo::uevent)
	 */
	void uLeaveSession(User& usr, Session*& session, const protocol::UserInfo::uevent reason=protocol::UserInfo::Leave) throw();
	
	//! Adds user
	/**
	 * @param[in] sock Socket for the new User
	 *
	 * @throw std::bad_alloc
	 */
	void uAdd(Socket sock) throw(std::bad_alloc);
	
	//! Removes user and does cleaning..
	/**
	 * @param[in,out] usr User who's being removed
	 * @param[in] reason Reason for removal (protocol::UserInfo::uevent)
	 */
	void uRemove(User*& usr, const protocol::UserInfo::uevent reason) throw();
	
	//! Delete session and do some cleaning
	/**
	 * @param[in,out] session Session to delete
	 */
	void sRemove(Session*& session) throw();
	
	//! Check user name uniqueness
	/**
	 * @param[in] usr User whose name to check
	 */
	bool validateUserName(User* usr) const throw();
	
	//! Check session title uniqueness
	/**
	 * @param[in] title Session title to check
	 */
	bool validateSessionTitle(const Array<char>& title) const throw();
	
	//! Reprocesses deflated data stream
	/**
	 * @param[in,out] usr User whose input to reprocess
	 *
	 * @throw std::bad_alloc
	 */
	void DeflateReprocess(User*& usr) throw(std::bad_alloc);
	
	//! Deflate outgoing data
	/**
	 * @param[in,out] buffer 
	 * @param[in] len
	 *
	 * @throw std::bad_alloc
	 */
	void Deflate(Buffer& buffer, size_t len) throw(std::bad_alloc);
	
	//! Cull idle users
	void cullIdlers() throw();
	
	//! Regenerate password seed
	/**
	 * @param[in] usr User whose password seed to regen
	 */
	void uRegenSeed(User& usr) const throw();
	
	//! Check if user is owner of session
	/**
	 * @param[in] usr User to test
	 * @param[in] session Target Session
	 */
	bool isOwner(const User& usr, const Session& session) const throw();
	
	//! Get Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 *
	 * @retval NULL if no session is found
	 */
	Session* getSession(const octet session_id) throw();
	
	//! Get const Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 *
	 * @retval NULL if no session was found
	 */
	const Session* getConstSession(const octet session_id) const throw();
	
public:
	//! Constructor
	Server() throw();
	
	#if 0
	//! Destructor
	virtual ~Server() throw();
	#endif
	
	//! Initializes anything that need to be done so.
	/**
	 * @retval false on error
	 * @retval true otherwise
	 *
	 * @throw std::bad_alloc
	 */
	bool init() throw(std::bad_alloc);
	
	/** set behaviour **/
	
	//! Set name length limit (default: 8)
	/**
	 * @param[in] limit New limit
	 */
	void setNameLengthLimit(const octet limit=16) throw() { name_len_limit = limit; }
	uint getNameLengthLimit() const throw() { return name_len_limit; }
	
	//! Set server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setPassword(char* pwstr=0, const octet len=0) throw()
	{
		password.set(pwstr, len);
	}
	bool haveServerPassword() const throw() { return (password.ptr!=0); }
	
	//! Set admin server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setAdminPassword(char* pwstr=0, const octet len=0) throw()
	{
		admin_password.set(pwstr, len);
	}
	bool haveAdminPassword() const throw() { return (admin_password.ptr!=0); }
	
	//! Set user limit
	/**
	 * @param[in] ulimit New limit
	 */
	void setUserLimit(const octet ulimit=10) throw() { user_limit = ulimit; }
	uint getUserLimit() const throw() { return user_limit; }
	
	//! Set listening port
	/**
	 * @param[in] _port port to bind to
	 */
	void setPort(const ushort _port=27750) throw() { lsock.getAddr().port(_port); }
	ushort getPort() const throw() { return lsock.port(); }
	
	//! Set operation mode
	/**
	 * @param[in] _enable transient mode
	 */
	void setTransient(const bool _enable=true) throw() { Transient = _enable; }
	
	//! Set auto-localhost admin promotion
	/**
	 * @param _enable auto-promotion
	 */
	void setLocalhostAdmin(const bool _enable=true) throw() { LocalhostAdmin = _enable; }
	
	//! Get requirement flags
	/**
	 * @see protocol::requirements
	 */
	octet getRequirements() const throw();
	
	void setUniqueNameEnforcing(bool _enabled=true) throw() { enforceUnique = _enabled; }
	bool getUniqueNameEnforcing() const throw() { return enforceUnique; }
	
	//! Set minimum board dimension (width or height)
	/**
	 * @param[in] mindim Minimum dimension in pixels
	 */
	void setMinDimension(const uint16_t mindim=400) throw() { min_dimension = mindim; }
	uint getMinDimension() const throw() { return min_dimension; }
	
	//! Set UTF-16 requirement
	/**
	 * @param[in] _enabled Enable UTF-16 requirement
	 */
	void setUTF16Requirement(const bool _enabled=true) throw() { wideStrings = _enabled; }
	bool getUTF16Requirement() const throw() { return wideStrings; }
	
	//! Set default user mode
	/**
	 * @param[in] _mode User mode flags
	 */
	void setUserMode(const octet _mode=0) throw() { default_user_mode = _mode; }
	uint getUserMode() const throw() { return default_user_mode; }
	
	//! Set session limit on server
	/**
	 * @param[in] _limit Session limit
	 */
	void setSessionLimit(const octet _limit=1) throw() { session_limit = _limit; }
	uint getSessionLimit() const throw() { return session_limit; }
	
	//! Set per user subscription limit
	/**
	 * @param[in] _slimit Subscrption limit
	 */
	void setSubscriptionLimit(const octet _slimit=1) throw() { max_subscriptions = _slimit; }
	uint getSubscriptionLimit() const throw() { return max_subscriptions; }
	
	//! Allow/disallow duplicate connections from same address
	/**
	 * @param[in] _block duplicate connections
	 * @todo Needs shorter name
	 */
	void setDuplicateConnectionBlocking(const bool _block=true) throw() { blockDuplicateConnections = _block; }
	//! Get current duplicate connection blocking state
	bool getDuplicateConnectionBlocking() const throw() { return blockDuplicateConnections; }
	
	/** Control functions **/
	
	//! Enter main loop
	int run() throw();
	
	//! Set server state to 'Exiting'
	void stop() throw();
	
	//! Clean-up users, sessions and anything else except config.
	void reset() throw();
	
	/** Status and information retrieval **/
	
	#if 0
private:
	virtual void eventNotify() { }
	#endif
}; // class Server

#endif // Server_Class_Included

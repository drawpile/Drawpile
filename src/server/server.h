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

#pragma once

#ifndef Server_Class_Included
#define Server_Class_Included

#include "config.h"

#include "common.h" // message_ref
#include "ev/event.h" // EventSystem
#include "socket.h" // Socket class
#include "array.h" // Array<>
#include "session.h"
#include "user.h"

#include "../shared/protocol.h" // protocol::UserInfo::reason
#include "types.h"
#include "statistics.h" // Statistics struct

#include <ctime> // time_t, time(0)
#include <map> // std::map
#include <list> // std::list

//! Server
class Server
{
public:
	enum {
	xNoError = 0,
	xLastUserLeft,
	xBindError,
	xSocketError,
	xUnsupportedIPv,
	xEventError,
	xListenFailed,
	xGenericError
	};
protected:
	/* data */
	
	//! Statistics
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
	Event ev;
	
	bool
		//! User identifiers
		user_ids[254],
		//! Session identifiers
		session_ids[254];
	
	//! FD to user mapping
	std::map<SOCKET, User*> users;
	
	//! Session ID to session mapping
	std::map<octet, Session> sessions;
	
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
	
	//! Users eligible for culling
	uint usersInLogin;
	
	/* functions */
	
	//! Frees user ID
	/**
	 * @param[in] id User ID to free
	 */
	void freeUserID(octet id) NOTHROW;
	
	//! Frees session ID
	/**
	 * @param[in] id Session ID to free
	 */
	void freeSessionID(octet id) NOTHROW;
	
	//! Get free user ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getUserID() NOTHROW;
	
	//! Get free session ID
	/**
	 * @retval 0 if all IDs have been exhausted
	 */
	const octet getSessionID() NOTHROW;
	
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
	message_ref msgPWRequest(User& usr, octet session) const;
	
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
	message_ref msgUserEvent(const User& usr, octet session_id, octet event) const;
	
	//! Generate error message
	/**
	 * @param[in] session Session ID to which the message refers to
	 * @param[in] errorCode protocol::error
	 *
	 * @return shared_ptr to Error message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgError(octet session, uint16_t errorCode) const;
	
	//! Generate ACK message
	/**
	 * @param[in] session Session ID to which the ACK refers to
	 * @param[in] msgtype Message type (protocol::Message::msgtype) to which the ACK refers to
	 *
	 * @return shared_ptr to Acknowledgement message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgAck(octet session, octet msgtype) const;
	
	//! Generate sync-wait message
	/**
	 * @param[in] session_id Session ID for the session which is to enter Sync-Wait state
	 *
	 * @return shared_ptr to SyncWait message
	 *
	 * @throw std::bad_alloc
	 */
	message_ref msgSyncWait(octet session_id) const;
	
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
	void uProcessData(User*& usr) NONNULL(1);
	
	//! Process stroke info, stroke end and tool info
	/**
	 * @param[in] usr User whose drawing commands to process
	 */
	void uHandleDrawing(User*& usr) NONNULL(1);
	
	//! Handle received password
	/**
	 * @param[in] usr User whose password to handle
	 */
	void uHandlePassword(User& usr) NOTHROW;
	
	//! Handle user message.
	/**
	 * @param[in,out] usr User whose message to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleMsg(User*& usr) NONNULL(1);
	
	//! Handle ACKs
	/**
	 * @param[in,out] usr User whose ACK message to handle
	 */
	void uHandleAck(User*& usr) NOTHROW NONNULL(1);
	
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
	void uSessionEvent(Session& session, User*& usr) NOTHROW /*NONNULL(2)*/;
	
	//! Handle instruction message
	/**
	 * @param[in,out] usr User whose session instruction to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uSessionInstruction(User*& usr) NONNULL(1);
	
	//! Set server or session password
	/**
	 * @param[in,out] usr User whose set password instruction to handle
	 */
	void uSetPassword(User*& usr) NOTHROW NONNULL(1);
	
	//! Handle user login.
	/**
	 * @param[in,out] usr User whose login to handle
	 *
	 * @throw std::bad_alloc
	 */
	void uHandleLogin(User*& usr) NONNULL(1);
	
	//! Handle user info in login
	/**
	 * @param[in] usr User whose login user info to handle
	 */
	void uLoginInfo(User& usr) NOTHROW;
	
	#ifdef LAYER_SUPPORT
	//! Handle layer event
	/**
	 * @param[in,out] usr User whose layer event to handle
	 */
	void uLayerEvent(User*& usr) NOTHROW NONNULL(1);
	#endif
	
	//! Handle layer select message
	void uSelectLayer(User& usr) NOTHROW;
	
	//! Send message to all users in session
	/**
	 * @param[in] session Target session for propagation
	 * @param[in] msg Shared_ptr of the message to be propagated
	 * @param[in] source (Optional) Source User to whom the message will NOT be sent to
	 */
	void Propagate(const Session& session, message_ref msg, User* source=0) NOTHROW;
	
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
	bool CheckPassword(const char *hashdigest, const char *str, size_t len, uint seed) NOTHROW NONNULL(1) NONNULL(2);
	
	//! Queue message to user
	/**
	 * Appends the message to user's output buffer,
	 * and manipulates event system.
	 *
	 * @param[in] usr User to whom to queue the message to
	 * @param[in] msg Shared_ptr to message to queue
	 */
	void uQueueMsg(User& usr, message_ref msg) NOTHROW;
	
	//! Begin synchronizing the session
	/**
	 * @param[in] session Session to synchronize
	 *
	 * @throw std::bad_alloc
	 */
	void SyncSession(Session& session);
	
	//! Break synchronization with user.
	/**
	 * @param[in] usr Target User
	 */
	void breakSync(User& usr) NOTHROW;
	
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
	void uLeaveSession(User& usr, Session& session, protocol::UserInfo::uevent reason=protocol::UserInfo::Leave);
	
	//! Adds user
	/**
	 * @throw std::bad_alloc
	 */
	void uAdd();
	
	//! Deletes user
	/**
	 * @param[in,out] usr User who's being removed
	 * @param[in] reason Reason for removal (protocol::UserInfo::uevent)
	 */
	void removeUser(User*& usr, protocol::UserInfo::uevent reason) NOTHROW NONNULL(1);
	
	//! Delete session and do some cleaning
	/**
	 * @param[in,out] session Session to delete
	 */
	void removeSession(Session& session) NOTHROW;
	
	//! Check user name uniqueness
	/**
	 * @param[in] usr User whose name to check
	 */
	bool validateUserName(User& usr) const NOTHROW;
	
	//! Check session title uniqueness
	/**
	 * @param[in] title Session title to check
	 */
	bool validateSessionTitle(const Array<char>& title) const NOTHROW;
	
	#if defined(HAVE_ZLIB)
	//! Reprocesses deflated data stream
	/**
	 * @param[in,out] usr User whose input to reprocess
	 *
	 * @throw std::bad_alloc
	 */
	void UncompressAndReprocess(User*& usr) NONNULL(1);
	
	//! Deflate (compress) outgoing data
	/**
	 * @param[in,out] buffer 
	 *
	 * @throw std::bad_alloc
	 */
	void Compress(Buffer& buffer);
	#endif
	
	//! Cull idle users
	void cullIdlers() NOTHROW;
	
	//! Regenerate password seed
	/**
	 * @param[in] usr User whose password seed to regen
	 */
	void uRegenSeed(User& usr) const NOTHROW;
	
	//! Get Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 */
	Session* getSession(octet session_id) NOTHROW;
	
	//! Get const Session* pointer
	/**
	 * @param[in] session_id Identifier for the session to find
	 */
	const Session* getSession(octet session_id) const NOTHROW;
	
	//! Get User* pointer
	User* getUser(SOCKET user_handle) NOTHROW;
	
	//! Get const User* pointer
	const User* getUser(SOCKET user_handle) const NOTHROW;
	
	//! Get user by identifier
	User* getUserByID(octet user_id) NOTHROW;
	
	//! Create session
	void createSession(User &usr, protocol::SessionInstruction &msg) NOTHROW;
public:
	//! Constructor
	Server() NOTHROW;
	
	#if 0
	//! Destructor
	virtual ~Server() NOTHROW;
	#endif
	
	//! Initializes anything that need to be done so.
	/**
	 * @retval 0 on success
	 *
	 * @throw std::bad_alloc
	 */
	int init();
	
	/** set behaviour **/
	
	//! Set name length limit (default: 8)
	/**
	 * @param[in] limit New limit
	 */
	void setNameLengthLimit(octet limit=16) NOTHROW;
	uint getNameLengthLimit() const NOTHROW;
	
	//! Set server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setPassword(char* pwstr=0, octet len=0) NOTHROW;
	
	//! Set admin server password
	/**
	 * @param[in] pwstr char* string to use for password
	 * @param[in] len pwstr length in bytes
	 */
	void setAdminPassword(char* pwstr=0, octet len=0) NOTHROW;
	
	//! Set user limit
	/**
	 * @param[in] ulimit New limit
	 */
	void setUserLimit(octet ulimit=10) NOTHROW;
	uint getUserLimit() const NOTHROW;
	
	//! Set listening port
	/**
	 * @param[in] _port port to bind to
	 */
	void setPort(ushort _port=27750) NOTHROW;
	ushort getPort() const NOTHROW;
	
	//! Set operation mode
	/**
	 * @param[in] _enable transient mode
	 */
	void setTransient(bool _enable=true) NOTHROW;
	
	//! Set auto-localhost admin promotion
	/**
	 * @param _enable auto-promotion
	 */
	void setLocalhostAdmin(bool _enable=true) NOTHROW;
	
	//! Get requirement flags
	/**
	 * @see protocol::requirements
	 */
	octet getRequirements() const NOTHROW;
	
	//! Get active extensions
	/**
	 * @see protocol::extensions
	 */
	octet getExtensions() const NOTHROW;
	
	//! Set unique name enforcing
	void setUniqueNameEnforcing(bool _enabled=true) NOTHROW;
	//! Get unique name enforcing
	bool getUniqueNameEnforcing() const NOTHROW;
	
	//! Set minimum board dimension (width or height)
	/**
	 * @param[in] mindim Minimum dimension in pixels
	 */
	void setMinDimension(uint16_t mindim=400) NOTHROW;
	uint getMinDimension() const NOTHROW;
	
	//! Set UTF-16 requirement
	/**
	 * @param[in] _enabled Enable UTF-16 requirement
	 */
	void setUTF16Requirement(bool _enabled=true) NOTHROW;
	bool getUTF16Requirement() const NOTHROW;
	
	//! Set default user mode
	/**
	 * @param[in] _mode User mode flags
	 */
	void setUserMode(octet _mode=0) NOTHROW;
	uint getUserMode() const NOTHROW;
	
	//! Set session limit on server
	/**
	 * @param[in] _limit Session limit
	 */
	void setSessionLimit(octet _limit=1) NOTHROW;
	uint getSessionLimit() const NOTHROW;
	
	//! Set per user subscription limit
	/**
	 * @param[in] _slimit Subscrption limit
	 */
	void setSubscriptionLimit(octet _slimit=1) NOTHROW;
	uint getSubscriptionLimit() const NOTHROW;
	
	//! Allow/disallow duplicate connections from same address
	/**
	 * @param[in] _block duplicate connections
	 * @todo Needs shorter name
	 */
	void setDuplicateConnectionBlocking(bool _block=true) NOTHROW;
	//! Get current duplicate connection blocking state
	bool getDuplicateConnectionBlocking() const NOTHROW;
	
	//! Enable/disable deflate compression
	/**
	 * @note This changes deflate support only if it was compiled in.
	 */
	void setDeflate(bool x) NOTHROW;
	//! Get current deflate compression setting
	bool getDeflate() const NOTHROW;
	
	/** Control functions **/
	
	//! Enter main loop
	int run();
	
	//! Set server state to 'Exiting'
	void stop() NOTHROW;
	
	//! Clean-up users, sessions and anything else except config.
	void reset() NOTHROW;
	
	//! Get statistics
	Statistics getStats() const NOTHROW;
}; // class Server

#endif // Server_Class_Included

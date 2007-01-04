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

#ifndef Protocol_INCLUDED
#define Protocol_INCLUDED

#include <cassert> // assert()
#include <cstddef> // size_t
#include <stdint.h> // [u]int#_t

#include "templates.h"

//#include "memstack.h" // MemoryStack<>

#include "protocol.errors.h"
#include "protocol.defaults.h"
#include "protocol.types.h"
#include "protocol.flags.h"
#include "protocol.tools.h"

//! DrawPile network protocol.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol
 */
namespace protocol
{

//! Implemented protocol revision number.
const uint16_t revision = 7;

//! Base for all message types.
/**
 * Minimum requirements for derived structs is the definitions of _type.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Message_structure
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Message_modifiers
 * @see protocol::type for full list of message types.
 *
 * @param _type message type.
 * @param _flags indicates message modifiers (see protocol::message namespace)
 */
struct Message
{
protected:
	Message(uint8_t _type, uint8_t _flags=message::None) throw()
		: type(_type),
		user_id(protocol::null_user),
		session_id(protocol::Global),
		modifiers(_flags),
		next(0),
		prev(0)
	{ }
	
	// write header (for serialize())
	inline
	size_t serializeHeader(char* ptr, const Message* msg) const throw();
	
	inline
	size_t unserializeHeader(const char* ptr) throw();
	
	inline
	size_t headerSize() const throw();
	
public:
	virtual ~Message() throw() { delete next, delete prev; }
	
	//! Message type identifier (full list in protocol::type namespace).
	const uint8_t type;
	
	uint8_t
		//! Originating user for the message, as assigned by the server.
		user_id,
		//! Target or referred session
		session_id;
	
	const uint8_t
		//! Message modifiers
		modifiers;
	
	Message
		//! Next message (of same type) in queue.
		*next,
		//! Previous message (of same type) in queue.
		*prev;
	
	//! Turn message (list) to char* buffer and set 'len' to the length of the buffer.
	/**
	 * You \b must call this for the \b last message in the list.
	 * 
	 * @param len refers to size_t to which the length of returned buffer is stored.
	 *
	 * @return Buffer ready to be sent to network.
	 *
	 * @throw std::bad_alloc
	 */
	char* serialize(size_t &len) const throw(std::bad_alloc);
	
	//! Get the number of bytes required to serialize the message payload.
	/**
	 * @return payload length in bytes. Defaults to zero payload.
	 */
	virtual
	size_t payloadLength() const throw();
	
	//! Serialize the message payload.
	/**
	 * The buffer must have room for at least payloadLength() bytes.
	 *
	 * @param buf serialized bytes will be stored here.
	 *
	 * @return number of bytes stored. Defaults to zero payload.
	 */
	virtual
	size_t serializePayload(char *buf) const throw();
	
	//! Check if buf contains enough data to unserialize
	/**
	 * The value is not guaranteed to be constant.
	 *
	 * Meaning: the length returned may grow when the function is able to scan
	 * the full extent of the message.
	 *
	 * @param buf points to data buffer waiting for serialization.
	 * @param len declares the length of the data buffer.
	 *
	 * @return length of data needed to unserialize or completely scan the length.
	 * Only unserialize() if you have at least this much data in the buffer at the
	 * time of calling this function. Length includes message type and other
	 * header data. Defaults to zero payload message with possible user modifier.
	 */
	virtual
	size_t reqDataLen(const char* buf, size_t len) const throw();
	
	//! Unserializes char* buffer to associated message struct.
	/**
	 * The base message struct will be the first of linked list if the message is of
	 * bundling sort. Otherwise, there's no linked list created.
	 * 
	 * Before calling unserialize(), you MUST ensure the buffer has enough data
	 * by calling reqDataLen(). Also, reqDataLen() is the only thing that can
	 * tell you how much data the call to unserialize() actually processed or will
	 * process of the provided data buffer.
	 *
	 * @param buf points to data buffer waiting for serialization.
	 * @param len declares the length of the data buffer.
	 *
	 * @return Used buffer length. Should be the same as the value previously returned
	 * by reqDataLen() call. Defaults to zero payload with possible user modifiers.
	 */
	virtual
	size_t unserialize(const char* buf, size_t len) throw(std::exception, std::bad_alloc);
	
	//! Validate the message contents.
	/**
	 * Checks the message contents for any deviations that should not occur.
	 * 
	 * @return 0 if nothing was detected to be wrong.
	 * @return 1 if the payload was invalid
	 * @return 2 if the payload was correct but the header was not.
	 */
	virtual
	int isValid() const throw();
};

//! Protocol identifier.
/**
 * The first message sent by client.
 * If the host can't make heads or tails what this is,
 * it should close the connection.
 *
 * Response: HostInfo
 */
struct Identifier
	: Message//, MemoryStack<Identifier>
{
	Identifier() throw()
		: Message(type::Identifier),
		revision(protocol::null_revision),
		level(protocol::null_implementation),
		flags(client::None),
		extensions(extensions::None)
	{ }
	
	~Identifier() throw() { }
	
	/* unique data */
	
	//! Protocol identifier (see protocol::identifierString).
	char identifier[identifier_size];
	
	uint16_t
		//! Used protocol revision.
		revision,
		//! Client feature implementation level.
		level;
	
	//! Client capabilities (see protocol::client)
	uint8_t flags;
	
	//! Extension flags (see protocol::extensions for full list).
	uint8_t extensions;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Stroke info message.
/**
 * Minimal data required for drawing with the tool provided by ToolInfo.
 *
 * Pressure should be set to 255 for tools that do not provide pressure info.
 *
 * Response: none
 */
struct StrokeInfo
	: Message//, MemoryStack<StrokeInfo>
{
	StrokeInfo() throw()
		: Message(type::StrokeInfo, message::isUser|message::isBundling),
		x(0),
		y(0)
	{ }
	
	~StrokeInfo() throw() { }
	
	/* unique data */
	
	uint16_t
		//! Horizontal (X) coordinate.
		x,
		//! Vertical (Y) coordinate.
		y;
	
	//! Applied pressure.
	uint8_t pressure;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::exception, std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Stroke End message
/**
 * Marks the end of a line defined by StrokeInfo messages.
 */
struct StrokeEnd
	: Message//, MemoryStack<StrokeEnd>
{
	StrokeEnd() throw()
		: Message(type::StrokeEnd,
			message::isUser)
	{ }
	
	~StrokeEnd() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// none needed
};

//! Tool Info message.
/**
 * The selected tool's information and settings.
 * Sent only when it or the active session changes, and just before sending first stroke
 * message for this tool. Contains info for two extremes of tool's settings that are used
 * to create the interpolating brush.
 *
 * Response: none
 */
struct ToolInfo
	: Message//, MemoryStack<ToolInfo>
{
	ToolInfo() throw()
		: Message(type::ToolInfo, message::isUser|message::isSession),
		tool_id(tool_type::None),
		mode(tool_mode::Normal),
		lo_color(0),
		hi_color(0),
		lo_size(0),
		hi_size(0),
		lo_hardness(0),
		hi_hardness(0)
	{ }
	
	~ToolInfo() throw() { }
	
	/* unique data */
	
	//! Tool identifier (for full list, see protocol::tool).
	uint8_t tool_id;
	
	//! Composition mode (tool::mode)
	uint8_t mode;
	
	uint32_t
		//! Lo pressure color (RGBA)
		lo_color,
		//! Hi pressure color (RGBA)
		hi_color;
	
	uint8_t
		//! Lo pressure size.
		lo_size,
		//! Hi pressure size.
		hi_size,
		//! Lo pressure hardness.
		lo_hardness,
		//! Hi pressure hardness.
		hi_hardness;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Synchronization request message.
/**
 * Requests raster data.
 *
 * - Client MUST create copy of the raster right after any ongoing drawing,
 * and start sending it to server.
 * - The copy is what is sent to the server and MUST NOT be altered during transfer.
 *
 * Response: Raster
 */
struct Synchronize
	: Message//, MemoryStack<Synchronize>
{
	Synchronize() throw()
		: Message(type::Synchronize, message::isSession)
	{ }
	
	~Synchronize() throw() { }
	
	/* unique data */
	
	// nothing needed
	
	// does not have any.
	
	/* functions */
	
	// nothing needed
};

//! Raster data message.
/**
 * Sends the image data, capable of serving only parts until the whole dataset is served.
 * Client that is to fullfill this request should make a copy of the raster data
 * since it MUST NOT change until the transfer is completed. The data MUST be in
 * lossless PNG format, other factors of the format are left for the client to decide,
 * though compression levels of 5 to 8 are recommended.
 *
 * This message is formed so that it can be send in chunks instead of one big message,
 * client is free to split the sent data into chunks if it sees fit, but the complete
 * raster must be transferred in order!
 *
 * Response: none
 */
struct Raster
	: Message//, MemoryStack<Raster>
{
	Raster() throw()
		: Message(type::Raster, message::isSession),
		offset(0),
		length(0),
		size(0),
		data(0)
	{ }
	
	~Raster() throw() { delete [] data; }
	
	/* unique data */
	
	uint32_t
		//! Offset of data chunk.
		offset,
		//! Size of raster data sent in this chunk.
		length,
		//! Size of the whole raster data.
		size;
	
	//! Raster data chunk.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! SyncWait message.
/**
 * Instructs client to enter SyncWait state.
 * No drawing data may be sent out after receiving this
 * (except if it completes already sent partial data).
 * 
 * - Client MUST wait for ACK/Sync from server before resuming.
 * - Client MUST fullfil any other requests by the server in timely manner.
 * - The response MUST be sent AFTER sending any remainder of incomplete data.
 * 
 * Response: Acknowledgement with event set to protocol::type::SyncWait.
 */
struct SyncWait
	: Message//, MemoryStack<SyncWait>
{
	SyncWait() throw()
		: Message(type::SyncWait, message::isSession)
	{ }
	
	~SyncWait() throw() { }
	
	/* unique data */
	
	// nothing needed
	
	/* functions */
	
	// nothing needed
};

//! Authentication request message.
/**
 * Request for password to login or join session.
 *
 * Response: Password
 */
struct Authentication
	: Message//, MemoryStack<Authentication>
{
	Authentication() throw()
		: Message(type::Authentication, message::isSession)
	{ }
	
	~Authentication() throw() { }
	
	/* unique data */
	
	//! Password seed; append this to the password string for hashing
	char seed[password_seed_size];
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Password message.
/**
 * Response to Authentication request.
 */
struct Password
	: Message//, MemoryStack<Password>
{
	Password() throw()
		: Message(type::Password, message::isSession)
	{ }
	
	~Password() throw() { }
	
	/* unique data */
	
	//! Password hash (SHA-1)
	char data[password_hash_size];
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Subscribe message.
/**
 * Client subscribes to the session.
 *
 * Response: Acknowledgement with event set to protocol::type::Subscribe.
 */
struct Subscribe
	: Message//, MemoryStack<Subscribe>
{
	Subscribe() throw()
		: Message(type::Subscribe, message::isSession)
	{ }
	
	~Subscribe() throw() { }
	
	/* unique data */
	
	// nothing needed
	
	/* functions */
	
	// nothing needed
};

//! Unsubscribe message.
/**
 * Client unsubscribes from session.
 *
 * Response: Acknowledgement with event set to protocol::type::Unsubscribe.
 */
struct Unsubscribe
	: Message//, MemoryStack<Unsubscribe>
{
	Unsubscribe() throw()
		: Message(type::Unsubscribe, message::isSession)
	{ }
	
	~Unsubscribe() throw() { }
	
	/* unique data */
	
	// nothing needed
	
	/* functions */
	
	// nothing needed
};

//! Admin Instruction message.
/**
 * Only accepted from an admin, or from session owner for altering the session.
 *
 * Response: Error or Acknowledgement with event set to protocol::type::Instruction.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Admin_interface
 */
struct Instruction
	: Message//, MemoryStack<Instruction>
{
	Instruction() throw()
		: Message(type::Instruction, message::isUser|message::isSession),
		length(0),
		data(0)
	{ }
	
	~Instruction() throw() { delete [] data; }
	
	/* unique data */
	
	uint8_t
		//! protocol::admin::command
		command,
		//! aux_data
		aux_data,
		//! aux data 2
		aux_data2;
	
	//! arb data length
	uint8_t length;
	
	//! arb data
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! List sessions request.
/**
 * Request to list the sessions on host.
 *
 * Response: sequential SessionInfo messages for all sessions,
 * followed by Acknowledgement with event set to protocol::type::ListSessions.
 */
struct ListSessions
	: Message//, MemoryStack<ListSessions>
{
	ListSessions() throw()
		: Message(type::ListSessions)
	{ }
	
	~ListSessions() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// needs none
};

//! Cancel instruction.
/**
 * Cancels the last request (such as Synchronize).
 */
struct Cancel
	: Message//, MemoryStack<Cancel>
{
	Cancel() throw()
		: Message(type::Cancel, message::isSession)
	{ }
	
	~Cancel() throw() { }
	
	/* unique data */
	
	// nothing needed
	
	/* functions */
	
	// nothing needed
};

//! User Info message.
/**
 * Modifiers: protocol::message::isUser
 *
 * Response: none
 */
struct UserInfo
	: Message//, MemoryStack<UserInfo>
{
	UserInfo() throw()
		: Message(type::UserInfo, message::isUser|message::isSession),
		mode(user_mode::None),
		event(user_event::None),
		length(0),
		name(0)
	{ }
	
	~UserInfo() throw() { delete [] name; }
	
	/* unique data */
	
	uint8_t
		//! User mode flags (protocol::user)
		mode,
		//! User event (protocol::user_event)
		event,
		//! Name length in bytes.
		length;
	
	//! User name.
	char* name;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Host info message
/**
 * Describes host limits, capabilities and load.
 *
 * Response: none
 */
struct HostInfo
	: Message//, MemoryStack<HostInfo>
{
	HostInfo() throw()
		: Message(type::HostInfo),
		sessions(0),
		sessionLimit(0),
		users(0),
		userLimit(0),
		nameLenLimit(0),
		maxSubscriptions(0),
		requirements(requirements::None),
		extensions(extensions::None)
	{ }
	
	~HostInfo() throw() { }
	
	/* unique data */
	
	uint8_t
		//! Number of sessions on server.
		sessions,
		//! Max number of sessions on server.
		sessionLimit,
		//! Connected users.
		users,
		//! Max connected users
		userLimit,
		//! User/session name length limit.
		nameLenLimit,
		//! Max session subscriptions per user.
		maxSubscriptions,
		//! Server operation flags (protocol::requirements).
		requirements,
		//! Extension support (protocol::extensions).
		extensions;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Session Info message.
struct SessionInfo
	: Message//, MemoryStack<SessionInfo>
{
	SessionInfo() throw()
		: Message(type::SessionInfo, message::isSession),
		width(0),
		height(0),
		owner(protocol::null_user),
		users(0),
		limit(0),
		mode(user_mode::None),
		flags(0),
		length(0),
		title(0)
	{ }
	
	~SessionInfo() throw() { delete [] title; }
	
	/* unique data */
	
	uint16_t
		//! Board width.
		width,
		//! Board height.
		height;
	
	uint8_t
		//! Session owner ID.
		owner,
		//! Number of subscribed users.
		users,
		//! User limit.
		limit,
		//! Default user mode.
		mode,
		//! Session flags.
		flags,
		//! Title length.
		length;
	
	//! Session title.
	char* title;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Acknowledgement message.
/**
 * Acknowledges some action or event.
 */
struct Acknowledgement
	: Message //, MemoryStack<Acknowledgement>
{
	Acknowledgement() throw()
		: Message(type::Acknowledgement),
		event(type::None)
	{ }
	
	~Acknowledgement() throw() { }
	
	/* unique data */
	
	//! Event identifier for which this is an acknowledgement for.
	uint8_t event;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Error message.
/**
 * Arbitrary error code message.
 */
struct Error
	: Message //, MemoryStack<Error>
{
	Error() throw()
		: Message(type::Error),
		code(error::None)
	{ }
	
	~Error() throw() { }
	
	/* unique data */
	
	//! Error code (protocol::error).
	uint16_t code;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw();
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Deflate Extension message.
/**
 * Contains an arbitrary number of messages compressed using Deflate algorithm.
 * Compression levels of 5 to 8 are recommended. This message is decompressed at
 * the host for proper propagation of the contained messages. The data inside may
 * contain any and all types of messages except raster message which is already
 * compressed and would not benefit from this. It is also recommended that the
 * minimum amount of data to be compressed is at least 300 bytes or so. The only
 * limit to this is that capacity of uncompressed size MUST NOT be exceeded
 * (about 65kbytes).
 *
 * Response: none
 */
struct Deflate
	: Message //, MemoryStack<Deflate>
{
	Deflate() throw()
		: Message(type::Deflate),
		uncompressed(0),
		length(0),
		data(0)
	{ }
	
	~Deflate() throw() { delete [] data; }
	
	/* unique data */
	
	uint16_t
		//! Size of the uncompressed data.
		uncompressed,
		//! Compressed data length.
		length;
	
	//! Compressed data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Chat Extension message.
/**
 * Sends a text string to be propagated to other users.
 * Client MUST NOT send chat messages with 0 length.
 */
struct Chat
	: Message //, MemoryStack<Chat>
{
	Chat() throw()
		: Message(type::Chat, message::isUser|message::isSession),
		length(0),
		data(0)
	{ }
	
	~Chat() throw() { delete [] data; }
	
	/* unique data */
	
	//! Message string length.
	uint8_t length;
	
	//! Message string (in UTF-8 format, or UTF-16 if the server so requires).
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Shared Palette message (proposed).
/**
 * Contains partial data of the shared palette
 */
struct Palette
	: Message//, MemoryStack<Palette>
{
	Palette() throw()
		: Message(type::Palette, message::isSession|message::isUser),
		offset(0),
		count(0),
		data(0)
	{ }
	
	~Palette() throw() { delete [] data; }
	
	/* unique data */
	
	uint8_t
		//! Color offset in shared palette.
		offset,
		//! Number of colors in data. Length of palette data is 3*color_count bytes.
		count;
	
	//! Palette data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len) throw(std::bad_alloc);
	size_t reqDataLen(const char *buf, size_t len) const throw();
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

} // namespace protocol

#endif // Protocol_INCLUDED

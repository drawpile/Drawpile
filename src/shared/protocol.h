/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#ifndef Protocol_INCLUDED
#define Protocol_INCLUDED

#include <cassert> // assert()
#include <stddef.h> // size_t
#include <stdint.h> // [u]int#_t

#include "templates.h"

#include "protocol.errors.h"
#include "protocol.defaults.h"
#include "protocol.tools.h"
#include "protocol.types.h"
#include "protocol.flags.h"

//! DrawPile network protocol.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol
 */
namespace protocol
{

//! Implemented protocol revision number.
const uint16_t revision = 6;

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
	Message(uint8_t _type, uint8_t _flags=protocol::message::None)
		: type(_type),
		user_id(protocol::null_user),
		modifiers(_flags),
		next(0),
		prev(0)
	{ }
	
	virtual ~Message() throw() { }
	
	//! Message type identifier (full list in protocol::type namespace).
	const uint8_t type;
	
	//! Originating user for the message, as assigned by the server.
	uint8_t user_id;
	
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
	 * @param len refers to size_t to which the length of returned buffer is stored.
	 *
	 * @return Buffer ready to be sent to network.
	 *
	 * @throw std::bad_alloc
	 */
	char* serialize(size_t &len) const;
	
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
	size_t reqDataLen(const char*, size_t) const;
	
	//! Unserializes char* buffer to associated message struct.
	/**
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
	size_t unserialize(const char*, size_t);
};

//! Protocol identifier.
/**
 * The first message sent to the server.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Protocol_identifier
 */
struct Identifier
	: Message
{
	Identifier()
		: Message(protocol::type::Identifier),
		revision(protocol::null_revision),
		level(protocol::null_implementation),
		extensions(protocol::extensions::None)
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
	
	//! Extension flags (see protocol::extensions for full list).
	uint8_t extensions;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Stroke info message.
/**
 * Minimal data required for drawing with the tool provided by ToolInfo.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Stroke_info
 */
struct StrokeInfo
	: Message
{
	StrokeInfo()
		: Message(protocol::type::StrokeInfo, message::isUser|message::isBundling),
		x(0),
		y(0)
	{ }
	
	~StrokeInfo() throw() { }
	
	/* unique data */
	
	uint16_t
		//! Horizontal coordinate.
		x,
		//! Vertical coordinate.
		y;
	
	//! Should default to 255 for devices that do not support it.
	uint8_t pressure;
	
	/* functions */
	
	/**
	 * @throw protocol::scrambled_buffer
	 */
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Stroke End message
/**
 * Marks the end of a line defined by StrokeInfo messages.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Stroke_end
 */
struct StrokeEnd
	: Message
{
	StrokeEnd()
		: Message(protocol::type::StrokeEnd, message::isUser)
	{ }
	
	~StrokeEnd() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// none needed
};

//! Tool Info message.
/**
 * Describes interpolating brush or some other tool.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Tool_info
 */
struct ToolInfo
	: Message
{
	ToolInfo()
		: Message(protocol::type::ToolInfo, message::isUser),
		tool_id(protocol::tool::None),
		lo_color(0),
		hi_color(0),
		lo_size(0),
		hi_size(0),
		lo_softness(0),
		hi_softness(0)
	{ }
	
	~ToolInfo() throw() { }
	
	/* unique data */
	
	//! Tool identifier (for full list, see protocol::tool).
	uint8_t tool_id;
	
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
		//! Lo pressure softness.
		lo_softness,
		//! Hi pressure softness.
		hi_softness;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Synchronization request message.
/**
 * Requests raster data.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Synchronize
 */
struct Synchronize
	: Message
{
	Synchronize()
		: Message(protocol::type::Synchronize)
	{ }
	
	~Synchronize() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// needs none
};

//! Raster data message.
/**
 * Serves raster data request delivered by Synchronize message.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Raster
 */
struct Raster
	: Message
{
	Raster()
		: Message(protocol::type::Raster),
		offset(0),
		length(0),
		size(0),
		data(0)
	{ }
	
	~Raster() throw() { delete [] data; }
	
	/* unique data */
	
	uint32_t
		//! Offset of raster data.
		offset,
		//! Length of raster data.
		length,
		//! Size of the whole raster.
		size;
	
	//! Raster data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! SyncWait message.
/**
 * Instructs client to enter SyncWait state.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#SyncWait
 */
struct SyncWait
	: Message
{
	SyncWait()
		: Message(protocol::type::SyncWait)
	{ }
	
	~SyncWait() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// needs none
};

//! Authentication request message.
/**
 * Request for password to login or join session.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Authentication
 */
struct Authentication
	: Message
{
	Authentication()
		: Message(protocol::type::Authentication),
		board_id(protocol::Global)
	{ }
	
	~Authentication() throw() { }
	
	/* unique data */
	
	//! Board identifier for which the auth request is aimed at (protocol::Global for server).
	uint8_t board_id;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Password message.
/**
 * Response to Authentication request.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Password
 */
struct Password
	: Message
{
	Password()
		: Message(protocol::type::Password),
		length(0),
		data(0)
	{ }
	
	~Password() throw() { delete [] data; }
	
	/* unique data */
	
	//! Board identifier, must be the same as in the auth request this is response to.
	uint8_t board_id;
	
	//! Password length.
	uint8_t length;
	
	//! Password data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Subscribe message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Subscribe
 */
struct Subscribe
	: Message
{
	Subscribe()
		: Message(protocol::type::Subscribe),
		board_id(protocol::Global)
	{ }
	
	~Subscribe() throw() { }
	
	/* unique data */
	
	//! Board identifier.
	uint8_t board_id;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Unsubscribe message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Unsubscribe
 */
struct Unsubscribe
	: Message
{
	Unsubscribe()
		: Message(protocol::type::Unsubscribe),
		board_id(protocol::Global)
	{ }
	
	~Unsubscribe() throw() { }
	
	/* unique data */
	
	//! Board identifier;
	uint8_t board_id;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Admin Instruction message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Instruction
 */
struct Instruction
	: Message
{
	Instruction()
		: Message(protocol::type::Instruction),
		length(0),
		data(0)
	{ }
	
	~Instruction() throw() { delete [] data; }
	
	/* unique data */
	
	//! Instruction string length.
	uint8_t length;
	
	//! Instruction string data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! List Boards request.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#List_boards
 *
 * Client requests info for all boards on server. Server should response with
 * sequential BoardInfo messages for all boards, and append ACK after all boards
 * have been described to mark the end of list.
 */
struct ListBoards
	: Message
{
	ListBoards()
		: Message(protocol::type::ListBoards)
	{ }
	
	~ListBoards() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// needs none
};

//! Cancel instruction.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Cancel
 */
struct Cancel
	: Message
{
	Cancel()
		: Message(protocol::type::Cancel)
	{ }
	
	~Cancel() throw() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	// needs none
};

//! User Info message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#User_info
 */
struct UserInfo
	: Message
{
	UserInfo()
		: Message(protocol::type::UserInfo, message::isUser),
		mode(protocol::user::None),
		event(protocol::user_event::None),
		length(0),
		name(0)
	{ }
	
	~UserInfo() throw() { delete [] name; }
	
	/* unique data */
	
	uint8_t
		//! Session ID
		board_id,
		//! User mode flags (see protocol::user for full list)
		mode,
		//! User event (see protocol::user_event for full list)
		event,
		//! Name length in bytes.
		length;
	
	//! User name.
	char* name;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Host info message
struct HostInfo
	: Message
{
	HostInfo()
		: Message(protocol::type::HostInfo),
		boards(0),
		boardLimit(0),
		users(0),
		userLimit(0),
		nameLenLimit(0),
		maxSubscriptions(0),
		requirements(protocol::requirements::None),
		extensions(protocol::extensions::None)
	{ }
	
	~HostInfo() throw() { }
	
	/* unique data */
	
	uint8_t
		//! Number of boards on server.
		boards,
		//! Max number of boards on server.
		boardLimit,
		//! Connected users.
		users,
		//! Max connected users (1 always reserved for admin over this).
		userLimit,
		//! User/board name length limit.
		nameLenLimit,
		//! Max board/session subscriptions per user.
		maxSubscriptions,
		//! Server operation flags (see protocol::requirements).
		requirements,
		//! Extension support (see protocol::extensions).
		extensions;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Board Info message.
struct BoardInfo
	: Message
{
	BoardInfo()
		: Message(protocol::type::BoardInfo),
		identifier(protocol::Global),
		width(0),
		height(0),
		owner(protocol::null_user),
		users(0),
		limit(0),
		uflags(protocol::user::None),
		flags(protocol::session::None),
		length(0),
		name(0)
	{ }
	
	~BoardInfo() throw() { delete [] name; }
	
	/* unique data */
	
	//! Board identifier.
	uint8_t identifier;
	
	uint16_t
		//! Board width.
		width,
		//! Board height.
		height;
	
	uint8_t
		//! Board owner user identifier.
		owner,
		//! Board users.
		users,
		//! Board user limit.
		limit,
		//! Default user flags.
		uflags,
		//! Session flags.
		flags,
		//! Board name length.
		length;
	
	//! Board name.
	char* name;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Acknowledgement message.
struct Acknowledgement
	: Message
{
	Acknowledgement()
		: Message(protocol::type::Acknowledgement),
		event(protocol::type::None)
	{ }
	
	~Acknowledgement() throw() { }
	
	/* unique data */
	
	//! Event identifier.
	uint8_t event;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Error message.
struct Error
	: Message
{
	Error()
		: Message(protocol::type::Error),
		code(protocol::error::None)
	{ }
	
	~Error() throw() { }
	
	/* unique data */
	
	//! Error code.
	uint16_t code;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Deflate Extension message.
struct Deflate
	: Message
{
	Deflate()
		: Message(protocol::type::Deflate),
		uncompressed(0),
		length(0),
		data(0)
	{ }
	
	~Deflate() throw() { delete [] data; }
	
	/* unique data */
	
	uint16_t
		//! Size of the data when uncompressed.
		uncompressed,
		//! Data length.
		length;
	
	//! Compressed data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Chat Extension message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Chat
 */
struct Chat
	: Message
{
	Chat()
		: Message(protocol::type::Chat, message::isUser),
		board_id(protocol::Global),
		length(0),
		data(0)
	{ }
	
	~Chat() throw() { delete [] data; }
	
	/* unique data */
	
	uint8_t
		//! Target board identifier (use protocol::Global for global messages).
		board_id,
		//! Message string length.
		length;
	
	//! Message string data.
	char* data;
	
	/* functions */
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

//! Shared Palette message (proposed).
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Palette
 */
struct Palette
	: Message
{
	Palette()
		: Message(protocol::type::Palette),
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
	
	size_t unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const throw();
	size_t payloadLength() const throw();
};

} // namespace protocol

#endif // Protocol_INCLUDED

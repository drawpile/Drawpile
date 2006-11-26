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

#include <stddef.h> // size_t
#include <stdint.h> // [u]int#_t

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

/*
 * only for getting the size of message count part of bundled messages.
 * Makes it easier to conform to same size :)
 */
const uint8_t message_count = 0;

//! Base for all message types.
/**
 * Minimum requirements for derived structs is the definitions of _type.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Message_structure
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Message_modifiers
 * @see protocol::type for full list of message types.
 *
 * @param _type message type.
 * @param _bundled does the message have the bundling modifier?
 *	Defaults to false.
 * @param _user does the message include user identifier?
 *	Defaults to false.
 */
struct Message
{
	Message(uint8_t _type, bool _bundled=false, bool _user=false)
		: type(_type),
		user_id(protocol::null_user),
		isBundled(_bundled),
		isUser(_user),
		next(0),
		prev(0)
	{ }
	
	virtual ~Message() { }
	
	//! Message type identifier (full list in protocol::type namespace).
	const uint8_t type;
	
	//! Originating user for the message, as assigned by the server.
	uint8_t user_id;
	
	const bool
		//! Message can bundle.
		isBundled,
		//! Message contains user ID.
		isUser;
	
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
	 * @return payload length in bytes
	 */
	virtual
	size_t payloadLength() const = 0;
	
	//! Serialize the message payload.
	/**
	 * The buffer must have room for at least payloadLength() bytes.
	 *
	 * @param buf serialized bytes will be stored here.
	 *
	 * @return number of bytes stored.
	 */
	virtual
	size_t serializePayload(char *buf) const = 0;
	
	//! Check if buf contains enough data to unserialize
	/**
	 * The value is not guaranteed to be constant for all message types, at all times.
	 *
	 * Meaning: the length returned may grow when the function is able to scan
	 * the full extent of the message.
	 *
	 * @param buf points to data buffer waiting for serialization.
	 * @param len declares the length of the data buffer.
	 *
	 * @return length of data needed to unserialize or completely scan the length.
	 * Only unserialize() if you have at least this much data in the buffer at the
	 * time of calling this function.
	 */
	virtual
	size_t reqDataLen(const char *buf, size_t len) const = 0;
	
	//! Unserializes char* buffer to associated message struct.
	/**
	 * Before calling unserialize(), you MUST ensure the buffer has enough data
	 * by calling reqDataLen(). Also, reqDataLen() is the only thing that can
	 * tell you how much data the call to unserialize() actually processed or will
	 * process of the provided data buffer.
	 *
	 * @param buf points to data buffer waiting for serialization.
	 * @param len declares the length of the data buffer.
	 */
	virtual
	void unserialize(const char* buf, size_t len) = 0;
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
	
	~Identifier() { }
	
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

	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
		: Message(protocol::type::StrokeInfo, true, true),
		x(0),
		y(0)
	{ }
	
	~StrokeInfo() { }
	
	/* unique data */
	
	uint16_t
		//! Horizontal coordinate.
		x,
		//! Vertical coordinate.
		y;
	
	//! Should default to 255 for devices that do not support it.
	uint8_t pressure;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
		: Message(protocol::type::StrokeEnd, false, true)
	{ }
	
	~StrokeEnd() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
		: Message(protocol::type::ToolInfo, false, true),
		tool_id(protocol::tool::None),
		lo_color(0),
		hi_color(0),
		lo_size(0),
		hi_size(0),
		lo_softness(0),
		hi_softness(0)
	{ }
	
	~ToolInfo() { }
	
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
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Synchronize() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Raster() { }
	
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
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~SyncWait() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Authentication() { }
	
	/* unique data */
	
	//! Board identifier for which the auth request is aimed at (protocol::Global for server).
	uint8_t board_id;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Password() { }
	
	/* unique data */
	
	//! Board identifier, must be the same as in the auth request this is response to.
	uint8_t board_id;
	
	//! Password length.
	uint8_t length;
	
	//! Password data.
	char* data;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Subscribe() { }
	
	/* unique data */
	
	//! Board identifier.
	uint8_t board_id;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Unsubscribe() { }
	
	/* unique data */
	
	//! Board identifier;
	uint8_t board_id;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Instruction() { }
	
	/* unique data */
	
	//! Instruction string length.
	uint8_t length;
	
	//! Instruction string data.
	char* data;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! List Boards request.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#List_boards
 */
struct ListBoards
	: Message
{
	ListBoards()
		: Message(protocol::type::ListBoards)
	{ }
	
	~ListBoards() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Cancel() { }
	
	/* unique data */
	
	// does not have any.
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! User Info message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#User_info
 */
struct UserInfo
	: Message
{
	UserInfo()
		: Message(protocol::type::UserInfo, false, true),
		mode(protocol::user::None),
		event(protocol::user_event::None),
		length(0),
		name(0)
	{ }
	
	~UserInfo() { }
	
	/* unique data */
	
	uint8_t
		//! User mode flags (see protocol::user for full list)
		mode,
		//! User event (see protocol::user_event for full list)
		event,
		//! Name length in bytes.
		length;
	
	//! User name.
	char* name;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~HostInfo() { }
	
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
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! Board Info message.
struct BoardInfo
	: Message
{
	BoardInfo()
		: Message(protocol::type::BoardInfo, true),
		identifier(protocol::Global),
		width(0),
		height(0),
		owner(protocol::null_user),
		users(0),
		userLimit(0),
		nameLength(0),
		name(0)
	{ }
	
	~BoardInfo() { }
	
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
		userLimit,
		//! Board name length.
		nameLength;
	
	//! Board name.
	char* name;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! Acknowledgement message.
struct Acknowledgement
	: Message
{
	Acknowledgement()
		: Message(protocol::type::Acknowledgement),
		event(protocol::type::None)
	{ }
	
	~Acknowledgement() { }
	
	/* unique data */
	
	//! Event identifier.
	uint8_t event;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! Error message.
struct Error
	: Message
{
	Error()
		: Message(protocol::type::Error),
		category(protocol::error::category::None),
		code(protocol::error::code::None)
	{ }
	
	~Error() { }
	
	/* unique data */
	
	uint16_t
		//! Error category.
		category,
		//! Error code.
		code;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Deflate() { }
	
	/* unique data */
	
	uint16_t
		//! Size of the data when uncompressed.
		uncompressed,
		//! Data length.
		length;
	
	//! Compressed data.
	char* data;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! Chat Extension message.
/**
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Chat
 */
struct Chat
	: Message
{
	Chat()
		: Message(protocol::type::Chat),
		board_id(protocol::Global),
		length(0),
		data(0)
	{ }
	
	~Chat() { }
	
	/* unique data */
	
	uint8_t
		//! Target board identifier (use protocol::Global for global messages).
		board_id,
		//! Message string length.
		length;
	
	//! Message string data.
	char* data;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
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
	
	~Palette() { }
	
	/* unique data */
	
	uint8_t
		//! Color offset in shared palette.
		offset,
		//! Number of colors in data. Length of palette data is 3*color_count bytes.
		count;
	
	//! Palette data.
	char* data;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len) const;
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

} // namespace protocol

#endif // Protocol_INCLUDED

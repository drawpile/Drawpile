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

#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include <stddef.h> // size_t
#include <stdint.h> // [u]int#_t

//! DrawPile network protocol.
namespace protocol
{

//! User ID for arbitrary user.
const uint8_t NoUser = 255;

/* only used by Identifier message */
const uint8_t identifier_size = 8;

//! Protocol identifier string.
const char identifierString[8] = {'D','r','a','w','P','i','l','e'};

//! Message type identifiers.
namespace type
{
	//! No message type specified, should assume error.
	const uint8_t None = 0;
	
	//! Protocol identifier message.
	const uint8_t Identifier = 7;
	
	//! Drawing data: Stroke Info message.
	const uint8_t StrokeInfo = 1;
	//! Drawing data: Stroke End message.
	const uint8_t StrokeEnd = 2;
	//! Drawing data: Tool Info message.
	const uint8_t ToolInfo = 3;
	
	//! Acknowledgement message.
	const uint8_t Acknowledgement = 255;
} // namespace type


//! Base for all message types.
/**
 * Minimum requirements for derived structs is the definitions of _type.
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
		user_id(NoUser),
		isBundled(_bundled),
		isUser(_user),
		next(0),
		prev(0)
	{ }
	
	virtual ~Message() { }
	
	//! Message type/identifier.
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
	 * by calling reqDataLen()
	 *
	 *
	 * @param buf points to data buffer waiting for serialization.
	 * @param len declares the length of the data buffer.
	 */
	virtual
	void unserialize(char* buf, size_t len) = 0;
};

//! Protocol identifier
/**
 * The first message sent to the server.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Protocol_identifier
 */
struct Identifier
	: Message
{
	Identifier()
		: Message(type::Identifier)
	{ }
	
	~Identifier() { }
	
	/* unique data */
	
	//! Protocol identifier.
	char identifier[identifier_size];
	
	uint16_t
		//! Protocol version.
		version,
		//! Client implementation level
		level;
	
	uint8_t
		//! Operation flags.
		flags,
		//! Extension flags.
		extensions;
	
	/* functions */
	
	void unserialize(const char* buf, size_t len);
	size_t reqDataLen(const char *buf, size_t len);

	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

//! Stroke info
/**
 * Minimal data required for drawing with the tool provided by ToolInfo.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Stroke_info
 */
struct StrokeInfo
	: Message
{
	StrokeInfo()
		: Message(type::StrokeInfo, true, true),
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
	size_t reqDataLen(const char *buf, size_t len);
	
	size_t serializePayload(char *buf) const;
	size_t payloadLength() const;
};

} // namespace protocol

#endif // PROTOCOL_H_INCLUDED

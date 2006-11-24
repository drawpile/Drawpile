/*******************************************************************************

Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

#include "sockets.h" // htonl(), ntohs(), etc.

#include <cassert> // assert()
#include <cstdlib> // NULL
#include <string> // memcpy()
#include <stdint.h> // uint16_t

//! DrawPile network protocol.
namespace protocol
{

//! User ID for arbitrary user.
const uint8_t NoUser = 255;

/* only used by Identifier message */
const uint8_t identifier_size = 8;

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
	
	/** Acknowledgement message. */
	const uint8_t Acknowledgement = 255;
} // namespace type


//! Base for all message types.
/**
 * Minimum requirements for derived structs are the definitions of _type and _minSize.
 *
 * @param _type message type.
 * @param _minSize minimum message size returned by length() function.
 * @param _bundled does the message have the bundling modifier?
 *	Defaults to false.
 * @param _user does the message include user identifier?
 *	Defaults to false.
 */
struct Message
{
	Message(uint8_t _type, size_t _minSize, bool _bundled=false, bool _user=false)
		: type(_type),
		user_id(NoUser),
		min(_minSize),
		isBundled(_bundled),
		isUser(_user),
		next(NULL),
		prev(NULL)
	{ }
	
	virtual ~Message() { }
	
	//! Message type/identifier.
	const uint8_t type;
	
	//! Originating user for the message, as assigned by the server.
	uint8_t user_id;
	
	//! Minimum size of the message.
	const size_t min;
	
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
	
	//! Get actual length of the message.
	/**
	 * Defaults to msg.min in cases where there's no variable length data included.
	 *
	 * @param buf has the raw data received from network.
	 * @param len contains the absolute length of buf.
	 *
	 * @return Buffer length required by unserialize() of the associated message type.
	 * This may change for variable length messages when there's not enough data available.
	 */
	virtual
	size_t length(char* buf, size_t len)
	{
		assert(buf != NULL);
		assert(buf[0] == type);
		return min;
	}
	
	/**
	 * Turn message (list) to char* buffer and set 'len' to the length of the buffer.
	 * By default, it includes message ID only.
	 *
	 * @param len refers to size_t to which the length of returned buffer is stored.
	 *
	 * @return NULL if out of memory.
	 * @return Buffer ready to be sent to network.
	 */
	virtual
	char* serialize(size_t &len)
	{
		char* buf = new (std::nothrow) char[1];
		if (!buf) return NULL;
		memcpy(buf, &type, 1);
		return buf;
	}
};

//! Protocol identifier
/**
 * The first message sent to the server.
 *
 * Has constant size of 14 bytes.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Protocol_identifier
 */
struct Identifier
	: Message /* Serializing<struct Identifier>*/
{
	Identifier()
		: Message(type::Identifier, 14)
	{ }
	
	~Identifier() { }
	
	/* unique data */
	
	//! Protocol identifier.
	char identifier[identifier_size];
	
	//! Protocol version.
	uint32_t version;
	
	uint8_t
		//! Operation flags.
		flags,
		//! Extension flags.
		extensions;
	
	/* functions */
	
	//! Serializes itself into char* buffer.
	///
	char* serialize(size_t &len)
	{
		assert(next != NULL || prev != NULL);
		
		char* buf = new (std::nothrow) char[min];
		if (!buf) return NULL;
		
		uint32_t tmp = htonl(version);
		
		memcpy(buf, &identifier, identifier_size);
		memcpy(buf+identifier_size, &tmp, sizeof(version));
		memcpy(buf+12, &flags, sizeof(flags));
		memcpy(buf+13, &extensions, sizeof(extensions));
		
		len = min;
		return buf;
	}
	
	//! Unserializes char* buffer to Identifier.
	static
	Identifier* unserialize(Identifier& in, char* buf, size_t len)
	{
		assert(buf[0] == in.type);
		assert(len < in.length(buf, len));
		
		memcpy(&in.identifier, buf, identifier_size);
		memcpy(&in.version, buf+identifier_size, sizeof(in.version));
		memcpy(&in.flags, buf+12, sizeof(in.flags));
		memcpy(&in.extensions, buf+13, sizeof(in.extensions));
		
		in.version = ntohl(in.version);
		
		return &in;
	}
};

//! Stroke info
/**
 * Minimal data required for drawing with the tool provided by ToolInfo.
 *
 * Has the constant size of 8 bytes.
 *
 * @see http://drawpile.sourceforge.net/wiki/index.php/Protocol#Stroke_info
 */
struct StrokeInfo
	: Message
{
	StrokeInfo()
		: Message(type::StrokeInfo,
		8,
		true, true),
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
	
	//! Turns itself into char* buffer.
	///
	char* serialize(size_t &len)
	{
		
		uint8_t count = 1;
		StrokeInfo *ptr = this;
		while (ptr->prev != NULL)
		{
			count++;
			ptr = static_cast<StrokeInfo*>(ptr->prev);
		}
		
		char* buf = new (std::nothrow) char[min*count];
		if (!buf) return NULL;
		
		uint16_t tmpX, tmpY;
		
		memcpy(buf, &ptr->type, 1);
		memcpy(buf+1, &NoUser, 1);
		memcpy(buf+2, &count, 1);
			
		char* tmp = buf+3;
		do
		{
			tmpX = htons(ptr->x), tmpY = htons(ptr->y);
			
			memcpy(tmp, &tmpX, 2);
			memcpy(tmp+=2, &tmpY, 2);
			memcpy(tmp+=2, &ptr->pressure, 1);
			tmp += 1;
			
			if (ptr->next == NULL) break;
			ptr = static_cast<StrokeInfo*>(ptr->next);
		}
		while (ptr != NULL);
		
		len = min*count;
		return buf;
	}
	
	//! Turns char* buffer to StrokeInfo structure list.
	static
	StrokeInfo* unserialize(StrokeInfo& in, char* buf, size_t len)
	{
		assert(buf[0] == in.type);
		assert(len < in.length(buf, len));
		
		uint8_t count;
		
		memcpy(&in.user_id, buf+1, sizeof(in.user_id));
		memcpy(&count, buf+2, sizeof(count));
		memcpy(&in.x, buf+3, sizeof(in.x));
		memcpy(&in.y, buf+3+sizeof(in.x), sizeof(in.y));
		memcpy(&in.pressure, buf+7, sizeof(in.pressure));
		
		assert(count == 0);
		
		in.x = ntohs(in.x);
		in.y = ntohs(in.y);
		
		return &in;
	}
};

} // namespace protocol

#endif // PROTOCOL_H_INCLUDED

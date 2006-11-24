/*******************************************************************************

Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#ifndef PROTOCOL_H_INCLUDED
#define PROTOCOL_H_INCLUDED

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
	
	/**
	 * Turn message (list) to char* buffer and set 'len' to the length of the buffer.
	 *
	 * @param len refers to size_t to which the length of returned buffer is stored.
	 *
	 * @return NULL if out of memory.
	 * @return Buffer ready to be sent to network.
	 */
	char* serialize(size_t &len) const;

	/**
	 * Get the number of bytes required to serialize the message payload.
	 * @return payload length in bytes
	 */
	virtual size_t payloadLength() const = 0;

	/**
	 * Serialize the message payload. The buffer must have room
	 * for at least payloadLength() bytes.
	 * @param buffer serialized bytes will be stored here.
	 * @return number of bytes stored.
	 */
	virtual size_t serializePayload(char *buffer) const = 0;
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
		: Message(type::Identifier)
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
	
	//! Unserializes char* buffer to Identifier.
	static Identifier* unserialize(Identifier& in, const char* buf, size_t len);

	//! Check if buf contains enough data to unserialize
	static bool isEnoughData(const char *buf, size_t len);

	size_t serializePayload(char *buffer) const;
	size_t payloadLength() const;
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
	
	//! Turns char* buffer to StrokeInfo structure list.
	static StrokeInfo* unserialize(StrokeInfo& in, const char* buf, size_t len);

	//! Check if buf contains enough data to unserialize
	static bool isEnoughData(const char *buf, size_t len);

	size_t serializePayload(char *buffer) const;
	size_t payloadLength() const;
};

} // namespace protocol

#endif // PROTOCOL_H_INCLUDED

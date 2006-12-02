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

#include "protocol.h"

#include <cassert> // assert()
#include <memory> // memcpy()

#include "templates.h"

namespace protocol {

/*
 * struct Message
 */

// Base serialization
char *Message::serialize(size_t &len) const
{
	// This _must_ be the last message in bundle.
	assert(next == NULL);
	
	size_t headerlen, length;
	
	if (bIsFlag(modifiers, message::isBundling))
	{
		// If we are bundling packets, there will be no extra headers
		headerlen = 0;
		length = sizeof(type) + sizeof(null_count)
			+ bIsFlag(modifiers, message::isUser)?sizeof(user_id):0;
	}
	else
	{
		// If messages are not bundled, simply concatenate whole messages
		headerlen = sizeof(type)
			+ bIsFlag(modifiers, message::isUser)?sizeof(user_id):0;
		length = headerlen;
	}
	
	// At least one message is serialized (the last)
	length += payloadLength();
	
	size_t count = 1;
	
	// first message in bundle
	const Message *ptr = 0;
	// Count number of messages to serialize and required size
	if (prev)
	{
		do
		{
			ptr = ptr->prev;
			++count;
			length += headerlen + ptr->payloadLength();
		}
		while (ptr->prev);
	}
	else
		ptr = this;
	// ptr now points to the first message in list (0 if no list).
	
	assert(ptr != 0); // some problems with the constness?
	
	// Allocate memory and serialize.
	char *data = new char[length];
	char *dataptr = data;

	if(bIsFlag(modifiers, message::isBundling)) {
		// Write bundled packets
		*(dataptr++) = type;
		if(bIsFlag(modifiers, message::isUser))
			*(dataptr++) = user_id;
		*(dataptr++) = count;
		
		while (ptr)
		{
			dataptr += ptr->serializePayload(dataptr);
			ptr=ptr->next;
		}
	} else {
		// Write whole packets
		while (ptr)
		{
			*(dataptr++) = ptr->type;
			if(bIsFlag(modifiers, message::isUser))
				*(dataptr++) = ptr->user_id;
			dataptr += ptr->serializePayload(dataptr);
			ptr=ptr->next;
		}
	}

	len = length;
	return data;
}

size_t Message::payloadLength() const throw()
{
	return 0;
}

size_t Message::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	return 0;
}

size_t Message::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + bIsFlag(modifiers, message::isUser)?sizeof(user_id):0;
}

size_t Message::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	if (bIsFlag(modifiers, message::isUser))
	{
		memcpy_t(user_id, buf+sizeof(type));
	}
	
	return sizeof(type) + bIsFlag(modifiers, message::isUser)?sizeof(user_id):0;
}

/*
 * struct Identifier
 */

size_t Identifier::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, &identifier, identifier_size); size_t i = identifier_size;
	
	memcpy_t(buf+i, bswap(copy(revision))); i += sizeof(revision);
	memcpy_t(buf+i, bswap(copy(level))); i += sizeof(level);
	memcpy_t(buf+i, extensions); i += sizeof(extensions);
	
	return i;
}

size_t Identifier::payloadLength() const throw()
{
	return identifier_size + sizeof(revision) + sizeof(level) + sizeof(extensions);
}

size_t Identifier::unserialize(const char* buf, size_t len)
{
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	memcpy(&identifier, buf+i, identifier_size); i += identifier_size;
	
	memcpy_t(revision, buf+i); i += sizeof(revision);
	memcpy_t(level, buf+i); i += sizeof(level);
	memcpy_t(extensions, buf+i); i += sizeof(extensions);
	
	bswap(revision);
	bswap(level);
	
	return i;
}

size_t Identifier::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(static_cast<uint8_t>(buf[0]) == protocol::type::Identifier);
	
	return sizeof(type) + identifier_size + sizeof(revision)
		+ sizeof(level) + sizeof(extensions);
}

/*
 * struct StrokeInfo
 */

size_t StrokeInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, bswap(copy(x))); size_t i = sizeof(x);
	memcpy_t(buf+i, bswap(copy(y))); i += sizeof(y);
	memcpy_t(buf+i, pressure); i += sizeof(pressure);
	
	return i;
}

size_t StrokeInfo::payloadLength() const throw()
{
	return sizeof(x) + sizeof(y) + sizeof(pressure);
}

size_t StrokeInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	uint8_t uid;
	memcpy_t(uid, buf+i); i += sizeof(user_id);
	
	uint8_t count;
	memcpy_t(count, buf+i); i += sizeof(count);
	
	if (count == 0)
		throw new scrambled_buffer();
	
	StrokeInfo *ptr = this;
	do
	{
		ptr->user_id = uid;
		memcpy_t(ptr->x, buf+i); i += sizeof(x);
		memcpy_t(ptr->y, buf+i); i += sizeof(y);
		memcpy_t(ptr->pressure, buf+i); i += sizeof(pressure);
		
		bswap(ptr->x);
		bswap(ptr->y);
		
		ptr = static_cast<StrokeInfo*>(next);
	}
	while (ptr);
	
	return i;
}

size_t StrokeInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	size_t h = sizeof(type) + sizeof(user_id) + sizeof(null_count);
	
	if (len < h + payloadLength())
		return h + payloadLength();
	else
		return h + buf[2] * payloadLength();
}

/*
 * struct StrokeEnd
 */

// nothing special needed

/*
 * struct ToolInfo
 */

size_t ToolInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(user_id, buf+i); i += sizeof(user_id);
	memcpy_t(tool_id, buf+i); i += sizeof(tool_id);
	
	memcpy_t(lo_color, buf+i); i += sizeof(lo_color);
	memcpy_t(hi_color, buf+i); i += sizeof(hi_color);
	
	memcpy_t(lo_size, buf+i); i += sizeof(lo_size);
	memcpy_t(hi_size, buf+i); i += sizeof(hi_size);
	memcpy_t(lo_softness, buf+i); i += sizeof(lo_softness);
	memcpy_t(hi_softness, buf+i); i += sizeof(hi_softness);
	
	return i;
}

size_t ToolInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(user_id) + payloadLength();
}

size_t ToolInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	assert(tool_id != protocol::tool::None);
	
	memcpy_t(buf, tool_id); size_t i = sizeof(tool_id);
	
	memcpy(buf+i, &lo_color, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(buf+i, &hi_color, sizeof(hi_color)); i += sizeof(hi_color);
	
	memcpy_t(buf+i, lo_size); i += sizeof(lo_size);
	memcpy_t(buf+i, hi_size); i += sizeof(hi_size);
	memcpy_t(buf+i, lo_softness); i += sizeof(lo_softness);
	memcpy_t(buf+i, hi_softness); i += sizeof(hi_softness);
	
	return i;
}

size_t ToolInfo::payloadLength() const throw()
{
	return sizeof(tool_id) + sizeof(lo_color)
		+ sizeof(hi_color) + sizeof(lo_size) + sizeof(hi_size)
		+ sizeof(lo_softness) + sizeof(hi_softness);
}

/*
 * struct Synchronize
 */

// no special implementation required

/*
 * struct Raster
 */

size_t Raster::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(offset, buf+i); i += sizeof(offset);
	memcpy_t(length, buf+i); i += sizeof(length);
	memcpy_t(size, buf+i); i += sizeof(size);
	
	bswap(offset);
	bswap(length);
	bswap(size);
	
	data = new char[length];
	memcpy(data, buf+i, length); i += length;
	
	return i;
}

size_t Raster::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type) + sizeof(offset) + sizeof(length))
		return sizeof(type) + sizeof(offset) + sizeof(length) + sizeof(size);
	else
	{
		uint32_t tmp = *(buf+sizeof(type)+sizeof(offset));
		return sizeof(type) + sizeof(offset) + sizeof(length) + sizeof(size)
			+ tmp;
	}
}

size_t Raster::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, bswap(copy(offset))); size_t i = sizeof(offset);
	memcpy_t(buf+i, bswap(copy(length))); i += sizeof(length);
	memcpy_t(buf+i, bswap(copy(size))); i += sizeof(size);
	
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Raster::payloadLength() const throw()
{
	return sizeof(offset) + sizeof(length) + sizeof(size) + length;
}

/*
 * struct SyncWait
 */

// no special implementation required

/*
 * struct Authentication
 */

// no special implementation required

/*
 * struct Password
 */

size_t Password::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(board_id, buf+i); i += sizeof(board_id);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	data = new char[length];
	memcpy(data, buf+i, length); i += length;
	
	return i;
}

size_t Password::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type)+sizeof(board_id)+sizeof(length))
		return sizeof(type)+sizeof(board_id)+sizeof(length);
	else
	{
		uint8_t tmp = *(buf+sizeof(type));
		return sizeof(type) + sizeof(board_id) + sizeof(length)
			+ tmp+sizeof(board_id);
	}
}

size_t Password::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, board_id); size_t i = sizeof(board_id);
	memcpy_t(buf+i, length); i += sizeof(length);
	
	memcpy(buf+i, data, length);
	
	return i;
}

size_t Password::payloadLength() const throw()
{
	return sizeof(board_id) + sizeof(length) + length;
}

/*
 * struct Subscribe
 */

size_t Subscribe::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy_t(board_id, buf+sizeof(type));
	
	return sizeof(type)+sizeof(board_id);
}

size_t Subscribe::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(board_id);
}

size_t Subscribe::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, board_id);
	
	return sizeof(board_id);
}

size_t Subscribe::payloadLength() const throw()
{
	return sizeof(board_id);
}

/*
 * struct Unsubscribe
 */

size_t Unsubscribe::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy_t(board_id, buf+sizeof(type));
	
	return sizeof(type) + sizeof(board_id);
}

size_t Unsubscribe::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(board_id);
}

size_t Unsubscribe::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, board_id);
	
	return sizeof(board_id);
}

size_t Unsubscribe::payloadLength() const throw()
{
	return sizeof(board_id);
}

/*
 * struct Instruction
 */

size_t Instruction::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	data = new char[length];
	memcpy(data, buf+i, length); i += length;
	
	return i;
}

size_t Instruction::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type) + sizeof(length))
		return sizeof(type) + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+sizeof(type));
		
		return sizeof(type) + sizeof(length) + rlen;
	}
}

size_t Instruction::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, length);
	memcpy(buf+sizeof(length), data, length);
	
	return sizeof(length) + length;
}

size_t Instruction::payloadLength() const throw()
{
	return sizeof(length) + length;
}

/*
 * struct ListBoards
 */

// no special implementation required

/*
 * struct Cancel
 */

// no special implementation required

/*
 * struct UserInfo
 */

size_t UserInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, buf+i); i += sizeof(user_id);
	
	memcpy_t(board_id, buf+i); i += sizeof(board_id);
	memcpy_t(mode, buf+i); i += sizeof(mode);
	memcpy_t(event, buf+i); i += sizeof(event);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	name = new char[length];
	memcpy(name, buf+i, length); i += length;
	
	return i;
}

size_t UserInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	size_t off = sizeof(type) + sizeof(board_id) + sizeof(mode) + sizeof(event);
	if (len < off + sizeof(length))
		return off + sizeof(length);
	else
	{
		uint8_t rlen;
	
		memcpy_t(rlen, buf+off);
		
		return off + sizeof(length) + rlen;
	}
}

size_t UserInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, board_id); size_t i = sizeof(board_id);
	memcpy_t(buf+i, mode); i += sizeof(mode);
	memcpy_t(buf+i, event); i += sizeof(event);
	memcpy_t(buf+i, length); i+= sizeof(length);
	
	memcpy(buf+i, name, length); i += length;
	
	return i;
}

size_t UserInfo::payloadLength() const throw()
{
	return sizeof(mode) + sizeof(event) + sizeof(length) + length;
}

/*
 * struct HostInfo
 */

size_t HostInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(boards, buf+i); i += sizeof(boards);
	memcpy_t(boardLimit, buf+i); i += sizeof(boardLimit);
	memcpy_t(users, buf+i); i += sizeof(users);
	memcpy_t(userLimit, buf+i); i += sizeof(userLimit);
	memcpy_t(nameLenLimit, buf+i); i += sizeof(nameLenLimit);
	memcpy_t(maxSubscriptions, buf+i); i += sizeof(maxSubscriptions);
	memcpy_t(requirements, buf+i); i += sizeof(requirements);
	memcpy_t(extensions, buf+i); i += sizeof(extensions);
	
	return i;
}

size_t HostInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + payloadLength();
}

size_t HostInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);

	memcpy_t(buf, boards); size_t i = sizeof(boards);
	memcpy_t(buf+i, boardLimit); i += sizeof(boardLimit);
	memcpy_t(buf+i, users); i += sizeof(users);
	memcpy_t(buf+i, userLimit); i += sizeof(userLimit);
	memcpy_t(buf+i, nameLenLimit); i += sizeof(nameLenLimit);
	memcpy_t(buf+i, maxSubscriptions); i += sizeof(maxSubscriptions);
	memcpy_t(buf+i, requirements); i += sizeof(requirements);
	memcpy_t(buf+i, extensions); i += sizeof(extensions);
	
	return i;
}

size_t HostInfo::payloadLength() const throw()
{
	return sizeof(boards) + sizeof(boardLimit) + sizeof(users)
		+ sizeof(userLimit) + sizeof(nameLenLimit) + sizeof(maxSubscriptions)
		+ sizeof(requirements) + sizeof(extensions);
}

/*
 * struct BoardInfo
 */

size_t BoardInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	memcpy(&identifier, buf+i, sizeof(identifier)); i += sizeof(identifier);
	
	memcpy_t(width, buf+i); i += sizeof(width);
	memcpy_t(height, buf+i); i += sizeof(width);
	memcpy_t(owner, buf+i); i += sizeof(owner);
	memcpy_t(users, buf+i); i += sizeof(users);
	memcpy_t(limit, buf+i); i += sizeof(limit);
	memcpy_t(uflags, buf+i); i += sizeof(uflags);
	memcpy_t(flags, buf+i); i += sizeof(uflags);
	memcpy_t(length, buf+i); i += sizeof(width);
	
	name = new char[length];
	memcpy(name, buf+i, length); i += length;
	
	return i;
}

size_t BoardInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	size_t p = sizeof(type) + sizeof(identifier) + sizeof(width) + sizeof(height)
		+ sizeof(owner) + sizeof(users) + sizeof(limit) + sizeof(limit);
	if (len < p + sizeof(length))
		return p + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+p);
		
		return p + sizeof(length) + rlen;
		// return p + sizeof(length) + memcpy_t_t(buf+p, length));
	}
}

size_t BoardInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, &identifier, sizeof(identifier)); size_t i = sizeof(identifier);
	
	memcpy_t(buf+i, width); i += sizeof(width);
	memcpy_t(buf+i, height); i += sizeof(height);
	memcpy_t(buf+i, owner); i += sizeof(owner);
	memcpy_t(buf+i, users); i += sizeof(users);
	memcpy_t(buf+i, limit); i += sizeof(limit);
	memcpy_t(buf+i, length); i += sizeof(length);
	
	memcpy(buf+i, name, length); i += length;

	return i;
}

size_t BoardInfo::payloadLength() const throw()
{
	return sizeof(identifier) + sizeof(width) + sizeof(height) + sizeof(owner)
		+ sizeof(users) + sizeof(limit) + sizeof(limit) + length;
}

/*
 * struct Acknowledgement
 */

size_t Acknowledgement::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy_t(event, buf+sizeof(type));
	
	return sizeof(type) + sizeof(event);
}

size_t Acknowledgement::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(event);
}

size_t Acknowledgement::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, event);
	
	return sizeof(event);
}

size_t Acknowledgement::payloadLength() const throw()
{
	return sizeof(event);
}

/*
 * struct Error
 */

size_t Error::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy_t(code, buf+sizeof(type));
	
	return sizeof(type) + sizeof(code);
}

size_t Error::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(code);
}

size_t Error::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, code);
	
	return sizeof(code);
}

size_t Error::payloadLength() const throw()
{
	return sizeof(code);
}

/*
 * struct Deflate
 */

size_t Deflate::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(uncompressed, buf+i); i += sizeof(uncompressed);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	bswap(uncompressed);
	bswap(length);
	
	data = new char[length];
	memcpy(data, buf+i, length);  i += length;
	
	return i;
}

size_t Deflate::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type) + sizeof(uncompressed) + sizeof(length))
		return sizeof(type) + sizeof(uncompressed) + sizeof(length);
	else
	{
		uint16_t rlen;
		
		memcpy_t(rlen, buf+sizeof(type)+sizeof(uncompressed));
		bswap(rlen);
		
		return sizeof(type) + sizeof(uncompressed) + sizeof(length) + rlen;
	}
}

size_t Deflate::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, bswap(copy(uncompressed))); size_t i = sizeof(uncompressed);
	memcpy_t(buf+i, bswap(copy(length))); i += sizeof(length);
	
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Deflate::payloadLength() const throw()
{
	return sizeof(uncompressed) + sizeof(length) + length;
}

/*
 * struct Chat
 */

size_t Chat::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy_t(board_id, buf+i); i += sizeof(board_id);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	data = new char[length];
	memcpy(data, buf+i, length); i += length;
	
	return i;
}

size_t Chat::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type) + sizeof(board_id) + sizeof(length))
		return sizeof(type) + sizeof(board_id) + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+sizeof(type)+sizeof(board_id));
		
		return sizeof(type) + sizeof(board_id) + sizeof(length) + rlen;
	}
}

size_t Chat::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, board_id); size_t i = sizeof(board_id);
	memcpy_t(buf+i, length); i += sizeof(length);
	
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Chat::payloadLength() const throw()
{
	return sizeof(board_id) + sizeof(length) + length;
}

/*
 * struct Palette
 */

size_t Palette::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	memcpy_t(offset, buf+i); i += sizeof(offset);
	memcpy_t(count, buf+i); i += sizeof(count);
	
	data = new char[count*RGB_size];
	memcpy(data, buf+i, count*RGB_size); i += count*RGB_size;
	
	return i;
}

size_t Palette::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	if (len < sizeof(type) + sizeof(offset) + sizeof(count))
		return sizeof(type) + sizeof(offset) + sizeof(count);
	else
	{
		uint8_t tmp = *(buf+sizeof(type)+sizeof(offset));
		return sizeof(type) + sizeof(offset) + sizeof(count)
			+ tmp * RGB_size;
	}
}

size_t Palette::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, offset); size_t i = sizeof(offset);
	memcpy_t(buf+i, count); i += sizeof(count);
	
	memcpy(buf+i, data, count*RGB_size); i += count*RGB_size;
	
	return i;
}

size_t Palette::payloadLength() const throw()
{
	return sizeof(offset) + sizeof(count) + count * RGB_size;
}

} // namespace protocol

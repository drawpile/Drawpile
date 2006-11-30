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
		bswap_mem(user_id, buf+sizeof(type));
	
	return sizeof(type) + bIsFlag(modifiers, message::isUser)?sizeof(user_id):0;
}

/*
 * struct Identifier
 */

size_t Identifier::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, &identifier, identifier_size); size_t i = identifier_size;
	
	bswap_mem(buf+i, revision); i += sizeof(revision);
	bswap_mem(buf+i, level); i += sizeof(level);
	bswap_mem(buf+i, extensions); i += sizeof(extensions);
	
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
	
	bswap_mem(revision, buf+i); i += sizeof(revision);
	bswap_mem(level, buf+i); i += sizeof(level);
	bswap_mem(extensions, buf+i); i += sizeof(extensions);
	
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
	
	bswap_mem(buf, x); size_t i = sizeof(x);
	bswap_mem(buf+i, y); i += sizeof(y);
	bswap_mem(buf+i, pressure); i += sizeof(pressure);
	
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
	
	uint8_t count, uid; // temporary
	
	size_t i = sizeof(type);
	
	bswap_mem(uid, buf+i); i += sizeof(user_id);
	bswap_mem(count, buf+i); i += sizeof(count);
	
	if (count == 0)
		throw new scrambled_buffer();
	
	StrokeInfo *ptr = this;
	do
	{
		ptr->user_id = uid;
		bswap_mem(ptr->x, buf+i); i += sizeof(x);
		bswap_mem(ptr->y, buf+i); i += sizeof(y);
		bswap_mem(ptr->pressure, buf+i); i += sizeof(pressure);
		
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
	bswap_mem(user_id, buf+i); i += sizeof(user_id);
	bswap_mem(tool_id, buf+i); i += sizeof(tool_id);
	
	memcpy(&lo_color, buf+i, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(&hi_color, buf+i, sizeof(hi_color)); i += sizeof(hi_color);
	
	bswap_mem(lo_size, buf+i); i += sizeof(lo_size);
	bswap_mem(hi_size, buf+i); i += sizeof(hi_size);
	bswap_mem(lo_softness, buf+i); i += sizeof(lo_softness);
	bswap_mem(hi_softness, buf+i); i += sizeof(hi_softness);
	
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
	
	bswap_mem(buf, tool_id); size_t i = sizeof(tool_id);
	
	memcpy(buf+i, &lo_color, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(buf+i, &hi_color, sizeof(hi_color)); i += sizeof(hi_color);
	
	bswap_mem(buf+i, lo_size); i += sizeof(lo_size);
	bswap_mem(buf+i, hi_size); i += sizeof(hi_size);
	bswap_mem(buf+i, lo_softness); i += sizeof(lo_softness);
	bswap_mem(buf+i, hi_softness); i += sizeof(hi_softness);
	
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
	bswap_mem(offset, buf+i); i += sizeof(offset);
	bswap_mem(length, buf+i); i += sizeof(length);
	bswap_mem(size, buf+i); i += sizeof(size);
	
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
		return sizeof(type) + sizeof(offset) + sizeof(length) + sizeof(size)
			+ bswap_mem<uint32_t>(buf+sizeof(type)+sizeof(offset));
	}
}

size_t Raster::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, offset); size_t i = sizeof(offset);
	bswap_mem(buf+i, length); i += sizeof(length);
	bswap_mem(buf+i, size); i += sizeof(size);
	
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
	bswap_mem(board_id, buf+i); i += sizeof(board_id);
	bswap_mem(length, buf+i); i += sizeof(length);
	
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
		return sizeof(type) + sizeof(board_id) + sizeof(length)
			+ bswap_mem<uint8_t>(buf+sizeof(type)+sizeof(board_id));
	}
}

size_t Password::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, board_id); size_t i = sizeof(board_id);
	bswap_mem(buf+i, length); i += sizeof(length);
	
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
	
	bswap_mem(board_id, buf+sizeof(type));
	
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
	
	bswap_mem(buf, board_id);
	
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
	
	bswap_mem(board_id, buf+sizeof(type));
	
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
	
	bswap_mem(buf, board_id);
	
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
	bswap_mem(length, buf+i); i += sizeof(length);
	
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
		
		bswap_mem(rlen, buf+sizeof(type));
		
		return sizeof(type) + sizeof(length) + rlen;
	}
}

size_t Instruction::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, length);
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
	
	bswap_mem(user_id, buf+i); i += sizeof(user_id);
	
	bswap_mem(board_id, buf+i); i += sizeof(board_id);
	bswap_mem(mode, buf+i); i += sizeof(mode);
	bswap_mem(event, buf+i); i += sizeof(event);
	bswap_mem(length, buf+i); i += sizeof(length);
	
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
	
		bswap_mem(rlen, buf+off);
		
		return off + sizeof(length) + rlen;
	}
}

size_t UserInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, board_id); size_t i = sizeof(board_id);
	bswap_mem(buf+i, mode); i += sizeof(mode);
	bswap_mem(buf+i, event); i += sizeof(event);
	bswap_mem(buf+i, length); i+= sizeof(length);
	
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
	bswap_mem(boards, buf+i); i += sizeof(boards);
	bswap_mem(boardLimit, buf+i); i += sizeof(boardLimit);
	bswap_mem(users, buf+i); i += sizeof(users);
	bswap_mem(userLimit, buf+i); i += sizeof(userLimit);
	bswap_mem(nameLenLimit, buf+i); i += sizeof(nameLenLimit);
	bswap_mem(maxSubscriptions, buf+i); i += sizeof(maxSubscriptions);
	bswap_mem(requirements, buf+i); i += sizeof(requirements);
	bswap_mem(extensions, buf+i); i += sizeof(extensions);
	
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

	bswap_mem(buf, boards); size_t i = sizeof(boards);
	bswap_mem(buf+i, boardLimit); i += sizeof(boardLimit);
	bswap_mem(buf+i, users); i += sizeof(users);
	bswap_mem(buf+i, userLimit); i += sizeof(userLimit);
	bswap_mem(buf+i, nameLenLimit); i += sizeof(nameLenLimit);
	bswap_mem(buf+i, maxSubscriptions); i += sizeof(maxSubscriptions);
	bswap_mem(buf+i, requirements); i += sizeof(requirements);
	bswap_mem(buf+i, extensions); i += sizeof(extensions);
	
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
	
	bswap_mem(width, buf+i); i += sizeof(width);
	bswap_mem(height, buf+i); i += sizeof(width);
	bswap_mem(owner, buf+i); i += sizeof(owner);
	bswap_mem(users, buf+i); i += sizeof(users);
	bswap_mem(limit, buf+i); i += sizeof(limit);
	bswap_mem(uflags, buf+i); i += sizeof(uflags);
	bswap_mem(flags, buf+i); i += sizeof(uflags);
	bswap_mem(length, buf+i); i += sizeof(width);
	
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
		
		bswap_mem(rlen, buf+p);
		
		return p + sizeof(length) + rlen;
		// return p + sizeof(length) + bswap_mem_t(buf+p, length));
	}
}

size_t BoardInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, &identifier, sizeof(identifier)); size_t i = sizeof(identifier);
	
	bswap_mem(buf+i, width); i += sizeof(width);
	bswap_mem(buf+i, height); i += sizeof(height);
	bswap_mem(buf+i, owner); i += sizeof(owner);
	bswap_mem(buf+i, users); i += sizeof(users);
	bswap_mem(buf+i, limit); i += sizeof(limit);
	bswap_mem(buf+i, length); i += sizeof(length);
	
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
	
	bswap_mem(event, buf+sizeof(type));
	
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
	
	bswap_mem(buf, event);
	
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
	
	bswap_mem(code, buf+sizeof(type));
	
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
	
	bswap_mem(buf, code);
	
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
	bswap_mem(uncompressed, buf+i); i += sizeof(uncompressed);
	bswap_mem(length, buf+i); i += sizeof(length);
	
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
		
		bswap_mem(rlen, buf+sizeof(type)+sizeof(uncompressed));
		
		return sizeof(type) + sizeof(uncompressed) + sizeof(length) + rlen;
	}
}

size_t Deflate::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, uncompressed); size_t i = sizeof(uncompressed);
	bswap_mem(buf+i, length); i += sizeof(length);
	
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
	bswap_mem(board_id, buf+i); i += sizeof(board_id);
	bswap_mem(length, buf+i); i += sizeof(length);
	
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
		
		bswap_mem(rlen, buf+sizeof(type)+sizeof(board_id));
		
		return sizeof(type) + sizeof(board_id) + sizeof(length) + rlen;
	}
}

size_t Chat::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, board_id); size_t i = sizeof(board_id);
	bswap_mem(buf+i, length); i += sizeof(length);
	
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
	
	bswap_mem(offset, buf+i); i += sizeof(offset);
	bswap_mem(count, buf+i); i += sizeof(count);
	
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
		return sizeof(type) + sizeof(offset) + sizeof(count)
			+ bswap_mem<uint8_t>(buf+sizeof(type)+sizeof(offset)) * RGB_size;
	}
}

size_t Palette::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	bswap_mem(buf, offset); size_t i = sizeof(offset);
	bswap_mem(buf+i, count); i += sizeof(count);
	
	memcpy(buf+i, data, count*RGB_size); i += count*RGB_size;
	
	return i;
}

size_t Palette::payloadLength() const throw()
{
	return sizeof(offset) + sizeof(count) + count * RGB_size;
}

} // namespace protocol

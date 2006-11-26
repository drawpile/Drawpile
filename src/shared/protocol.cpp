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

/* struct Message */

// Base serialization
char *Message::serialize(size_t &len) const
{
	// This _must_ be the last message in bundle.
	assert(next == NULL);
	
	size_t headerlen, length;
	
	if (isBundled)
	{
		// If we are bundling packets, there will be no extra headers
		headerlen = 0;
		length = sizeof(type) + sizeof(null_count) + isUser?sizeof(user_id):0;
	}
	else
	{
		// If messages are not bundled, simply concatenate whole messages
		headerlen = sizeof(type) + isUser?sizeof(user_id):0;
		length = headerlen;
	}
	
	// At least one message is serialized (the last)
	length += payloadLength();
	
	size_t count = 1;
	
	// first message in bundle
	const Message *ptr;
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
	
	// Allocate memory and serialize.
	char *data = new char[length];
	char *dataptr = data;

	if(isBundled) {
		// Write bundled packets
		*(dataptr++) = type;
		if(isUser)
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
			if(isUser)
				*(dataptr++) = ptr->user_id;
			dataptr += ptr->serializePayload(dataptr);
			ptr=ptr->next;
		}
	}

	len = length;
	return data;
}

/* struct Identifier */

size_t Identifier::serializePayload(char *buffer) const
{
	size_t i = identifier_size;
	memcpy(buffer, &identifier, identifier_size);
	memcpy(buffer+i, &bswap(revision), sizeof(revision)); i += sizeof(revision);
	memcpy(buffer+i, &bswap(level), sizeof(level)); i += sizeof(level);
	memcpy(buffer+i, &extensions, sizeof(extensions)); i += sizeof(extensions);
	
	return i;
}

size_t Identifier::payloadLength() const
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
	
	memcpy(&extensions, buf+i, sizeof(extensions)); i += sizeof(extensions);
	
	return i;
}

size_t Identifier::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(static_cast<uint8_t>(buf[0]) == protocol::type::Identifier);
	
	return sizeof(type)+sizeof(user_id) + identifier_size + sizeof(revision) + sizeof(level)
		+ sizeof(extensions);
}

/* struct StrokeInfo */

size_t StrokeInfo::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &bswap(x), sizeof(x)); size_t i = sizeof(x);
	memcpy(buf+i, &bswap(y), sizeof(y)); i += sizeof(y);
	memcpy(buf+i, &pressure, sizeof(pressure)); i += sizeof(pressure);
	
	return i;
}

size_t StrokeInfo::payloadLength() const
{
	return sizeof(x) + sizeof(y) + sizeof(pressure);
}

size_t StrokeInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	uint8_t count, uid;
	
	size_t i = sizeof(type);
	memcpy(&uid, buf+i, sizeof(user_id)); i += sizeof(user_id);
	memcpy(&count, buf+i, sizeof(count)); i += sizeof(count);
	
	if (count == 0)
	{
		// TODO
		// throw new scrambled_buffer();
	}
	
	StrokeInfo *ptr = this;
	do
	{
		ptr->user_id = uid;
		bswap_mem(ptr->x, buf+i); i += sizeof(x);
		bswap_mem(ptr->y, buf+i); i += sizeof(y);
		memcpy(&ptr->pressure, buf+i, sizeof(pressure)); i += sizeof(pressure);
		
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

/* struct StrokeEnd */

size_t StrokeEnd::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy(&user_id, buf+sizeof(type), sizeof(user_id));
	
	return sizeof(type)+sizeof(user_id);
}

size_t StrokeEnd::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t StrokeEnd::serializePayload(char *buf) const
{
	assert(buf != 0);
	//assert(1);
	
	return 0;
}

size_t StrokeEnd::payloadLength() const
{
	return 0;
}

/* struct ToolInfo */

size_t ToolInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&user_id, buf+i, sizeof(user_id)); i += sizeof(user_id);
	memcpy(&tool_id, buf+i, sizeof(tool_id)); i += sizeof(tool_id);
	memcpy(&lo_color, buf+i, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(&hi_color, buf+i, sizeof(hi_color)); i += sizeof(hi_color);
	memcpy(&lo_size, buf+i, sizeof(lo_size)); i += sizeof(lo_size);
	memcpy(&hi_size, buf+i, sizeof(hi_size)); i += sizeof(hi_size);
	memcpy(&lo_softness, buf+i, sizeof(lo_softness)); i += sizeof(lo_softness);
	memcpy(&hi_softness, buf+i, sizeof(hi_softness)); i += sizeof(hi_softness);
	
	/*
	lo_color = bswap(lo_color);
	hi_color = bswap(hi_color);
	*/
	
	return i;
}

size_t ToolInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(user_id) + payloadLength();
}

size_t ToolInfo::serializePayload(char *buf) const
{
	assert(buf != 0);
	assert(tool_id != protocol::tool::None);
	
	/*
	lo_color = bswap(lo_color);
	hi_color = bswap(hi_color);
	*/
	
	memcpy(buf, &tool_id, sizeof(tool_id)); size_t i = sizeof(tool_id);
	memcpy(buf+i, &lo_color, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(buf+i, &hi_color, sizeof(hi_color)); i += sizeof(hi_color);
	memcpy(buf+i, &lo_size, sizeof(lo_size)); i += sizeof(lo_size);
	memcpy(buf+i, &hi_size, sizeof(hi_size)); i += sizeof(hi_size);
	memcpy(buf+i, &lo_softness, sizeof(lo_softness)); i += sizeof(lo_softness);
	memcpy(buf+i, &hi_softness, sizeof(hi_softness)); i += sizeof(hi_softness);

	return i;
}

size_t ToolInfo::payloadLength() const
{
	return sizeof(tool_id) + sizeof(lo_color)
		+ sizeof(hi_color) + sizeof(lo_size) + sizeof(hi_size)
		+ sizeof(lo_softness) + sizeof(hi_softness);
}

/* struct Synchronize */

size_t Synchronize::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	return sizeof(type);
}

size_t Synchronize::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t Synchronize::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	return 0;
}

size_t Synchronize::payloadLength() const
{
	return 0;
}

/* struct Raster */

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
		uint32_t rlen; // temporary
		
		bswap_mem(rlen, buf+sizeof(type)+sizeof(offset));
		
		return sizeof(type) + sizeof(offset) + sizeof(length) + sizeof(size) + rlen;
	}
}

size_t Raster::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &bswap(offset), sizeof(offset)); size_t i = sizeof(offset);
	memcpy(buf+i, &bswap(length), sizeof(length)); i += sizeof(length);
	memcpy(buf+i, &bswap(size), sizeof(size)); i += sizeof(size);
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Raster::payloadLength() const
{
	return sizeof(offset) + sizeof(length) + sizeof(size) + length;
}

/* struct SyncWait */

size_t SyncWait::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	return sizeof(type);
}

size_t SyncWait::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t SyncWait::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	return 0;
}

size_t SyncWait::payloadLength() const
{
	return 0;
}

/* struct Authentication */

size_t Authentication::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	return sizeof(type);
}

size_t Authentication::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t Authentication::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	return 0;
}

size_t Authentication::payloadLength() const
{
	return 0;
}

/* struct Password */

size_t Password::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&board_id, buf+i, sizeof(board_id)); i += sizeof(board_id);
	memcpy(&length, buf+i, sizeof(length)); i += sizeof(length);
	
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
		uint8_t rlen;
		
		memcpy(&rlen, buf+sizeof(type)+sizeof(board_id), sizeof(length));
		
		return sizeof(type) + sizeof(board_id) + sizeof(length) + rlen;
	}
}

size_t Password::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &board_id, sizeof(board_id)); size_t i = sizeof(board_id);
	memcpy(buf+i, &length, sizeof(length)); i += sizeof(length);
	memcpy(buf+i, data, length);
	
	return i;
}

size_t Password::payloadLength() const
{
	return sizeof(board_id) + sizeof(length) + length;
}

/* struct Subscribe */

size_t Subscribe::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy(&board_id, buf+sizeof(type), sizeof(board_id));
	
	return sizeof(type)+sizeof(board_id);
}

size_t Subscribe::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(board_id);
}

size_t Subscribe::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &board_id, sizeof(board_id));
	
	return sizeof(board_id);
}

size_t Subscribe::payloadLength() const
{
	return sizeof(board_id);
}

/* struct Unsubscribe */

size_t Unsubscribe::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy(&board_id, buf+sizeof(type), sizeof(board_id));
	
	return sizeof(type) + sizeof(board_id);
}

size_t Unsubscribe::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(board_id);
}

size_t Unsubscribe::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &board_id, sizeof(board_id));
	
	return sizeof(board_id);
}

size_t Unsubscribe::payloadLength() const
{
	return sizeof(board_id);
}

/* struct Instruction */

size_t Instruction::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&length, buf+i, sizeof(length)); i += sizeof(length);
	
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
		
		memcpy(&rlen, buf+sizeof(type), sizeof(length));
		
		return sizeof(type) + sizeof(length) + rlen;
	}
}

size_t Instruction::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &length, sizeof(length));
	memcpy(buf+sizeof(length), data, length);
	
	return sizeof(length) + length;
}

size_t Instruction::payloadLength() const
{
	return sizeof(length) + length;
}

/* struct ListBoards */

size_t ListBoards::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	return sizeof(type);
}

size_t ListBoards::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t ListBoards::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	return 0;
}

size_t ListBoards::payloadLength() const
{
	return 0;
}

/* struct Cancel */

size_t Cancel::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	return sizeof(type);
}

size_t Cancel::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type);
}

size_t Cancel::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	return 0;
}

size_t Cancel::payloadLength() const
{
	return 0;
}

/* struct UserInfo */

size_t UserInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&user_id, buf+i, sizeof(user_id)); i += sizeof(user_id);
	memcpy(&mode, buf+i, sizeof(mode)); i += sizeof(mode);
	memcpy(&event, buf+i, sizeof(event)); i += sizeof(event);
	memcpy(&length, buf+i, sizeof(length)); i += sizeof(length);
	
	name = new char[length];
	memcpy(name, buf+i, length); i += length;
	
	return i;
}

size_t UserInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	size_t off = sizeof(type) + sizeof(mode) + sizeof(event);
	if (len < off + sizeof(length))
		return off + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy(&rlen, buf+off, sizeof(length));
		
		return off + sizeof(length) + rlen;
	}
}

size_t UserInfo::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &mode, sizeof(mode)); size_t i = sizeof(mode);
	memcpy(buf+i, &event, sizeof(event)); i += sizeof(event);
	memcpy(buf+i, &length, sizeof(length)); i+= sizeof(length);
	memcpy(buf+i, name, length); i += length;
	
	return i;
}

size_t UserInfo::payloadLength() const
{
	return sizeof(mode) + sizeof(event) + sizeof(length) + length;
}

/* struct HostInfo */

size_t HostInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&boards, buf+i, sizeof(boards)); i += sizeof(boards);
	memcpy(&boardLimit, buf+i, sizeof(boardLimit)); i += sizeof(boardLimit);
	memcpy(&users, buf+i, sizeof(users)); i += sizeof(users);
	memcpy(&userLimit, buf+i, sizeof(userLimit)); i += sizeof(userLimit);
	memcpy(&nameLenLimit, buf+i, sizeof(nameLenLimit)); i += sizeof(nameLenLimit);
	memcpy(&maxSubscriptions, buf+i, sizeof(maxSubscriptions)); i += sizeof(maxSubscriptions);
	memcpy(&requirements, buf+i, sizeof(requirements)); i += sizeof(requirements);
	memcpy(&extensions, buf+i, sizeof(extensions)); i += sizeof(extensions);
	
	return i;
}

size_t HostInfo::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + payloadLength();
}

size_t HostInfo::serializePayload(char *buf) const
{
	assert(buf != 0);

	memcpy(buf, &boards, sizeof(boards)); size_t i = sizeof(boards);
	memcpy(buf+i, &boardLimit, sizeof(boardLimit)); i += sizeof(boardLimit);
	memcpy(buf+i, &users, sizeof(users)); i += sizeof(users);
	memcpy(buf+i, &userLimit, sizeof(userLimit)); i += sizeof(userLimit);
	memcpy(buf+i, &nameLenLimit, sizeof(nameLenLimit)); i += sizeof(nameLenLimit);
	memcpy(buf+i, &maxSubscriptions, sizeof(maxSubscriptions)); i += sizeof(maxSubscriptions);
	memcpy(buf+i, &requirements, sizeof(requirements)); i += sizeof(requirements);
	memcpy(buf+i, &extensions, sizeof(extensions)); i += sizeof(extensions);
	
	return i;
}

size_t HostInfo::payloadLength() const
{
	return sizeof(boards) + sizeof(boardLimit) + sizeof(users)
		+ sizeof(userLimit) + sizeof(nameLenLimit) + sizeof(maxSubscriptions)
		+ sizeof(requirements) + sizeof(extensions);
}

/* struct BoardInfo */

size_t BoardInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	memcpy(&identifier, buf+i, sizeof(identifier)); i += sizeof(identifier);
	
	bswap_mem(width, buf+i); i += sizeof(width);
	bswap_mem(height, buf+i); i += sizeof(width);
	
	memcpy(&owner, buf+i, sizeof(owner)); i += sizeof(owner);
	memcpy(&users, buf+i, sizeof(users)); i += sizeof(users);
	memcpy(&limit, buf+i, sizeof(limit)); i += sizeof(limit);
	
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
		
		memcpy(&rlen, buf+p, sizeof(length));
		
		return p + sizeof(length) + rlen;
		// return p + sizeof(length) + bswap_mem_t(buf+p, length));
	}
}

size_t BoardInfo::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &identifier, sizeof(identifier)); size_t i = sizeof(identifier);
	memcpy(buf+i, &bswap(width), sizeof(width)); i += sizeof(width);
	memcpy(buf+i, &bswap(height), sizeof(height)); i += sizeof(height);
	memcpy(buf+i, &owner, sizeof(owner)); i += sizeof(owner);
	memcpy(buf+i, &users, sizeof(users)); i += sizeof(users);
	memcpy(buf+i, &limit, sizeof(limit)); i += sizeof(limit);
	memcpy(buf+i, &length, sizeof(length)); i += sizeof(length);
	memcpy(buf+i, name, length); i += length;

	return i;
}

size_t BoardInfo::payloadLength() const
{
	return sizeof(identifier) + sizeof(width) + sizeof(height) + sizeof(owner)
		+ sizeof(users) + sizeof(limit) + sizeof(limit) + length;
}

/* struct Acknowledgement */

size_t Acknowledgement::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	memcpy(&event, buf+sizeof(type), sizeof(event));
	
	return sizeof(type) + sizeof(event);
}

size_t Acknowledgement::reqDataLen(const char *buf, size_t len) const
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	
	return sizeof(type) + sizeof(event);
}

size_t Acknowledgement::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &event, sizeof(event));
	
	return sizeof(event);
}

size_t Acknowledgement::payloadLength() const
{
	return sizeof(event);
}

/* struct Error */

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

size_t Error::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &bswap(code), sizeof(code));
	
	return sizeof(code);
}

size_t Error::payloadLength() const
{
	return sizeof(code);
}

/* struct Deflate */

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

size_t Deflate::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &bswap(uncompressed), sizeof(uncompressed)); size_t i = sizeof(uncompressed);
	memcpy(buf+i, &bswap(length), sizeof(length)); i += sizeof(length);
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Deflate::payloadLength() const
{
	return sizeof(uncompressed) + sizeof(length) + length;
}

/* struct Chat */

size_t Chat::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&board_id, buf+i, sizeof(board_id)); i += sizeof(board_id);
	memcpy(&length, buf+i, sizeof(length)); i += sizeof(length);
	
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
		
		memcpy(&rlen, buf+sizeof(type)+sizeof(board_id), sizeof(length));
		
		return sizeof(type) + sizeof(board_id) + sizeof(length) + rlen;
	}
}

size_t Chat::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &board_id, sizeof(board_id)); size_t i = sizeof(board_id);
	memcpy(buf+i, &length, sizeof(length)); i += sizeof(length);
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Chat::payloadLength() const
{
	return sizeof(board_id) + sizeof(length) + length;
}

/* struct Palette */

size_t Palette::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	memcpy(&offset, buf+i, sizeof(offset)); i += sizeof(offset);
	memcpy(&count, buf+i, sizeof(count)); i += sizeof(count);
	
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
		uint8_t rcount;
		
		memcpy(&rcount, buf+sizeof(type)+sizeof(offset), sizeof(count));
		
		return sizeof(type) + sizeof(offset) + sizeof(count) + rcount*RGB_size;
	}
}

size_t Palette::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	memcpy(buf, &offset, sizeof(offset)); size_t i = sizeof(offset);
	memcpy(buf+i, &count, sizeof(count)); i += sizeof(count);
	memcpy(buf+i, data, count*RGB_size); i += count*RGB_size;
	
	return i;
}

size_t Palette::payloadLength() const
{
	return sizeof(offset) + sizeof(count) + count * RGB_size;
}

} // namespace protocol

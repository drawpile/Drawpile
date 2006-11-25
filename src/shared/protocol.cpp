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
#include "sockets.h" // htonl(), ntohs(), etc.

#include <cassert> // assert()
#include <memory> // memcpy()

namespace protocol {
	
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
		length = 2 + isUser?1:0;
	}
	else
	{
		// If messages are not bundled, simply concatenate whole messages
		headerlen = 1 + isUser?1:0;
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

// Identifier payload serialization
size_t Identifier::serializePayload(char *buffer) const
{
	uint32_t tVer = htons(version);
	uint32_t tLvl = htons(level);
	
	size_t i = identifier_size;
	memcpy(buffer, &identifier, identifier_size);
	memcpy(buffer+i, &tVer, sizeof(version)); i += sizeof(version);
	memcpy(buffer+i, &tLvl, sizeof(level)); i += sizeof(level);
	memcpy(buffer+i, &flags, sizeof(flags)); i += sizeof(flags);
	memcpy(buffer+i, &extensions, sizeof(extensions)); i += sizeof(extensions);
	
	return i;
}

size_t Identifier::payloadLength() const
{
	return identifier_size + sizeof(version) + sizeof(flags) + sizeof(extensions);
}

void Identifier::unserialize(const char* buf, size_t len)
{
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = identifier_size;
	memcpy(&identifier, buf, identifier_size);
	memcpy(&version, buf+i, sizeof(version)); i += sizeof(version);
	memcpy(&level, buf+i, sizeof(level)); i += sizeof(level);
	memcpy(&flags, buf+i, sizeof(flags)); i += sizeof(flags);
	memcpy(&extensions, buf+i, sizeof(extensions));
	
	version = ntohs(version);
	level = ntohs(level);
}

size_t Identifier::reqDataLen(const char *buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type::Identifier);
	
	return (1 + identifier_size + sizeof(uint32_t) + sizeof(uint8_t) * 2);
}

/* Stroke info serialization */

size_t StrokeInfo::serializePayload(char *buf) const
{
	assert(buf != 0);
	
	uint16_t
		nx = htons(x),
		ny = htons(y);
	
	memcpy(buf, &nx, sizeof(x)); size_t size = sizeof(x);
	memcpy(buf+size, &ny, sizeof(y)); size += sizeof(y);
	memcpy(buf+4, &pressure, sizeof(pressure)); size += sizeof(pressure);
	
	return size;
}

size_t StrokeInfo::payloadLength() const
{
	return 5;
}

void StrokeInfo::unserialize(const char* buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type);
	assert(reqDataLen(buf, len) <= len);
	
	uint8_t count;
	
	size_t i = 1;
	memcpy(&user_id, buf+i, sizeof(user_id)); i += sizeof(user_id);
	memcpy(&count, buf+i, sizeof(count)); i += sizeof(count);
	memcpy(&x, buf+i, sizeof(x)); i += sizeof(x);
	memcpy(&y, buf+i, sizeof(y)); i += sizeof(y);
	memcpy(&pressure, buf+i, sizeof(pressure));
	
	assert(count != 0);
	
	x = ntohs(x);
	y = ntohs(y);
}

size_t StrokeInfo::reqDataLen(const char *buf, size_t len)
{
	assert(buf != 0 && len != 0);
	assert(buf[0] == type::StrokeInfo);
	
	if (len < 3)
		return 3;
	else
		return (3 + buf[2] * 5);
}


} // namespace protocol

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

#include "protocol.h"

#ifndef NDEBUG
#include <iostream>
#endif // NDEBUG

#include <limits>
#include <cassert> // assert()
#include <memory> // memcpy()

#if defined(HAVE_BOOST)
#include <boost/static_assert.hpp>
using namespace boost;
#endif

#include "protocol.errors.h"
#include "templates.h"

namespace protocol {

/*
 * struct Message
 */

//
size_t Message::serializeHeader(char* ptr, const Message* msg) const throw()
{
	size_t i = 0;
	
	memcpy_t(ptr, msg->type); i += sizeof(msg->type);
	
	if (fIsSet(msg->modifiers, message::isUser))
	{
		memcpy_t(ptr+i, msg->user_id);
		i += sizeof(msg->user_id);
	}
	
	if (fIsSet(msg->modifiers, message::isSession))
	{
		memcpy_t(ptr+i, msg->session_id);
		i += sizeof(msg->session_id);
	}
	
	return i;
}

//
size_t Message::unserializeHeader(const char* ptr) throw()
{
	size_t i = sizeof(type);
	
	if (fIsSet(modifiers, message::isUser))
	{
		memcpy_t(user_id, ptr+i);
		i += sizeof(user_id);
	}
	
	if (fIsSet(modifiers, message::isSession))
	{
		memcpy_t(session_id, ptr+i);
		i += sizeof(session_id);
	}
	
	return i;
}

//
size_t Message::headerSize() const throw()
{
	return sizeof(type) + (fIsSet(modifiers, message::isUser)?sizeof(user_id):0)
		+ (fIsSet(modifiers, message::isSession)?sizeof(session_id):0);
}

// Base serialization
char *Message::serialize(size_t &len) const throw(std::bad_alloc)
{
	// This _must_ be the last message in bundle.
	assert(next == 0);
	
	size_t
		headerlen,
		length = headerSize();
	
	if (fIsSet(modifiers, message::isBundling))
	{
		// If we are bundling packets, there will be no extra headers
		headerlen = 0;
		length += sizeof(null_count);
	}
	else
	{
		// If messages are not bundled, simply concatenate whole messages
		headerlen = headerSize();
	}
	
	// At least one message is serialized (the last)
	length += payloadLength();
	
	uint8_t count = 1;
	
	// first message in bundle
	const Message *ptr = this;
	// Count number of messages to serialize and required size
	if (prev)
	{
		do
		{
			// ptr->prev should not point to ptr
			assert(ptr != ptr->prev);
			assert(count <= std::numeric_limits<uint8_t>::max());
			
			ptr = ptr->prev;
			++count;
			length += headerlen + ptr->payloadLength();
		}
		while (ptr->prev);
	}
	// ptr now points to the first message in list.
	
	assert(ptr != 0); // some problems with the constness?
	
	// Allocate memory and serialize.
	char *data = new char[length];
	char *dataptr = data;
	len = length;
	
	switch (fIsSet(modifiers, message::isBundling))
	{
	case true:
		// Write bundled packets
		dataptr += serializeHeader(dataptr, ptr);
		memcpy_t(dataptr++, count);
		while (ptr)
		{
			dataptr += ptr->serializePayload(dataptr);
			ptr = ptr->next;
		}
		break;
	default: // case false:
		// Write whole packets
		while (ptr)
		{
			dataptr += serializeHeader(dataptr, ptr);
			dataptr += ptr->serializePayload(dataptr);
			ptr = ptr->next;
		}
		break;
	}
	
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

size_t Message::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize();
}

size_t Message::unserialize(const char* buf, size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = sizeof(type);
	
	if (fIsSet(modifiers, message::isUser))
	{
		memcpy_t(user_id, buf+i);
		i += sizeof(user_id);
	}
	
	if (fIsSet(modifiers, message::isSession))
	{
		memcpy_t(session_id, buf+i);
		i += sizeof(session_id);
	}
	
	return i;
}

int Message::isValid() const throw()
{
	return true;
}

/*
 * struct Identifier
 */

size_t Identifier::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, identifier, identifier_size); size_t i = identifier_size;
	
	uint16_t rev_t = revision, lvl_t = level;
	
	memcpy_t(buf+i, bswap(rev_t)); i += sizeof(revision);
	memcpy_t(buf+i, bswap(lvl_t)); i += sizeof(level);
	memcpy_t(buf+i, flags); i += sizeof(flags);
	memcpy_t(buf+i, extensions); i += sizeof(extensions);
	
	return i;
}

size_t Identifier::payloadLength() const throw()
{
	return identifier_size + sizeof(revision) + sizeof(level)
		+ sizeof(flags) + sizeof(extensions);
}

size_t Identifier::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(identifier, buf+i, identifier_size); i += identifier_size;
	
	memcpy_t(revision, buf+i); i += sizeof(revision);
	memcpy_t(level, buf+i); i += sizeof(level);
	memcpy_t(flags, buf+i); i += sizeof(flags);
	memcpy_t(extensions, buf+i); i += sizeof(extensions);
	
	bswap(revision);
	bswap(level);
	
	return i;
}

size_t Identifier::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

/*
 * struct StrokeInfo
 */

size_t StrokeInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	uint16_t
		x_tmp = x,
		y_tmp = y;
	
	memcpy_t(buf, bswap(x_tmp)); size_t i = sizeof(x);
	memcpy_t(buf+i, bswap(y_tmp)); i += sizeof(y);
	memcpy_t(buf+i, pressure); i += sizeof(pressure);
	
	return i;
}

size_t StrokeInfo::payloadLength() const throw()
{
	return sizeof(x) + sizeof(y) + sizeof(pressure);
}

size_t StrokeInfo::unserialize(const char* buf, size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	uint8_t uid = user_id;
	
	uint8_t count, ucount=0;
	memcpy_t(count, buf+i); i += sizeof(count);
	
	if (count == 0)
		throw scrambled_buffer();
	
	StrokeInfo *ptr = this;
	Message* last = 0;
	do
	{
		ptr->user_id = uid;
		
		// make sure we aren't overflowing the buffer
		assert((i+sizeof(x)+sizeof(y)+sizeof(pressure)) <= len);
		
		// extract data
		memcpy_t(ptr->x, buf+i); i += sizeof(x);
		memcpy_t(ptr->y, buf+i); i += sizeof(y);
		memcpy_t(ptr->pressure, buf+i); i += sizeof(pressure);
		
		// swap coords
		bswap(ptr->x);
		bswap(ptr->y);
		
		// increment count
		++ucount;
		
		if (ucount != count)
		{
			last = ptr;
			ptr = new StrokeInfo;
			last->next = ptr;
			ptr->prev = last;
		}
	}
	while (ucount != count);
	
	return i;
}

size_t StrokeInfo::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	size_t h = headerSize() + sizeof(null_count);
	
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

size_t ToolInfo::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(tool_id, buf+i); i += sizeof(tool_id);
	memcpy_t(mode, buf+i); i += sizeof(mode);
	
	memcpy_t(lo_color, buf+i); i += sizeof(lo_color);
	memcpy_t(hi_color, buf+i); i += sizeof(hi_color);
	
	memcpy_t(lo_size, buf+i); i += sizeof(lo_size);
	memcpy_t(hi_size, buf+i); i += sizeof(hi_size);
	memcpy_t(lo_hardness, buf+i); i += sizeof(lo_hardness);
	memcpy_t(hi_hardness, buf+i); i += sizeof(hi_hardness);
	
	return i;
}

size_t ToolInfo::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t ToolInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	assert(tool_id != tool_type::None);
	
	memcpy_t(buf, tool_id); size_t i = sizeof(tool_id);
	memcpy_t(buf+i, mode); i += sizeof(mode);
	
	memcpy_t(buf+i, lo_color); i += sizeof(lo_color);
	memcpy_t(buf+i, hi_color); i += sizeof(hi_color);
	
	memcpy_t(buf+i, lo_size); i += sizeof(lo_size);
	memcpy_t(buf+i, hi_size); i += sizeof(hi_size);
	memcpy_t(buf+i, lo_hardness); i += sizeof(lo_hardness);
	memcpy_t(buf+i, hi_hardness); i += sizeof(hi_hardness);
	
	return i;
}

size_t ToolInfo::payloadLength() const throw()
{
	return sizeof(tool_id) + sizeof(mode) + sizeof(lo_color)
		+ sizeof(hi_color) + sizeof(lo_size) + sizeof(hi_size)
		+ sizeof(lo_hardness) + sizeof(hi_hardness);
}

/*
 * struct Synchronize
 */

// nothing needed

/*
 * struct Raster
 */

size_t Raster::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
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

size_t Raster::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	size_t off = headerSize() + sizeof(offset) + sizeof(length) + sizeof(size);
	
	if (len < off)
		return off;
	else
	{
		uint32_t tmp;
		memcpy_t(tmp, buf+headerSize()+sizeof(offset));
		return off + bswap(tmp);
	}
}

size_t Raster::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	uint32_t off_tmp = offset, len_tmp = length, size_tmp = size;
	
	memcpy_t(buf, bswap(off_tmp)); size_t i = sizeof(offset);
	memcpy_t(buf+i, bswap(len_tmp)); i += sizeof(length);
	memcpy_t(buf+i, bswap(size_tmp)); i += sizeof(size);
	
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

// nothing needed

/*
 * struct Authentication
 */

size_t Authentication::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(seed, buf+i, password_seed_size); i += password_seed_size;
	
	return i;
}

size_t Authentication::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + password_seed_size;
}

size_t Authentication::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, seed, password_seed_size);
	
	return password_seed_size;
}

size_t Authentication::payloadLength() const throw()
{
	return password_seed_size;
}

/*
 * struct Password
 */

size_t Password::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(data, buf+i, password_hash_size); i += password_hash_size;
	
	return i;
}

size_t Password::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + password_hash_size;
}

size_t Password::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, data, password_hash_size);
	
	return password_hash_size;
}

size_t Password::payloadLength() const throw()
{
	return password_hash_size;
}

/*
 * struct Subscribe
 */

// nothing needed

/*
 * struct Unsubscribe
 */

// nothing needed

/*
 * struct Instruction
 */

size_t Instruction::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(command, buf+i); i += sizeof(command);
	memcpy_t(aux_data, buf+i); i += sizeof(aux_data);
	memcpy_t(aux_data2, buf+i); i += sizeof(aux_data2);
	
	memcpy_t(length, buf+i); i += sizeof(length);
	
	data = new char[length];
	memcpy(data, buf+i, length); i += length;
	
	return i;
}

size_t Instruction::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	size_t off = headerSize() + sizeof(command)
		+ sizeof(aux_data) + sizeof(aux_data2);
	
	if (len < off)
		return off + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+off);
		
		return off + sizeof(length) + rlen;
	}
}

size_t Instruction::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	size_t i=0;
	
	memcpy_t(buf+i, command); i += sizeof(command);
	memcpy_t(buf+i, aux_data); i += sizeof(aux_data);
	memcpy_t(buf+i, aux_data2); i += sizeof(aux_data2);
	
	memcpy_t(buf+i, length); i += sizeof(length);
	
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Instruction::payloadLength() const throw()
{
	return sizeof(command)
		+ sizeof(aux_data) + sizeof(aux_data2) + sizeof(length) + length;
}

/*
 * struct ListSessions
 */

// no special implementation required

/*
 * struct Cancel
 */

// nothing needed

/*
 * struct UserInfo
 */

size_t UserInfo::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(mode, buf+i); i += sizeof(mode);
	memcpy_t(event, buf+i); i += sizeof(event);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	name = new char[length+1];
	memcpy(name, buf+i, length); i += length;
	name[length] = '\0';
	
	return i;
}

size_t UserInfo::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	size_t off = headerSize() + sizeof(mode) + sizeof(event);
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
	
	memcpy_t(buf, mode); size_t i = sizeof(mode);
	memcpy_t(buf+i, event); i += sizeof(event);
	memcpy_t(buf+i, length); i += sizeof(length);
	
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

size_t HostInfo::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(sessions, buf+i); i += sizeof(sessions);
	memcpy_t(sessionLimit, buf+i); i += sizeof(sessionLimit);
	memcpy_t(users, buf+i); i += sizeof(users);
	memcpy_t(userLimit, buf+i); i += sizeof(userLimit);
	memcpy_t(nameLenLimit, buf+i); i += sizeof(nameLenLimit);
	memcpy_t(maxSubscriptions, buf+i); i += sizeof(maxSubscriptions);
	memcpy_t(requirements, buf+i); i += sizeof(requirements);
	memcpy_t(extensions, buf+i); i += sizeof(extensions);
	
	return i;
}

size_t HostInfo::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t HostInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);

	memcpy_t(buf, sessions); size_t i = sizeof(sessions);
	memcpy_t(buf+i, sessionLimit); i += sizeof(sessionLimit);
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
	return sizeof(sessions) + sizeof(sessionLimit) + sizeof(users)
		+ sizeof(userLimit) + sizeof(nameLenLimit) + sizeof(maxSubscriptions)
		+ sizeof(requirements) + sizeof(extensions);
}

/*
 * struct SessionInfo
 */

size_t SessionInfo::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(width, buf+i); i += sizeof(width);
	memcpy_t(height, buf+i); i += sizeof(height);
	memcpy_t(owner, buf+i); i += sizeof(owner);
	memcpy_t(users, buf+i); i += sizeof(users);
	memcpy_t(limit, buf+i); i += sizeof(limit);
	memcpy_t(mode, buf+i); i += sizeof(mode);
	memcpy_t(flags, buf+i); i += sizeof(flags);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	title = new char[length+1];
	memcpy(title, buf+i, length); i += length;
	title[length] = '\0';
	
	return i;
}

size_t SessionInfo::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	size_t p = headerSize() + sizeof(width) + sizeof(height)
		+ sizeof(owner) + sizeof(users) + sizeof(mode) + sizeof(flags) + sizeof(limit);
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

size_t SessionInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, width); size_t i = sizeof(width);
	memcpy_t(buf+i, height); i += sizeof(height);
	memcpy_t(buf+i, owner); i += sizeof(owner);
	memcpy_t(buf+i, users); i += sizeof(users);
	memcpy_t(buf+i, limit); i += sizeof(limit);
	memcpy_t(buf+i, mode); i += sizeof(mode);
	memcpy_t(buf+i, flags); i += sizeof(flags);
	memcpy_t(buf+i, length); i += sizeof(length);
	
	memcpy(buf+i, title, length); i += length;

	return i;
}

size_t SessionInfo::payloadLength() const throw()
{
	return sizeof(width) + sizeof(height) + sizeof(owner)
		+ sizeof(users) + sizeof(limit) + sizeof(mode) + sizeof(flags)
		+ sizeof(length) + length;
}

/*
 * struct Acknowledgement
 */

size_t Acknowledgement::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(event, buf+i);
	
	return i + sizeof(event);
}

size_t Acknowledgement::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
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

size_t Error::unserialize(const char* buf, size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(code, buf+i);
	
	return i + sizeof(code);
}

size_t Error::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
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

size_t Deflate::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(uncompressed, buf+i); i += sizeof(uncompressed);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	bswap(uncompressed);
	bswap(length);
	
	data = new char[length];
	memcpy(data, buf+i, length);  i += length;
	
	return i;
}

size_t Deflate::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	if (len < headerSize() + sizeof(uncompressed) + sizeof(length))
		return headerSize() + sizeof(uncompressed) + sizeof(length);
	else
	{
		uint16_t rlen;
		
		memcpy_t(rlen, buf+headerSize()+sizeof(uncompressed));
		bswap(rlen);
		
		return headerSize() + sizeof(uncompressed) + sizeof(length) + rlen;
	}
}

size_t Deflate::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	uint16_t unc_t = uncompressed, len_t = length;
	
	memcpy_t(buf, bswap(unc_t)); size_t i = sizeof(uncompressed);
	memcpy_t(buf+i, bswap(len_t)); i += sizeof(length);
	
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

size_t Chat::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(length, buf+i); i += sizeof(length);
	
	data = new char[length+1];
	memcpy(data, buf+i, length); i += length;
	data[length] = '\0';
	
	return i;
}

size_t Chat::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	if (len < headerSize() + sizeof(length))
		return headerSize() + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+headerSize());
		
		return headerSize() + sizeof(length) + rlen;
	}
}

size_t Chat::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, length); size_t i = sizeof(length);
	
	memcpy(buf+i, data, length); i += length;
	
	return i;
}

size_t Chat::payloadLength() const throw()
{
	return sizeof(length) + length;
}

/*
 * struct Palette
 */

size_t Palette::unserialize(const char* buf, size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(offset, buf+i); i += sizeof(offset);
	memcpy_t(count, buf+i); i += sizeof(count);
	
	data = new char[count*RGB_size];
	memcpy(data, buf+i, count*RGB_size); i += count*RGB_size;
	
	return i;
}

size_t Palette::reqDataLen(const char *buf, size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	if (len < headerSize() + sizeof(offset) + sizeof(count))
		return headerSize() + sizeof(offset) + sizeof(count);
	else
	{
		uint8_t tmp;
		memcpy_t(tmp, buf+headerSize()+sizeof(offset));
		return headerSize() + sizeof(offset) + sizeof(count)
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

/*
 * struct SessionSelect
 */

// nothing needed

} // namespace protocol

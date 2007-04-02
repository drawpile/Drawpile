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

size_t Message::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type);
	
	return sizeof(type);
}

size_t Message::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	return sizeof(type);
}

size_t Message::headerSize() const throw()
{
	return sizeof(type);
}

// Base serialization
char* Message::serialize(size_t &length, char* data, size_t &size) const throw(std::bad_alloc)
{
	// This _must_ be the last message in bundle.
	assert(next == 0);
	
	#ifndef NDEBUG
	if (data == 0)
		assert(size == 0);
	if (size == 0)
		assert(data == 0);
	#endif
	
	// At least one message is serialized (the last)
	length += payloadLength();
	
	uint8_t count = 1;
	
	// first message in bundle
	const Message *ptr = this;
	
	// Count number of messages to serialize and required size
	while (ptr->prev != 0)
	{
		assert(ptr != ptr->prev); // infinite loop
		assert(count != std::numeric_limits<uint8_t>::max()); // overflow
		
		ptr = ptr->prev;
		
		length += ptr->payloadLength();
		
		++count;
	}
	
	// headers
	if (isBundling)
		length += headerSize() + sizeof(null_count);
	else
		length += headerSize() * count;
	
	// Allocate memory if necessary
	if (size < length)
	{
		data = new char[length];
		size = length;
	}
	
	// 'iterator'
	char *dataptr = data;
	
	if (isBundling)
	{
		// Write bundled packets
		dataptr += serializeHeader(dataptr);
		memcpy_t(dataptr++, count);
		do
		{
			dataptr += ptr->serializePayload(dataptr);
		}
		while (ptr = ptr->next);
	}
	else
	{
		// Write whole packets
		do
		{
			dataptr += serializeHeader(dataptr);
			dataptr += ptr->serializePayload(dataptr);
		}
		while (ptr = ptr->next);
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

size_t Message::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize();
}

size_t Message::unserialize(const char* buf, const size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	return unserializeHeader(buf);
}

bool Message::isValid() const throw()
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

size_t Identifier::unserialize(const char* buf, const size_t len) throw()
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

size_t Identifier::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

bool Identifier::isValid() const throw()
{
	// TODO
	const bool r1 = true; // flags
	const bool r2 = true; // extensions
	return (r1 and r2 and memcmp(identifier, identifier_string, 8) == 0);
}

/*
 * struct StrokeInfo
 */

size_t StrokeInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	uint16_t tmp;
	
	tmp = x;
	memcpy_t(buf, bswap(tmp));
	tmp = y;
	memcpy_t(buf+sizeof(x), bswap(tmp));
	memcpy_t(buf+sizeof(x)+sizeof(y), pressure);
	
	return payloadLength();
}

size_t StrokeInfo::payloadLength() const throw()
{
	return sizeof(x) + sizeof(y) + sizeof(pressure);
}

size_t StrokeInfo::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	return i;
}

size_t StrokeInfo::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	return i;
}

size_t StrokeInfo::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id);
}

size_t StrokeInfo::unserialize(const char* buf, const size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	uint8_t count;
	memcpy_t(count, buf+i); i += sizeof(count);
	
	if (count == 0)
		throw std::exception(); // TODO: Need better exception
	
	// make sure we aren't overflowing the buffer
	assert(i + payloadLength() <= len);
	
	// extract data
	memcpy_t(x, buf+i),
	memcpy_t(y, buf+(i+=sizeof(x))),
	memcpy_t(pressure, buf+(i+=sizeof(y)));
	i += sizeof(pressure);
	
	// swap coords
	bswap(x),
	bswap(y);
	
	if (count != 1)
	{
		--count; // remove first from count
		
		StrokeInfo *ptr;
		Message* last = this;
		
		do
		{
			// make sure we aren't overflowing the buffer
			assert(i + payloadLength() <= len);
			
			ptr = new StrokeInfo;
			
			// extract data to temporaries
			memcpy_t(ptr->x, buf+i),
			memcpy_t(ptr->y, buf+i+sizeof(x)),
			memcpy_t(ptr->pressure, buf+i+sizeof(y)+sizeof(x));
			i += sizeof(y)+sizeof(x)+sizeof(pressure);
			
			// swap coords
			bswap(ptr->x), bswap(ptr->y);
			
			// set user ID
			ptr->user_id = user_id;
			
			// create link from previous to the new..
			ptr->prev = last;
			last = last->next = ptr;
		}
		while (--count != 0);
	}
	
	return i;
}

size_t StrokeInfo::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(null_count);
	
	if (len < off + payloadLength())
		return off + payloadLength();
	else
		return off + buf[2] * payloadLength();
}

/*
 * struct StrokeEnd
 */

size_t StrokeEnd::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	return i;
}

size_t StrokeEnd::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	return i;
}

size_t StrokeEnd::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id);
}

/*
 * struct ToolInfo
 */

size_t ToolInfo::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(tool_id, buf+i); i += sizeof(tool_id);
	memcpy_t(mode, buf+i); i += sizeof(mode);
	
	memcpy(lo_color, buf+i, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(hi_color, buf+i, sizeof(hi_color)); i += sizeof(hi_color);
	
	memcpy_t(lo_size, buf+i); i += sizeof(lo_size);
	memcpy_t(hi_size, buf+i); i += sizeof(hi_size);
	memcpy_t(lo_hardness, buf+i); i += sizeof(lo_hardness);
	memcpy_t(hi_hardness, buf+i); i += sizeof(hi_hardness);
	memcpy_t(spacing, buf+i); i += sizeof(spacing);
	
	return i;
}

size_t ToolInfo::reqDataLen(const char *buf, const size_t len) const throw()
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
	
	memcpy(buf+i, lo_color, sizeof(lo_color)); i += sizeof(lo_color);
	memcpy(buf+i, hi_color, sizeof(hi_color)); i += sizeof(hi_color);
	
	memcpy_t(buf+i, lo_size); i += sizeof(lo_size);
	memcpy_t(buf+i, hi_size); i += sizeof(hi_size);
	memcpy_t(buf+i, lo_hardness); i += sizeof(lo_hardness);
	memcpy_t(buf+i, hi_hardness); i += sizeof(hi_hardness);
	memcpy_t(buf+i, spacing); i += sizeof(spacing);
	
	return i;
}

size_t ToolInfo::payloadLength() const throw()
{
	return sizeof(tool_id) + sizeof(mode) + sizeof(lo_color)
		+ sizeof(hi_color) + sizeof(lo_size) + sizeof(hi_size)
		+ sizeof(lo_hardness) + sizeof(hi_hardness) + sizeof(spacing);
}

size_t ToolInfo::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	return i;
}

size_t ToolInfo::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	return i;
}

size_t ToolInfo::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id);
}

bool ToolInfo::isValid() const throw()
{
	// TODO
	const bool r1 = true; // tool ID
	const bool r2 = true; // composition mode
	return (r1 and r2);
}

/*
 * struct Synchronize
 */

size_t Synchronize::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Synchronize::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Synchronize::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Raster
 */

size_t Raster::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
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
	
	if (length != 0)
	{
		data = new char[length];
		memcpy(data, buf+i, length); i += length;
	}
	else
		data = 0;
	
	return i;
}

size_t Raster::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(offset) + sizeof(length) + sizeof(size);
	
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
	
	if (length != 0)
	{
		memcpy(buf+i, data, length); i += length;
	}
	
	return i;
}

size_t Raster::payloadLength() const throw()
{
	return sizeof(offset) + sizeof(length) + sizeof(size) + length;
}

size_t Raster::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Raster::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Raster::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool Raster::isValid() const throw()
{
	return (offset+length <= size);
}

/*
 * struct SyncWait
 */

size_t SyncWait::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t SyncWait::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t SyncWait::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Authentication
 */

size_t Authentication::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(seed, buf+i, password_seed_size); i += password_seed_size;
	
	return i;
}

size_t Authentication::reqDataLen(const char *buf, const size_t len) const throw()
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

size_t Authentication::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Authentication::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Authentication::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Password
 */

size_t Password::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(data, buf+i, password_hash_size); i += password_hash_size;
	
	return i;
}

size_t Password::reqDataLen(const char *buf, const size_t len) const throw()
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

size_t Password::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Password::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Password::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Subscribe
 */

size_t Subscribe::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Subscribe::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Subscribe::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Unsubscribe
 */

size_t Unsubscribe::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Unsubscribe::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Unsubscribe::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct Instruction
 */

size_t Instruction::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(command, buf+i); i += sizeof(command);
	memcpy_t(aux_data, buf+i); i += sizeof(aux_data);
	memcpy_t(aux_data2, buf+i); i += sizeof(aux_data2);
	
	memcpy_t(length, buf+i); i += sizeof(length);
	
	if (length != 0)
	{
		data = new char[length];
		memcpy(data, buf+i, length); i += length;
	}
	else
		data = 0;
	
	return i;
}

size_t Instruction::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(command)
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
	
	if (length != 0)
	{
		memcpy(buf+i, data, length); i += length;
	}
	
	return i;
}

size_t Instruction::payloadLength() const throw()
{
	return sizeof(command)
		+ sizeof(aux_data) + sizeof(aux_data2) + sizeof(length) + length;
}

size_t Instruction::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Instruction::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Instruction::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

bool Instruction::isValid() const throw()
{
	// TODO
	switch (command)
	{
	case 3:
	case 2:
	case 1:
		return true;
	default:
		return false;
	}
}

/*
 * struct ListSessions
 */

// no special implementation required

/*
 * struct Cancel
 */

size_t Cancel::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Cancel::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Cancel::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

/*
 * struct UserInfo
 */

size_t UserInfo::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(mode, buf+i); i += sizeof(mode);
	memcpy_t(event, buf+i); i += sizeof(event);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	if (length != 0)
	{
		name = new char[length+1];
		memcpy(name, buf+i, length); i += length;
		name[length] = '\0';
	}
	else
		name = 0;
	
	return i;
}

size_t UserInfo::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(mode) + sizeof(event);
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
	
	if (length != 0)
	{
		memcpy(buf+i, name, length); i += length;
	}
	
	return i;
}

size_t UserInfo::payloadLength() const throw()
{
	return sizeof(mode) + sizeof(event) + sizeof(length) + length;
}

size_t UserInfo::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t UserInfo::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t UserInfo::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

bool UserInfo::isValid() const throw()
{
	// TODO
	const bool r1 = true; // mode
	bool r2 = true;
	switch (event)
	{
	case 3:
	case 2:
	case 1:
		r2 = true;
		break;
	default:
		r2 = false;
		break;
	}
	return (r1 and r2);
}

/*
 * struct HostInfo
 */

size_t HostInfo::unserialize(const char* buf, const size_t len) throw()
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

size_t HostInfo::reqDataLen(const char *buf, const size_t len) const throw()
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

bool HostInfo::isValid() const throw()
{
	// TODO
	const bool r1 = true; // extensions
	const bool r2 = true; // requirements
	return (r1 and r2);
}

/*
 * struct SessionInfo
 */

size_t SessionInfo::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
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
	memcpy_t(level, buf+i); i += sizeof(level);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	if (length != 0)
	{
		title = new char[length+1];
		memcpy(title, buf+i, length); i += length;
		title[length] = '\0';
	}
	else
		title = 0;
	
	return i;
}

size_t SessionInfo::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(width) + sizeof(height)
		+ sizeof(owner) + sizeof(users) + sizeof(mode) + sizeof(flags)
		+ sizeof(limit) + sizeof(level);
	if (len < off + sizeof(length))
		return off + sizeof(length);
	else
	{
		uint8_t rlen;
		
		memcpy_t(rlen, buf+off);
		
		return off + sizeof(length) + rlen;
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
	memcpy_t(buf+i, level); i += sizeof(level);
	memcpy_t(buf+i, length); i += sizeof(length);
	
	if (length != 0)
	{
		memcpy(buf+i, title, length); i += length;
	}
	
	return i;
}

size_t SessionInfo::payloadLength() const throw()
{
	return sizeof(width) + sizeof(height) + sizeof(owner)
		+ sizeof(users) + sizeof(limit) + sizeof(mode) + sizeof(flags)
		+ sizeof(level) + sizeof(length) + length;
}

size_t SessionInfo::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionInfo::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionInfo::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool SessionInfo::isValid() const throw()
{
	// TODO
	const bool c1 = true; // mode
	const bool c2 = true; // flags
	return (width != 0 and height != 0 and c1 and c2);
}

/*
 * struct Acknowledgement
 */

size_t Acknowledgement::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(event, buf+i);
	
	return i + sizeof(event);
}

size_t Acknowledgement::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t Acknowledgement::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, event);
	
	return payloadLength();
}

size_t Acknowledgement::payloadLength() const throw()
{
	return sizeof(event);
}

size_t Acknowledgement::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Acknowledgement::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Acknowledgement::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool Acknowledgement::isValid() const throw()
{
	// TODO
	switch (event)
	{
	case 3:
	case 2:
	case 1:
		return true;
	default:
		return false;
	}
}

/*
 * struct Error
 */

size_t Error::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(code, buf+i); i += sizeof(code);
	
	return i;
}

size_t Error::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t Error::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, code);
	
	return payloadLength();
}

size_t Error::payloadLength() const throw()
{
	return sizeof(code);
}

size_t Error::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Error::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Error::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool Error::isValid() const throw()
{
	// TODO
	switch (code)
	{
	case 3:
	case 2:
	case 1:
		return true;
	default:
		return false;
	}
}

/*
 * struct Deflate
 */

size_t Deflate::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(uncompressed, buf+i); i += sizeof(uncompressed);
	memcpy_t(length, buf+i); i += sizeof(length);
	
	bswap(uncompressed);
	bswap(length);
	
	if (length != 0)
	{
		data = new char[length];
		memcpy(data, buf+i, length);  i += length;
	}
	else
		data = 0;
	
	return i;
}

size_t Deflate::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(uncompressed);
	if (len < off + sizeof(length))
		return off + sizeof(length);
	else
	{
		uint16_t rlen;
		
		memcpy_t(rlen, buf+off);
		bswap(rlen);
		
		return off + sizeof(length) + rlen;
	}
}

size_t Deflate::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	uint16_t unc_t = uncompressed, len_t = length;
	
	memcpy_t(buf, bswap(unc_t)); size_t i = sizeof(uncompressed);
	memcpy_t(buf+i, bswap(len_t)); i += sizeof(length);
	
	if (length != 0)
	{
		memcpy(buf+i, data, length); i += length;
	}
	
	return i;
}

size_t Deflate::payloadLength() const throw()
{
	return sizeof(uncompressed) + sizeof(length) + length;
}

bool Deflate::isValid() const throw()
{
	return (uncompressed != 0 and length != 0);
}

/*
 * struct Chat
 */

size_t Chat::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(length, buf+i); i += sizeof(length);
	
	if (length != 0)
	{
		data = new char[length+1];
		memcpy(data, buf+i, length); i += length;
		data[length] = '\0';
	}
	else
		data = 0;
	
	return i;
}

size_t Chat::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize();
	if (len < off + sizeof(length))
		return off + sizeof(length);
	else
	{
		uint8_t rlen;
		memcpy_t(rlen, buf+off);
		
		return off + sizeof(length) + rlen;
	}
}

size_t Chat::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	assert(data != 0); // protocol violation
	
	memcpy_t(buf, length); size_t i = sizeof(length);
	
	if (length != 0)
	{
		memcpy(buf+i, data, length); i += length;
	}
	
	return i;
}

size_t Chat::payloadLength() const throw()
{
	return sizeof(length) + length;
}

size_t Chat::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Chat::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Chat::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

bool Chat::isValid() const throw()
{
	return (length != 0);
}

/*
 * struct Palette
 */

size_t Palette::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(offset, buf+i); i += sizeof(offset);
	memcpy_t(count, buf+i); i += sizeof(count);
	
	if (count != 0)
	{
		data = new char[count*RGB_size];
		memcpy(data, buf+i, count*RGB_size); i += count*RGB_size;
	}
	else
		data = 0;
	
	return i;
}

size_t Palette::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize() + sizeof(offset);
	if (len < off + sizeof(count))
		return off + sizeof(count);
	else
	{
		uint8_t tmp;
		memcpy_t(tmp, buf+off);
		return off + sizeof(count) + tmp * RGB_size;
	}
}

size_t Palette::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	assert(count != 0); // protocol violation
	
	memcpy_t(buf, offset); size_t i = sizeof(offset);
	memcpy_t(buf+i, count); i += sizeof(count);
	
	if (count != 0)
	{
		memcpy(buf+i, data, count*RGB_size); i += count*RGB_size;
	}
	
	return i;
}

size_t Palette::payloadLength() const throw()
{
	return sizeof(offset) + sizeof(count) + count * RGB_size;
}

size_t Palette::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t Palette::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t Palette::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

bool Palette::isValid() const throw()
{
	return (count != 0);
}

/*
 * struct SessionSelect
 */

size_t SessionSelect::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionSelect::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionSelect::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

/*
 * struct SessionEvent
 */

size_t SessionEvent::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(action, buf+i); i += sizeof(action);
	memcpy_t(target, buf+i); i += sizeof(target);
	memcpy_t(aux, buf+i); i += sizeof(aux);
	
	return i;
}

size_t SessionEvent::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t SessionEvent::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, action); size_t i = sizeof(action);
	memcpy_t(buf+i, target); i += sizeof(target);
	memcpy_t(buf+i, aux); i += sizeof(aux);
	
	return i;
}

size_t SessionEvent::payloadLength() const throw()
{
	return sizeof(action) + sizeof(target) + sizeof(aux);
}

size_t SessionEvent::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionEvent::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t SessionEvent::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool SessionEvent::isValid() const throw()
{
	// TODO: action is the only thing we can test
	return true;
}

/*
 * struct LayerEvent
 */

size_t LayerEvent::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(layer_id, buf+i); i += sizeof(layer_id);
	memcpy_t(action, buf+i); i += sizeof(action);
	memcpy_t(mode, buf+i); i += sizeof(mode);
	memcpy_t(opacity, buf+i); i += sizeof(opacity);
	
	return i;
}

size_t LayerEvent::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + payloadLength();
}

size_t LayerEvent::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, layer_id); size_t i = sizeof(layer_id);
	memcpy_t(buf+i, action); i += sizeof(action);
	memcpy_t(buf+i, mode); i += sizeof(mode);
	memcpy_t(buf+i, opacity); i += sizeof(opacity);
	
	return i;
}

size_t LayerEvent::payloadLength() const throw()
{
	return sizeof(layer_id) + sizeof(action) + sizeof(mode) + sizeof(opacity);
}

size_t LayerEvent::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t LayerEvent::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t LayerEvent::headerSize() const throw()
{
	return sizeof(type) + sizeof(session_id);
}

bool LayerEvent::isValid() const throw()
{
	// TODO: action is the only one we can test
	return true;
}

/*
 * struct LayerSelect
 */

size_t LayerSelect::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(layer_id, buf+i); i += sizeof(layer_id);
	
	return i;
}

size_t LayerSelect::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize() + sizeof(layer_id);
}

size_t LayerSelect::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, layer_id);
	
	return payloadLength();
}

size_t LayerSelect::payloadLength() const throw()
{
	return sizeof(layer_id);
}

size_t LayerSelect::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr, type); size_t i = sizeof(type);
	
	memcpy_t(ptr+i, user_id);
	i += sizeof(user_id);
	
	memcpy_t(ptr+i, session_id);
	i += sizeof(session_id);
	
	return i;
}

size_t LayerSelect::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	size_t i = sizeof(type);
	
	memcpy_t(user_id, ptr+i);
	i += sizeof(user_id);
	
	memcpy_t(session_id, ptr+i);
	i += sizeof(session_id);
	
	return i;
}

size_t LayerSelect::headerSize() const throw()
{
	return sizeof(type) + sizeof(user_id) + sizeof(session_id);
}

} // namespace protocol

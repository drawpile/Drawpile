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
#include <cassert> // assert()

#include "templates.h"
#include "protocol.errors.h"
#include "templates.h"

#ifndef NDEBUG
#include "protocol.tools.h"
#endif

namespace protocol {

/*
 * struct Message
 */

Message::Message(const uint8_t _type, const size_t _header, const uint8_t _flags) throw()
	: headerSize(_header),
	isUser(fIsSet(_flags, static_cast<uint8_t>(protocol::message::isUser))),
	isSession(fIsSet(_flags, static_cast<uint8_t>(protocol::message::isSession))),
	isSelected(fIsSet(_flags, static_cast<uint8_t>(protocol::message::isSelected))),
	isBundling(fIsSet(_flags, static_cast<uint8_t>(protocol::message::isBundling))),
	type(_type), user_id(0), session_id(0),
	next(0),
	prev(0)
{
	assert(_header > 0 and _header <= sizeof(type)+sizeof(user_id)+sizeof(session_id));
	assert(isSelected ? !isSession : true);
	assert(isSession ? !isSelected : true);
}

size_t Message::serializeHeader(char* ptr) const throw()
{
	assert(ptr != 0);
	
	memcpy_t(ptr++, type);
	
	if (isUser)
		memcpy_t(ptr++, user_id); 
	
	if (isSession)
		memcpy_t(ptr++, session_id);
	
	return headerSize;
}

size_t Message::unserializeHeader(const char* ptr) throw()
{
	assert(ptr != 0);
	
	++ptr; // skip type
	
	if (isUser)
		memcpy_t(user_id, ptr++);
	
	if (isSession)
		memcpy_t(session_id, ptr++);
	
	return headerSize;
}

// Base serialization
char* Message::serialize(size_t &length, char* data, size_t &size) const throw(std::bad_alloc)
{
	// This _must_ be the last message in bundle.
	assert(next == 0);
	
	#ifndef NDEBUG
	if (data == 0 or size == 0)
		assert(data == 0 and size == 0);
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
		length += headerSize + sizeof(null_count);
	else
		length += headerSize * count;
	
	// Allocate memory if necessary
	if (size < length)
	{
		size = roundToNext(length, 4096u);
		assert(size >= length);
		data = new char[size];
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
			assert(ptr);
			dataptr += ptr->serializePayload(dataptr);
		}
		while ((ptr = ptr->next) != 0);
	}
	else
	{
		// Write whole packets
		do
		{
			assert(ptr);
			dataptr += serializeHeader(dataptr);
			dataptr += ptr->serializePayload(dataptr);
		}
		while ((ptr = ptr->next) != 0);
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
	
	return headerSize;
}

size_t Message::unserialize(const char* buf, const size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	return unserializeHeader(buf);
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
	
	return headerSize + payloadLength();
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

size_t StrokeInfo::unserialize(const char* buf, const size_t len) throw(std::exception, std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	uint8_t count = *(buf+i);
	
	if (count == 0)
		throw std::exception(); // TODO: Need better exception
	
	i += sizeof(count);
	
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
	
	const size_t off = headerSize + sizeof(null_count);
	
	if (len < off + payloadLength())
		return off + payloadLength();
	else
		return off + buf[2] * payloadLength();
}

/*
 * struct StrokeEnd
 */

// none needed

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
	
	memcpy_t(lo_color.RGBA, buf+i); i += sizeof(lo_color.RGBA);
	memcpy_t(hi_color.RGBA, buf+i); i += sizeof(hi_color.RGBA);
	
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
	
	return headerSize + payloadLength();
}

size_t ToolInfo::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	assert(tool_id != tool_type::None);
	
	memcpy_t(buf, tool_id); size_t i = sizeof(tool_id);
	memcpy_t(buf+i, mode); i += sizeof(mode);
	
	memcpy_t(buf+i, lo_color.RGBA); i += sizeof(lo_color.RGBA);
	memcpy_t(buf+i, hi_color.RGBA); i += sizeof(hi_color.RGBA);
	
	memcpy_t(buf+i, lo_size); i += sizeof(lo_size);
	memcpy_t(buf+i, hi_size); i += sizeof(hi_size);
	memcpy_t(buf+i, lo_hardness); i += sizeof(lo_hardness);
	memcpy_t(buf+i, hi_hardness); i += sizeof(hi_hardness);
	memcpy_t(buf+i, spacing); i += sizeof(spacing);
	
	return i;
}

size_t ToolInfo::payloadLength() const throw()
{
	return sizeof(tool_id) + sizeof(mode) + sizeof(lo_color.RGBA)
		+ sizeof(hi_color.RGBA) + sizeof(lo_size) + sizeof(hi_size)
		+ sizeof(lo_hardness) + sizeof(hi_hardness) + sizeof(spacing);
}

/*
 * struct Synchronize
 */

// none needed

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
	
	const size_t off = headerSize + sizeof(offset) + sizeof(length) + sizeof(size);
	
	if (len < off)
		return off;
	else
	{
		uint32_t tmp;
		memcpy_t(tmp, buf+headerSize+sizeof(offset));
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

/*
 * struct SyncWait
 */

// none needed

/*
 * struct PasswordRequest
 */

size_t PasswordRequest::unserialize(const char* buf, const size_t len) throw()
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy(seed, buf+i, password_seed_size); i += password_seed_size;
	
	return i;
}

size_t PasswordRequest::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	return headerSize + password_seed_size;
}

size_t PasswordRequest::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy(buf, seed, password_seed_size);
	
	return password_seed_size;
}

size_t PasswordRequest::payloadLength() const throw()
{
	return password_seed_size;
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
	
	return headerSize + password_hash_size;
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

// none needed

/*
 * struct Unsubscribe
 */

// none needed

/*
 * struct SessionInstruction
 */

size_t SessionInstruction::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(action, buf+i); i += sizeof(action);
	memcpy_t(width, buf+i); i += sizeof(width);
	memcpy_t(height, buf+i); i += sizeof(height);
	memcpy_t(user_mode, buf+i); i += sizeof(user_mode);
	memcpy_t(user_limit, buf+i); i += sizeof(user_limit);
	memcpy_t(flags, buf+i); i += sizeof(flags);
	memcpy_t(title_len, buf+i); i += sizeof(title_len);
	if (title_len != 0)
	{
		title = new char[title_len];
		memcpy(title, buf+i, title_len); i += title_len;
	}
	
	bswap(height);
	bswap(width);
	
	return i;
}

size_t SessionInstruction::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize + sizeof(action) + sizeof(width) + sizeof(height)
		+ sizeof(user_mode) + sizeof(user_limit) + sizeof(flags);
	
	if (len < off+sizeof(title_len))
		return off+sizeof(title_len);
	else
	{
		uint8_t tlen;
		memcpy_t(tlen, buf+off);
		return off + sizeof(title_len) + tlen;
	}
}

size_t SessionInstruction::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, action); size_t i = sizeof(action);
	
	uint16_t tmp_w=width, tmp_h=height;
	
	memcpy_t(buf+i, bswap(tmp_w)); i += sizeof(width);
	memcpy_t(buf+i, bswap(tmp_h)); i += sizeof(height);
	memcpy_t(buf+i, user_mode); i += sizeof(user_mode);
	memcpy_t(buf+i, user_limit); i += sizeof(user_limit);
	memcpy_t(buf+i, flags); i += sizeof(flags);
	memcpy_t(buf+i, title_len); i += sizeof(title_len);
	memcpy(buf+i, title, title_len); i += title_len;
	
	return i;
}

size_t SessionInstruction::payloadLength() const throw()
{
	return sizeof(action) + sizeof(width) + sizeof(height)
		+ sizeof(user_mode) + sizeof(user_limit) + sizeof(flags) + sizeof(title_len)
		+ title_len;
}

/*
 * struct SetPassword
 */

size_t SetPassword::unserialize(const char* buf, const size_t len) throw(std::bad_alloc)
{
	assert(buf != 0 and len > 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	assert(reqDataLen(buf, len) <= len);
	
	size_t i = unserializeHeader(buf);
	
	memcpy_t(password_len, buf+i); i += sizeof(password_len);
	if (password_len != 0)
	{
		password = new char[password_len];
		memcpy(password, buf+i, password_len); i += password_len;
	}
	
	return i;
}

size_t SetPassword::reqDataLen(const char *buf, const size_t len) const throw()
{
	assert(buf != 0 and len != 0);
	assert(static_cast<uint8_t>(buf[0]) == type);
	
	const size_t off = headerSize;
	if (len < off + sizeof(password_len))
		return off + sizeof(password_len);
	else
	{
		uint8_t pwlen;
		memcpy_t(pwlen, buf+off);
		return off + sizeof(password_len) + pwlen;
	}
}

size_t SetPassword::serializePayload(char *buf) const throw()
{
	assert(buf != 0);
	
	memcpy_t(buf, password_len); size_t i = sizeof(password_len);
	memcpy(buf+i, password, password_len); i += password_len;
	
	return i;
}

size_t SetPassword::payloadLength() const throw()
{
	return sizeof(password_len) + password_len;
}

/*
 * struct ListSessions
 */

// no special implementation required

/*
 * struct Cancel
 */

// none needed

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
	
	const size_t off = headerSize + sizeof(mode) + sizeof(event);
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
	
	return headerSize + payloadLength();
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
	
	const size_t off = headerSize + sizeof(width) + sizeof(height)
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
	
	return headerSize + payloadLength();
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
	
	return headerSize + payloadLength();
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
	
	const size_t off = headerSize + sizeof(uncompressed);
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
	
	if (len < headerSize + sizeof(length))
		return headerSize + sizeof(length);
	else
	{
		uint8_t rlen;
		memcpy_t(rlen, buf+headerSize);
		
		return headerSize + sizeof(length) + rlen;
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
	
	const size_t off = headerSize + sizeof(offset);
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

/*
 * struct SessionSelect
 */

// none needed

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
	
	return headerSize + payloadLength();
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
	
	return headerSize + payloadLength();
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
	
	return headerSize + sizeof(layer_id);
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

} // namespace protocol

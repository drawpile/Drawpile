#include <cassert> // assert()
#include <cstdlib> // NULL
#include <string> // memcpy()
#include <stdint.h> // uint16_t

#include "sockets.h" // htonl(), ntohs(), etc.
#include "protocol.h"

using namespace protocol;

// Base serialization
char *Message::serialize(size_t &len) const
{
	size_t length;
	size_t headerlen;
	unsigned int count = 1;

	if(isBundled) {
		// If messages are not bundled, simply concatenate whole messages
		headerlen = 1 + isUser?1:0;
		length = headerlen;
	} else {
		// If we are bundling packets, there will be no extra headers
		headerlen = 0;
		length = 2 + isUser?1:0;
	}

	// At least one message is serialized
	length += payloadLength();

	// Count number of messages to serialize and required size
	const Message *ptr;
	if(next) {
		const Message *ptr2 = next;
		while(ptr2) {
			++count;
			length += headerlen + ptr->payloadLength();
			ptr2=ptr2->next;
		}
	}
	if(prev) {
		ptr = prev;
		do {
			++count;
			length += headerlen + ptr->payloadLength();
			ptr = ptr->prev;
		} while(ptr->prev);
	}

	// ptr now points to the first message in list.
	// Allocate memory and serialize.
	char *data = new char[length];
	char *dataptr = data;

	// Write bundled packets
	if(isBundled) {
		*(dataptr++) = type;
		*(dataptr++) = count;
		if(isUser)
			*(dataptr++) = user_id;
		while(ptr) {
			dataptr += ptr->serializePayload(dataptr);
			ptr=ptr->next;
		}
	} else {
		// Write whole packets
		while(ptr) {
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
	uint32_t tmp = htonl(version);
	size_t size = identifier_size;
	memcpy(buffer, &identifier, identifier_size);
	memcpy(buffer+size, &tmp, sizeof(version)); size += sizeof(version);
	memcpy(buffer+size, &flags, sizeof(flags)); size += sizeof(flags);
	memcpy(buffer+size, &extensions, sizeof(extensions)); size += sizeof(extensions);
	return size;
}

size_t Identifier::payloadLength() const
{
	return identifier_size + sizeof(version) + sizeof(flags) + sizeof(extensions);
}

Identifier *Identifier::unserialize(Identifier& in, const char* buf, size_t len)
{
	assert(buf[0] == in.type);
	assert(isEnoughData(buf, len));
	
	memcpy(&in.identifier, buf, identifier_size);
	memcpy(&in.version, buf+identifier_size, sizeof(in.version));
	memcpy(&in.flags, buf+12, sizeof(in.flags));
	memcpy(&in.extensions, buf+13, sizeof(in.extensions));
	
	in.version = ntohl(in.version);
	
	return &in;
}

bool Identifier::isEnoughData(const char *buf, size_t len)
{
	assert(buf[0] == type::Identifier);
	const size_t reqlen = 1 + identifier_size + sizeof(uint32_t) + sizeof(uint8_t) * 2;
	return reqlen <= len;
}

// Stroke info serialization
size_t StrokeInfo::serializePayload(char *buffer) const
{
	uint32_t nx = htonl(x);
	uint32_t ny = htonl(y);
	memcpy(buffer, &nx, 2);
	memcpy(buffer+2, &ny, 2);
	memcpy(buffer+4, &pressure, 1);
	return 5;
}

size_t StrokeInfo::payloadLength() const
{
	return 5;
}

StrokeInfo* StrokeInfo::unserialize(StrokeInfo& in, const char* buf, size_t len)
{
	assert(buf[0] == in.type);
	assert(isEnoughData(buf, len));
	
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

bool StrokeInfo::isEnoughData(const char *buf, size_t len)
{
	assert(buf[0] == type::StrokeInfo);
	size_t reqlen = 2+ buf[1]*5;
	return reqlen <= len;
}


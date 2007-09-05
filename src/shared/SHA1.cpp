/******************************************************************************

  by M.K.A. <wyrmchild@users.sourceforge.net>
  based on code by Dominik Reichl <dominik.reichl@t-online.de>
  http://www.dominik-reichl.de/
  
  100% free public domain implementation of the SHA-1 algorithm   

******************************************************************************/

#include "SHA1.h"

#include "templates.h"

#include <cassert>
#include <memory.h> // memset(), memcpy()
#ifdef HAVE_OPENSSL
	#include <stdexcept>
#endif
#include <algorithm> // std::swap

SHA1::SHA1()
{
	Reset();
}

void SHA1::Reset()
{
	#ifndef HAVE_OPENSSL
	// SHA1 initialization constants
	m_state[0] = 0x67452301;
	m_state[1] = 0xEFCDAB89;
	m_state[2] = 0x98BADCFE;
	m_state[3] = 0x10325476;
	m_state[4] = 0xC3D2E1F0;
	
	m_left = 0;
	m_size = 0;
	#else // OpenSSL
	if (SHA1_Init(&context) != 1)
	{
		throw std::exception;
	}
	#endif // HAVE_OPENSSL
}

#ifndef HAVE_OPENSSL
// Rotate \b v n bits to the left
uint32_t SHA1::LeftRotate(const uint32_t v, const uint32_t n) const
{
	return (v << n) | (v >> (32 - n));
}

uint32_t SHA1::Chunk(const uint32_t i)
{
	return (workblock.l[i&0x0F] = LeftRotate((workblock.l[(i-3)&0x0F] ^ workblock.l[(i-8)&0x0F] ^ workblock.l[(i-14)&15] ^ workblock.l[i&0x0F]), 1));
}

void SHA1::R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i)
{
	z += ((w & (x ^ y)) ^ y) + workblock.l[i] + 0x5A827999 + LeftRotate(v, 5);
	w = LeftRotate(w, 30);
}

void SHA1::R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i)
{
	z += ((w & (x ^ y)) ^ y) + Chunk(i) + 0x5A827999 + LeftRotate(v, 5);
	w = LeftRotate(w, 30);
}

void SHA1::R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i)
{
	z += (w ^ x ^ y) + Chunk(i) + 0x6ED9EBA1 + LeftRotate(v, 5);
	w = LeftRotate(w, 30);
}

void SHA1::R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i)
{
	z += (((w | x) & y) | (w & x)) + Chunk(i) + 0x8F1BBCDC + LeftRotate(v, 5);
	w = LeftRotate(w, 30);
}

void SHA1::R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i)
{
	z += (w ^ x ^ y) + Chunk(i) + 0xCA62C1D6 + LeftRotate(v, 5);
	w = LeftRotate(w, 30);
}

void SHA1::Transform()
{
	#ifndef IS_BIG_ENDIAN
	for (int i=0; i != 16; ++i)
		bswap(workblock.l[i]);
	#endif
	
	m_left -= 64;
	
	uint32_t
		a = m_state[0],
		b = m_state[1],
		c = m_state[2],
		d = m_state[3],
		e = m_state[4];
	
	// 4 rounds of 20 operations each. Loop unrolled.
	R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
	
	R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
	
	R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
	R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
	R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
	R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
	R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
	
	R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
	R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
	R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
	R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
	R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);

	R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
	R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
	R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
	R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
	R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
	
	// Add the working vars back into state
	m_state[0] += a;
	m_state[1] += b;
	m_state[2] += c;
	m_state[3] += d;
	m_state[4] += e;
}
#endif // HAVE_OPENSSL

// Use this function to hash in binary data and strings
void SHA1::Update(const uchar *data, const uint64_t len)
{
	assert(len >= 0);
	assert(data != 0);
	
	#ifdef HAVE_OPENSSL
	SHA1_Update(&context, data, len);
	#else
	
	m_left += len;
	
	// data left in workblock from previous update
	const uint32_t left = m_size % 64;
	
	// update total size
	m_size += len;
	
	if (m_left < 64ULL)
	{
		// less data than we can process
		memcpy(workblock.c+left, data, len);
	}
	else
	{
		int64_t i = 64 - left;
		memcpy(workblock.c+left, data, i);
		Transform();
		
		int64_t last = len - (m_left % 64);
		for (; i != last; i += 64)
		{
			memcpy(workblock.c, data+i, 64);
			Transform();
		}
		
		// save rest of the buffer for later processing
		memcpy(workblock.c, data+i, len - i);
	}
	assert(m_left < 64);
	#endif
}

void SHA1::Final()
{
	#ifdef HAVE_OPENSSL
	SHA1_Final(m_state, &context);
	#else
	
	// test if the input data length is larger than what SHA-1 can handle
	assert(m_size <= (std::numeric_limits<uint64_t>::max()/8));
	
	union {
		uint64_t ll;
		#ifndef IS_BIG_ENDIAN
		uint32_t l[2]; // for byte swapping
		#endif
	} swb = {m_size * 8}; // size in bits
	
	#ifndef IS_BIG_ENDIAN
	std::swap(swb.l[0], swb.l[1]);
	bswap(swb.l[0]);
	bswap(swb.l[1]);
	#endif
	
	static const uchar padding[64] = {0x80,0};
	Update(padding, 1);
	
	if (m_left < 56)
		Update(padding+1, 56-m_left);
	else if (m_left > 56)
		Update(padding+1, 120-m_left);
	
	// append size
	Update(reinterpret_cast<uchar*>(&swb.ll), 8);
	
	// should be even 512 bits (64 bytes) now
	assert(m_size % 64 == 0);
	
	bswap(m_state[0]);
	bswap(m_state[1]);
	bswap(m_state[2]);
	bswap(m_state[3]);
	bswap(m_state[4]);
	#endif
}

// Get the final hash as a pre-formatted (ASCII) string (40 bytes long)
void SHA1::HexDigest(char *string) const
{
	assert(string != 0);
	
	// Hex magic by George Anescu
	static const uchar Hex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	
	uchar digest[20];
	GetHash(digest);
	
	for (int i=0; i != 20; ++i)
	{
		*(string+(i*2)) = Hex[digest[i] >> 4];
		*(string+(i*2)+1) = Hex[digest[i] & 0xF];
	}
}

// Get the raw message digest (20 bytes long)
void SHA1::GetHash(uchar *digest) const
{
	assert(digest != 0);
	
	memcpy(digest, m_state, 20);
}

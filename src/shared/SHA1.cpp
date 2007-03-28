/******************************************************************************

   Modified by M.K.A. <wyrmchild@users.sourceforge.net> - 2006-12-12
   Removed rest of the macros in 2007-01-12
   
   ---
   
   100% free public domain implementation of the SHA-1 algorithm
   by Dominik Reichl <dominik.reichl@t-online.de>
   Web: http://www.dominik-reichl.de/
   
   Version 1.6 - 2005-02-07

******************************************************************************/

#include "SHA1.h"

#include <cassert>
#include <memory.h> // memset(), memcpy()

SHA1::SHA1() throw()
	#ifndef NDEBUG
	: finalized(false)
	#endif
{
	memset(m_block.l, 0, 16);
	Reset();
}

SHA1::~SHA1() throw()
{
	#ifdef SHA1_WIPE_VARIABLES
	Reset();
	#endif
}

void SHA1::Reset() throw()
{
	// SHA1 initialization constants
	m_state[0] = 0x67452301;
	m_state[1] = 0xEFCDAB89;
	m_state[2] = 0x98BADCFE;
	m_state[3] = 0x10325476;
	m_state[4] = 0xC3D2E1F0;
	
	m_count[0] = 0;
	m_count[1] = 0;
	
	#ifndef NDEBUG
	finalized = false;
	#endif
}

// Rotate x bits to the left
inline
uint32_t SHA1::ROL32(const uint32_t v, const uint32_t n) const throw()
{
	return (v << n) | (v >> (32 - n));
}


inline
uint32_t SHA1::SHABLK0(const uint32_t i) throw()
{
	#ifdef IS_BIG_ENDIAN
	return m_block.l[i];
	#else
	return (m_block.l[i] = 
		(ROL32(m_block.l[i],24) & 0xFF00FF00) | (ROL32(m_block.l[i],8) & 0x00FF00FF)
	);
	#endif
}

inline
uint32_t SHA1::SHABLK1(const uint32_t i) throw()
{
	return (m_block.l[i&15]
		= ROL32(
		(
			m_block.l[(i+13)&15]
			^ m_block.l[(i+8)&15]
			^ m_block.l[(i+2)&15]
			^ m_block.l[i&15]
		), 1)
	);
}

inline
void SHA1::_R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += ((w & (x ^ y)) ^ y) + SHABLK0(i) + 0x5A827999 + ROL32(v, 5);
	w = ROL32(w, 30);
}

inline
void SHA1::_R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += ((w & (x ^ y)) ^ y) + SHABLK1(i) + 0x5A827999 + ROL32(v, 5);
	w = ROL32(w, 30);
}

inline
void SHA1::_R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (w ^ x ^ y) + SHABLK1(i) + 0x6ED9EBA1 + ROL32(v, 5);
	w = ROL32(w, 30);
}

inline
void SHA1::_R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (((w | x) & y) | (w & x)) + SHABLK1(i) + 0x8F1BBCDC + ROL32(v, 5);
	w = ROL32(w, 30);
}

inline
void SHA1::_R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (w ^ x ^ y) + SHABLK1(i) + 0xCA62C1D6 + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::Transform(uint32_t *state, const uint8_t *buffer) throw()
{
	// Copy state[] to working vars
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
	
	memcpy(m_block.c, buffer, 64);
	
	// 4 rounds of 20 operations each. Loop unrolled.
	_R0(a,b,c,d,e, 0); _R0(e,a,b,c,d, 1); _R0(d,e,a,b,c, 2); _R0(c,d,e,a,b, 3);
	_R0(b,c,d,e,a, 4); _R0(a,b,c,d,e, 5); _R0(e,a,b,c,d, 6); _R0(d,e,a,b,c, 7);
	_R0(c,d,e,a,b, 8); _R0(b,c,d,e,a, 9); _R0(a,b,c,d,e,10); _R0(e,a,b,c,d,11);
	_R0(d,e,a,b,c,12); _R0(c,d,e,a,b,13); _R0(b,c,d,e,a,14); _R0(a,b,c,d,e,15);
	_R1(e,a,b,c,d,16); _R1(d,e,a,b,c,17); _R1(c,d,e,a,b,18); _R1(b,c,d,e,a,19);
	_R2(a,b,c,d,e,20); _R2(e,a,b,c,d,21); _R2(d,e,a,b,c,22); _R2(c,d,e,a,b,23);
	_R2(b,c,d,e,a,24); _R2(a,b,c,d,e,25); _R2(e,a,b,c,d,26); _R2(d,e,a,b,c,27);
	_R2(c,d,e,a,b,28); _R2(b,c,d,e,a,29); _R2(a,b,c,d,e,30); _R2(e,a,b,c,d,31);
	_R2(d,e,a,b,c,32); _R2(c,d,e,a,b,33); _R2(b,c,d,e,a,34); _R2(a,b,c,d,e,35);
	_R2(e,a,b,c,d,36); _R2(d,e,a,b,c,37); _R2(c,d,e,a,b,38); _R2(b,c,d,e,a,39);
	_R3(a,b,c,d,e,40); _R3(e,a,b,c,d,41); _R3(d,e,a,b,c,42); _R3(c,d,e,a,b,43);
	_R3(b,c,d,e,a,44); _R3(a,b,c,d,e,45); _R3(e,a,b,c,d,46); _R3(d,e,a,b,c,47);
	_R3(c,d,e,a,b,48); _R3(b,c,d,e,a,49); _R3(a,b,c,d,e,50); _R3(e,a,b,c,d,51);
	_R3(d,e,a,b,c,52); _R3(c,d,e,a,b,53); _R3(b,c,d,e,a,54); _R3(a,b,c,d,e,55);
	_R3(e,a,b,c,d,56); _R3(d,e,a,b,c,57); _R3(c,d,e,a,b,58); _R3(b,c,d,e,a,59);
	_R4(a,b,c,d,e,60); _R4(e,a,b,c,d,61); _R4(d,e,a,b,c,62); _R4(c,d,e,a,b,63);
	_R4(b,c,d,e,a,64); _R4(a,b,c,d,e,65); _R4(e,a,b,c,d,66); _R4(d,e,a,b,c,67);
	_R4(c,d,e,a,b,68); _R4(b,c,d,e,a,69); _R4(a,b,c,d,e,70); _R4(e,a,b,c,d,71);
	_R4(d,e,a,b,c,72); _R4(c,d,e,a,b,73); _R4(b,c,d,e,a,74); _R4(a,b,c,d,e,75);
	_R4(e,a,b,c,d,76); _R4(d,e,a,b,c,77); _R4(c,d,e,a,b,78); _R4(b,c,d,e,a,79);
	
	// Add the working vars back into state
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	
	// Wipe variables
	#ifdef SHA1_WIPE_VARIABLES
	a = b = c = d = e = 0;
	#endif
}

// Use this function to hash in binary data and strings
void SHA1::Update(const uint8_t *data, const uint32_t len) throw()
{
	assert(len > 0);
	assert(data != 0);
	
	assert(not finalized);
	
	const uint32_t j = (m_count[0] >> 3) & 63;
	
	if ((m_count[0] += len << 3) < (len << 3))
		++m_count[1];
	
	m_count[1] += (len >> 29);
	
	uint32_t i;
	if (j + len > 63)
	{
		i = 64 - j;
		memcpy(&m_buffer[j], data, i);
		Transform(m_state, m_buffer);
		
		for (; i + 63 < len; i += 64)
			Transform(m_state, &data[i]);
		
		memcpy(&m_buffer[0], &data[i], len - i);
	}
	else
	{
		memcpy(&m_buffer[j], &data[0], len);
	}
}

void SHA1::Final() throw()
{
	assert(not finalized);
	
	uint32_t i;
	uint8_t finalcount[8];
	
	for(i = 0; i < 8; ++i)
		finalcount[i] = (uint8_t)((m_count[((i >= 4) ? 0 : 1)]
			>> ((3 - (i & 3)) * 8) ) & 255); // Endian independent
	
	Update((uint8_t *)"\200", 1);
	
	while ((m_count[0] & 504) != 448)
		Update((uint8_t *)"\0", 1);
	
	Update(finalcount, 8); // Cause a SHA1Transform()
	
	for (i = 0; i != 20; ++i)
	{
		m_digest[i] = static_cast<uint8_t>((m_state[i >> 2] >> ((3 - (i & 3)) * 8) ) & 255);
	}
	
	// Wipe variables for security reasons
	#ifdef SHA1_WIPE_VARIABLES
	i = 0;
	memset(m_buffer, 0, 64);
	memset(m_state, 0, 20);
	memset(m_count, 0, 8);
	memset(finalcount, 0, 8);
	Transform(m_state, m_buffer);
	#endif
	
	#ifndef NDEBUG
	finalized = true;
	#endif
}

// Get the final hash as a pre-formatted string
void SHA1::HexDigest(char *szReport) const throw()
{
	assert(finalized);
	assert(szReport != 0);
	
	// Hex magic by George Anescu.
	static const unsigned char saucHex[16] =
		{'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	
	for (int i=0; i != 20; ++i)
	{
		*(szReport+(i*2)) = saucHex[m_digest[i] >> 4];
		*(szReport+(i*2)+1) = saucHex[m_digest[i] & 0xF];
	}
}

// Get the raw message digest
void SHA1::GetHash(uint8_t *puDest) const throw()
{
	assert(finalized);
	assert(puDest != 0);
	
	memcpy(puDest, m_digest, 20);
}

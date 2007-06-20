/******************************************************************************

   Modified by M.K.A. <wyrmchild@users.sourceforge.net> - 2006-12-12
   Removed rest of the macros in 2007-01-12
   
   ---
   
   100% free public domain implementation of the SHA-1 algorithm
   by Dominik Reichl <dominik.reichl@t-online.de>
   Web: http://www.dominik-reichl.de/
   
   Version 1.6 - 2005-02-07

******************************************************************************/

#include "templates.h"
#include "SHA1.h"

#include <cassert>
#include <memory.h> // memset(), memcpy()

SHA1::SHA1() throw()
	#ifndef NDEBUG
	: finalized(false)
	#endif
{
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
	memset(m_block.l, 0, sizeof(m_block.l));
	memset(m_buffer, 0, sizeof(m_buffer));
	
	m_state[0] = 0x67452301;
	m_state[1] = 0xEFCDAB89;
	m_state[2] = 0x98BADCFE;
	m_state[3] = 0x10325476;
	m_state[4] = 0xC3D2E1F0;
	
	memset(m_count, 0, sizeof(m_count));
	
	#ifndef NDEBUG
	finalized = false;
	#endif
}

// Rotate \b v n bits to the left
uint32_t SHA1::ROL32(const uint32_t v, const uint32_t n) const throw()
{
	#ifdef USE_ASM
	uint32_t x;
	asm (
		"rol %%cl, %0;"
		: "=r"(x)
		: "0"(v), "c"(n)
	);
	return x;
	#else
	return (v << n) | (v >> (32 - n));
	#endif
}


uint32_t SHA1::SHABLK0(const uint32_t i) throw()
{
	#ifdef IS_BIG_ENDIAN
	return m_block.l[i];
	#else
	return bswap(m_block.l[i]);
	#endif
}

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

void SHA1::R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += ((w & (x ^ y)) ^ y) + SHABLK0(i) + 0x5A827999 + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += ((w & (x ^ y)) ^ y) + SHABLK1(i) + 0x5A827999 + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (w ^ x ^ y) + SHABLK1(i) + 0x6ED9EBA1 + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (((w | x) & y) | (w & x)) + SHABLK1(i) + 0x8F1BBCDC + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw()
{
	z += (w ^ x ^ y) + SHABLK1(i) + 0xCA62C1D6 + ROL32(v, 5);
	w = ROL32(w, 30);
}

void SHA1::Transform(uint32_t *state, const uchar *buffer) throw()
{
	// Copy state[] to working vars
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
	
	memcpy(m_block.c, buffer, 64);
	
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
void SHA1::Update(const uchar *data, const uint32_t len) throw()
{
	assert(len >= 0);
	assert(data != 0);
	
	if (len == 0) return;
	
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
		memcpy(&m_buffer[j], &data[0], len);
}

void SHA1::Final() throw()
{
	assert(not finalized);
	
	uint32_t i;
	uint8_t finalcount[8];
	
	for(i = 0; i < 8; ++i)
		finalcount[i] = (uint8_t)((m_count[((i >= 4) ? 0 : 1)]
			>> ((3 - (i & 3)) * 8) ) & 255); // Endian independent
	
	Update((uchar *)"\200", 1);
	
	while ((m_count[0] & 504) != 448)
		Update((uchar *)"\0", 1);
	
	Update(finalcount, 8); // Cause a SHA1Transform()
	
	for (i = 0; i != 20; ++i)
	{
		m_digest[i] = static_cast<uchar>((m_state[i >> 2] >> ((3 - (i & 3)) * 8) ) & 255);
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

// Get the final hash as a pre-formatted (ASCII) string (40 bytes long)
void SHA1::HexDigest(char *szReport) const throw()
{
	assert(finalized);
	assert(szReport != 0);
	
	// Hex magic by George Anescu
	static const uchar saucHex[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	
	/*
	#ifdef HAVE_SNPRINTF
	snprintf(
	#else
	sprintf(
	#endif
		szReport,
		#ifdef HAVE_SNPRINTF
		40,
		#endif
		"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		m_digest[0], m_digest[1], m_digest[2], m_digest[3],
		m_digest[4], m_digest[5], m_digest[6], m_digest[7],
		m_digest[8], m_digest[9], m_digest[10], m_digest[11],
		m_digest[12], m_digest[13], m_digest[14], m_digest[15],
		m_digest[16], m_digest[17], m_digest[18], m_digest[19]
	);
	*/
	
	for (int i=0; i != 20; ++i)
	{
		*(szReport+(i*2)) = saucHex[m_digest[i] >> 4];
		*(szReport+(i*2)+1) = saucHex[m_digest[i] & 0xF];
	}
}

// Get the raw message digest (20 bytes long)
void SHA1::GetHash(uchar *puDest) const throw()
{
	assert(finalized);
	assert(puDest != 0);
	
	memcpy(puDest, m_digest, 20);
}

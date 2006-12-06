/**
Modified by M.K.A. 06-12-2006.
**/

/*
	100% free public domain implementation of the SHA-1 algorithm
	by Dominik Reichl <dominik.reichl@t-online.de>
	Web: http://www.dominik-reichl.de/

	Version 1.6 - 2005-02-07

	======== Test Vectors (from FIPS PUB 180-1) ========

	SHA1("abc") =
		A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D

	SHA1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") =
		84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1

	SHA1(A million repetitions of "a") =
		34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

#include <stdint.h>

#include <memory.h> // Needed for memset and memcpy

#include <stdio.h>  // Needed for file access and sprintf
#include <string.h> // Needed for strcat and strcpy

#ifdef _MSC_VER
#include <stdlib.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Declare SHA1 workspace

typedef union
{
	uint8_t  c[64];
	uint32_t l[16];
} SHA1_WORKSPACE_BLOCK;

class CSHA1
{
public:
	// Constructor and Destructor
	CSHA1();
	~CSHA1();
	
	uint32_t m_state[5];
	uint32_t m_count[2];
	uint32_t __reserved1[1];
	uint8_t  m_buffer[64];
	uint8_t  m_digest[20];
	uint32_t __reserved2[3];

	void Reset();

	// Update the hash value
	void Update(uint8_t *data, uint32_t len);

	// Finalize hash and report
	void Final();

	// Report functions: as pre-formatted and raw data
	void HexDigest(char *szReport);
	void GetHash(uint8_t *puDest);

private:
	int SHABLK0(int i);
	
	// Private SHA-1 transformation
	void Transform(uint32_t *state, uint8_t *buffer);
	
	#ifndef NDEBUG
	bool finalized;
	#endif
	
	// Member variables
	uint8_t m_workspace[64];
	SHA1_WORKSPACE_BLOCK *m_block; // SHA1 pointer to the byte array above
};

#endif // SHA1_INCLUDEd

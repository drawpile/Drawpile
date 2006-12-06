
#ifndef TEST_HASHING_INCLUDED
#define TEST_HASHING_INCLUDED

#include "../shared/SHA1.h"
#include "testing.h"

#include <iostream>

namespace test
{

void hashing()
{
	testname("SHA-1 hashing");
	
	CSHA1 h;
	
	uint32_t res_len = 40;

	char* digest = new char[res_len+1];
	digest[res_len] = '\0';
	
	testing("SHA-1 #1");
	
	{
	unsigned char test1[] = "abc";
	unsigned char test1_result[] = "A9993E364706816ABA3E25717850C26C9CD0D89D";
	h.Update((uint8_t*)&test1, strlen((char*)&test1));
	h.Final();
	h.HexDigest(digest);
	
	testvars((char*)&test1, digest, (char*)&test1_result, false, res_len);
	}
	
	h.Reset();
	
	testing("SHA-1 #2");
	
	{
	unsigned char test2[] ="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	unsigned char test2_result[] = "84983E441C3BD26EBAAE4AA1F95129E5E54670F1";
	h.Update((uint8_t*)&test2, strlen((char*)&test2));
	h.Final();
	h.HexDigest(digest);
	
	testvars((char*)&test2, digest, (char*)&test2_result, false, res_len);
	}
	
	delete [] digest;
}

} // namespace test

#endif // TEST_HASHING_INCLUDED

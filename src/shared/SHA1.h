/******************************************************************************

  by M.K.A. <wyrmchild@users.sourceforge.net>
  based on code by Dominik Reichl <dominik.reichl@t-online.de>
  http://www.dominik-reichl.de/
  
  100% free public domain implementation of the SHA-1 algorithm   

******************************************************************************/

#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

#include "config.h"

#ifdef HAVE_OPENSSL
	#include <openssl/sha.h>
#endif

#include <boost/cstdint.hpp>

//! SHA-1 hash algorithm
/**
 * @example "SHA-1 hashing"
 * @code
 * SHA1 hash;
 *
 * hash.Update("abc", 3);
 * hash.Final();
 *
 * char digest[41] = {0};
 * hash.HexDigest(digest);
 *
 * printf("digest: %40x\n", digest);
 * @endcode
 */
class SHA1
{
	typedef unsigned char uchar;
	
	#ifdef HAVE_OPENSSL
	SHA_CTX context;
	#else
	
	// workblock
	union {
		uchar c[64];
		uint32_t l[16];
	} workblock;
	
	//! Number of bytes processed
	uint64_t m_size;
	#endif // HAVE_OPENSSL
	
	//! Intermediate digest
	union {
		uint32_t m_state[5];
		uchar m_digest[20];
	};
public:
	//! ctor
	SHA1() throw();
	
	//! Reset hasher
	void Reset() throw();
	
	//! Update the hash value
	/**
	 * @param data Input buffer for updating hash
	 * @param len Length of buffer
	 */
	void Update(const uchar *data, uint64_t len) throw();
	
	//! Finalize hash
	/**
	 * @note Must be called before either .HexDigest() or .GetHash()
	 */
	void Final() throw();
	
	//! Get hex digest
	/**
	 * @param string Target buffer for hex digest, needs to be at least 40 bytes long
	 */
	void HexDigest(char *string) const throw();
	
	//! Get binary digest
	/**
	 * @param digest Target buffer for binary digest, needs to be at least 20 bytes long
	 */
	void GetHash(uchar *digest) const throw();

private:
	#ifndef HAVE_OPENSSL
	uint32_t Chunk(const uint32_t i) throw();
	
	uint32_t ROL32(const uint32_t v, const uint32_t n) const throw();
	
	void R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void Transform(const uchar buffer[64]) throw();
	#endif // HAVE_OPENSSL
	
	#ifndef NDEBUG
	bool finalized;
	#endif
};

#endif // SHA1_INCLUDEd

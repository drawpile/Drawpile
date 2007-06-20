/******************************************************************************

   Modified by M.K.A. <wyrmchild@sourceforge.net> - 2006-12-12

   100% free public domain implementation of the SHA-1 algorithm
   by Dominik Reichl <dominik.reichl@t-online.de>
   Web: http://www.dominik-reichl.de/

   Version 1.6 - 2005-02-07

******************************************************************************/

#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

#include "config.h"
#include <boost/cstdint.hpp>

//! SHA-1 hash algorithm
class SHA1
{
	typedef unsigned char uchar;
	
	union {
		uchar c[64];
		uint32_t l[16];
	} m_block;
public:
	//! ctor
	SHA1() throw();
	
	uint32_t m_state[5];
	uint32_t m_count;
	uint64_t m_size;
	
	//! Reset hasher
	void Reset() throw();
	
	//! Update the hash value
	/**
	 * @param data Input buffer for updating hash
	 * @param len Length of buffer
	 */
	void Update(const uchar *data, const uint32_t len) throw();
	
	//! Finalize hash and report
	/**
	 * Must be called before either .HexDigest() or .GetHash()
	 */
	void Final() throw();
	
	//! Get hex digest
	/**
	 * @param szReport Target buffer for hex digest, needs to be at least 40 bytes long
	 */
	void HexDigest(char *szReport) const throw();
	
	//! Get binary digest
	/**
	 * @param puDest Target buffer for binary digest, needs to be at least 20 bytes long
	 */
	void GetHash(uchar *puDest) const throw();

private:
	uint32_t SHABLK1(const uint32_t i) throw();
	
	uint32_t ROL32(const uint32_t v, const uint32_t n) const throw();
	
	void R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void Transform(const uchar *buffer) throw();
	
	#ifndef NDEBUG
	bool finalized;
	#endif
};

#endif // SHA1_INCLUDEd

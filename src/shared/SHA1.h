/******************************************************************************

   Modified by M.K.A. <wyrmchild@sourceforge.net> - 2006-12-12

   100% free public domain implementation of the SHA-1 algorithm
   by Dominik Reichl <dominik.reichl@t-online.de>
   Web: http://www.dominik-reichl.de/

   Version 1.6 - 2005-02-07

******************************************************************************/

#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

#include <stdint.h>

class SHA1
{
	typedef union
	{
		uint8_t  c[64];
		uint32_t l[16];
	} Workspace;
public:
	// Constructor and Destructor
	SHA1() throw();
	~SHA1() throw();
	
	uint32_t m_state[5];
	uint32_t m_count[2];
	//uint32_t __reserved1[1];
	uint8_t  m_buffer[64];
	uint8_t  m_digest[20];
	//uint32_t __reserved2[3];
	
	void Reset() throw();
	
	// Update the hash value
	void Update(const unsigned char *data, const uint32_t len) throw();
	
	// Finalize hash and report
	void Final() throw();
	
	// Report functions: as pre-formatted and raw data
	// Requires you to provide pre-allocated string with length of at least 40 bytes
	void HexDigest(char *szReport) const throw();
	// Requires a string of at least 20 bytes
	void GetHash(unsigned char *puDest) const throw();

private:
	inline
	uint32_t SHABLK0(const uint32_t i) throw();
	
	inline
	uint32_t SHABLK1(const uint32_t i) throw();
	
	inline
	uint32_t ROL32(const uint32_t v, const uint32_t n) const throw();
	
	inline
	void _R0(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	inline
	void _R1(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	inline
	void _R2(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	inline
	void _R3(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	inline
	void _R4(const uint32_t v, uint32_t &w, const uint32_t x, const uint32_t y, uint32_t &z, const uint32_t i) throw();
	
	void Transform(uint32_t *state, const unsigned char *buffer) throw();
	
	#ifndef NDEBUG
	bool finalized;
	#endif
	
	// Member variables
	
	Workspace m_block;
};

#ifdef SHA1_OSTREAM
#include <ostream>

//! ostream extension for sha1
std::ostream& operator<< (std::ostream& os, const SHA1& hash)
{
	char digest[41];
	digest[40] = '\0';
	hash.HexDigest(digest);
	return os << digest;
}
#endif // SHA1_OSTREAM

#endif // SHA1_INCLUDEd

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
	void Update(const uint8_t *data, uint32_t len) throw();
	
	// Finalize hash and report
	void Final() throw();
	
	// Report functions: as pre-formatted and raw data
	void HexDigest(char *szReport) const throw();
	void GetHash(uint8_t *puDest) const throw();

private:
	inline
	uint32_t SHABLK0(uint32_t i) throw();
	
	inline
	uint32_t SHABLK1(uint32_t i) throw();
	
	inline
	uint32_t ROL32(uint32_t v, uint32_t n) throw();
	
	inline
	void _R0(uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) throw();
	
	inline
	void _R1(uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) throw();
	
	inline
	void _R2(uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) throw();
	
	inline
	void _R3(uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) throw();
	
	inline
	void _R4(uint32_t v, uint32_t &w, uint32_t x, uint32_t y, uint32_t &z, uint32_t i) throw();
	
	void Transform(uint32_t *state, const uint8_t *buffer) throw();
	
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

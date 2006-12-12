/******************************************************************************

   Modified by M.K.A. <wyrmchild@sourceforge.net> - 2006-12-12

   100% free public domain implementation of the SHA-1 algorithm
   by Dominik Reichl <dominik.reichl@t-online.de>
   Web: http://www.dominik-reichl.de/

   Version 1.6 - 2005-02-07

******************************************************************************/

#ifndef CSHA1_INCLUDED
#define CSHA1_INCLUDED

#include <stdint.h>

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
	CSHA1() throw();
	~CSHA1() throw();
	
	uint32_t m_state[5];
	uint32_t m_count[2];
	uint32_t __reserved1[1];
	uint8_t  m_buffer[64];
	uint8_t  m_digest[20];
	uint32_t __reserved2[3];

	void Reset() throw();

	// Update the hash value
	void Update(uint8_t *data, uint32_t len) throw();

	// Finalize hash and report
	void Final() throw();

	// Report functions: as pre-formatted and raw data
	void HexDigest(char *szReport) throw();
	void GetHash(uint8_t *puDest) throw();

private:
	inline
	int SHABLK0(int i) throw();
	
	// Private SHA-1 transformation
	void Transform(uint32_t *state, uint8_t *buffer) throw();
	
	#ifndef NDEBUG
	bool finalized;
	#endif
	
	// Member variables
	uint8_t m_workspace[64];
	SHA1_WORKSPACE_BLOCK *m_block; // SHA1 pointer to the byte array above
};

#endif // SHA1_INCLUDEd

/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "shared/SHA1.h"

#include <iostream>
#include <memory>

// source: http://csrc.nist.gov/ipsec/papers/rfc2202-testcases.txt
int main()
{
	SHA1 hash;
	
	hash.Update(0, 0);
	hash.Final();
	
	unsigned char digest[20];
	hash.GetHash(digest);
	
	unsigned char res[] = {0xda,0x39,0xa3,0xee,0x5e,0x6b,0x4b,0x0d,0x32,0x55,0xbf,0xef,0x95,0x60,0x18,0x90,0xaf,0xd8,0x07,0x09};
	
	int rv = memcmp(digest, res, 20);
	if (rv != 0)
		std::cerr << "digest mismatch" << std::endl;
	
	return (rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

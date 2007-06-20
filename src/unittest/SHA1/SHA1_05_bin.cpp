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
	
	unsigned char src[20];
	memset(src, 0x0b, 20);
	
	hash.Update(src, 20);
	
	unsigned char src2[] = "Hi There";
	hash.Update(src2, 8);
	
	hash.Final();
	
	unsigned char digest[20];
	hash.GetHash(digest);
	
	unsigned char res[] = {0xB6,0x17,0x31,0x86,0x55,0x05,0x72,0x64,0xE2,0x8B,0xC0,0xB6,0xFB,0x37,0x8C,0x8E,0xF1,0x46,0xBE,0x00};
	
	int rv = memcmp(digest, res, 20);
	if (rv != 0)
	{
		unsigned char res_str[] = "B617318655057264E28BC0B6FB378C8EF146BE00";
		
		char hex[41];
		hash.HexDigest(hex);
		hex[40] = 0;
		std::cerr << "expected: " << res_str << std::endl
			<< "result:   " << hex << std::endl;
	}
	
	return (rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

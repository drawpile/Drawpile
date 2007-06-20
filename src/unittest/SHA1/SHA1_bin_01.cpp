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

int main()
{
	SHA1 hash;
	
	unsigned char src1[] = "abc";
	
	hash.Update(src1, 3);
	hash.Final();
	
	unsigned char digest[20];
	hash.GetHash(digest);
	
	unsigned char res[] = {0xA9,0x99,0x3E,0x36,0x47,0x06,0x81,0x6A,0xBA,0x3E,0x25,0x71,0x78,0x50,0xC2,0x6C,0x9C,0xD0,0xD8,0x9D};
	char res1[] = "A9993E364706816ABA3E25717850C26C9CD0D89D";
	
	int rv = memcmp(digest, res, 20);
	if (rv != 0)
	{
		char hexdigest[41];
		hash.HexDigest(hexdigest);
		hexdigest[40] = 0;
		std::cerr << "result  : " << hexdigest << std::endl
			<< "expected: " << res1 << std::endl;
	}
	
	return (rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

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
	
	unsigned char src4[] = "0123456701234567012345670123456701234567012345670123456701234567";
	
	hash.Update(src4, strlen(reinterpret_cast<char*>(src4)));
	hash.Final();
	
	char hexdigest[40];
	hash.HexDigest(hexdigest);
	
	char res4[] = "DEA356A2CDDD90C7A7ECEDC5EBB563934F460452";
	
	int rv = memcmp(hexdigest, res4, 40);
	if (rv != 0)
		std::cerr << "result  : " << hexdigest << std::endl << "expected: " << res4 << std::endl;
	
	return (rv == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

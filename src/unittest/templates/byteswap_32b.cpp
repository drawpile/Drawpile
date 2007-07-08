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

#include <boost/cstdint.hpp>
#include "shared/templates.h"

int main()
{
	union num {
		uint32_t x;
		struct { uint8_t a,b,c,d; };
	};
	
	num u;
	
	u.a = 1;
	u.b = 2;
	u.c = 3;
	u.d = 4;
	
	bswap(u.x);
	
	bool r = ((u.a == 4) and (u.b == 3) and (u.c == 2) and (u.d == 1));
	
	return (r ? 0 : 1);
}

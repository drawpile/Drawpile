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

#include "shared/protocol.h"

#include <memory>

int main()
{
	protocol::Identifier *ident = new protocol::Identifier(1,2,3,4);
	
	if (ident->type != protocol::Message::Identifier)
		return EXIT_FAILURE;
	
	char *buffer=0;
	size_t size=0, length=0;
	buffer = ident->serialize(length, buffer, size);
	
	protocol::Identifier *ident2 = new protocol::Identifier;
	
	size_t length2 = ident2->unserialize(buffer, length);
	
	bool r = ((length == length2)
		and (ident->type == ident2->type)
		and (ident->revision == ident2->revision)
		and (ident->level == ident2->level)
		and (ident->flags == ident2->flags)
		and (ident->extensions == ident2->extensions));
	
	return (r ? EXIT_SUCCESS : EXIT_FAILURE);
}

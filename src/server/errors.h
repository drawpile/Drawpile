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

#ifndef SystemErrors_INCLUDED
#define SystemErrors_INCLUDED

#include <cerrno> // ENOMEM, etc.
#ifdef WIN32
	#include <winsock2.h>
#endif

namespace error
{

const int InvalidDescriptor = -1;
const int OutOfMemory = ENOMEM;
const int Fault = EFAULT;
const int BadDescriptor = EBADF;
const int SystemDescriptorLimit = ENFILE;
const int InsufficientPermissions = EPERM;

#ifdef WIN32
const int Interrupted = WSAEINTR;
const int WouldBlock = WSAEWOULDBLOCK;
const int DescriptorLimit = WSAEMFILE;
#else
const int DescriptorLimit = EMFILE;
const int Interrupted = EINTR;
	#ifndef EWOULDBLOCK
const int WouldBlock = EWOULDBLOCK;
	#else // some systems don't define EWOULDBLOCK
const int WouldBlock = EAGAIN;
	#endif
#endif

} // namespace:error

#endif // SystemErrors_INCLUDED

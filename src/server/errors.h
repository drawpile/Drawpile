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

#include <cstddef>

static const int OutOfMemory = ENOMEM;
static const int Fault = EFAULT;
static const int BadDescriptor = EBADF;
static const int SystemDescriptorLimit = ENFILE;
static const int InsufficientPermissions = EPERM;

#ifdef WIN32
static const int Interrupted = WSAEINTR;
static const int WouldBlock = WSAEWOULDBLOCK;
static const int DescriptorLimit = WSAEMFILE;
#else
static const int DescriptorLimit = EMFILE;
static const int Interrupted = EINTR;
	#ifndef EWOULDBLOCK
static const int WouldBlock = EWOULDBLOCK;
	#else // some systems don't define EWOULDBLOCK
static const int WouldBlock = EAGAIN;
	#endif
#endif

#endif // SystemErrors_INCLUDED

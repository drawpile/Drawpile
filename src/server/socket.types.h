/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef Sockets_Ext_INCLUDED
#define Sockets_Ext_INCLUDED

#ifdef WIN32
#include <ws2tcpip.h> // SOCKET, socklen_t
typedef SOCKET fd_t;
#else // POSIX
#include <sys/socket.h> // socklen_t
#include <netinet/in.h> // sockaddr_in
typedef int fd_t;
#define INVALID_SOCKET -1
#endif

#endif // Sockets_Ext_INCLUDED

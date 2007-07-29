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

#include "net.h"

#ifdef WIN32
	#include <winsock2.h>
#endif

bool startNetSubsystem()
{
	#ifdef WIN32
	const int maj=2, min=2;
	
	WSADATA info;
	if (WSAStartup((16<<maj|min), &info))
		return false;
	if (LOBYTE(info.wVersion) != maj or HIBYTE(info.wVersion) != min)
	{
		stopNetSubsystem();
		return false;
	}
	#endif
	
	return true;
}

void stopNetSubsystem()
{
	#ifdef WIN32
	WSACleanup();
	#endif
}

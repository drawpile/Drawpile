/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   ---
   
   Defines the most effecient event mechanism available as EventSystem.

*******************************************************************************/

#ifndef Event_H_INCLUDED
#define Event_H_INCLUDED

#include "config.h"

#if defined(EV_EPOLL)
	#include "epoll.h"
	typedef event::Epoll EventSystem;
#elif defined(EV_KQUEUE)
	#include "kqueue.h"
	typedef event::Kqueue EventSystem;
#elif defined(EV_WSAPOLL)
	#include "wsapoll.h"
	typedef event::WSAPoll EventSystem;
#elif defined(EV_SELECT)
	#include "select.h"
	typedef event::Select EventSystem;
#else
	#error No event mechanism defined!
#endif

#endif // Event_H_INCLUDED

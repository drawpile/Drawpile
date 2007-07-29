/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef EventKqueue_INCLUDED
#define EventKqueue_INCLUDED

#include "interface.h"
#include "traits.h"

#include <sys/types.h> // ?
#include <sys/event.h> // ?
#include <ctime> // timespec
#include <cstddef> // size_t

namespace event {

//! kqueue
/**
 * 
 */
class Kqueue
	: public Interface<Kqueue>
{
private:
	timespec _timeout;
	int nfds;
	kevent chlist[max_events], *evtrigr;
	size_t chlist_count, evtrigr_size;
public:
	/**
	 * @throw std::bad_alloc
	 */
	Kqueue();
	~Kqueue() __attribute__ ((nothrow));
	
	void timeout(uint msecs) __attribute__ ((nothrow));
	int wait() __attribute__ ((nothrow));
	int add(fd_t fd, int events) __attribute__ ((nothrow));
	int remove(fd_t fd) __attribute__ ((nothrow));
	int modify(fd_t fd, int events) __attribute__ ((nothrow));
	bool getEvent(fd_t &fd, int &events) __attribute__ ((nothrow));
};

/* traits */

template <> struct read<Kqueue> { static const int value; };
template <> struct write<Kqueue> { static const int value; };
template <> struct system<Kqueue> { static const std::string value; };

} // namespace:event

#endif // EventKqueue_INCLUDED

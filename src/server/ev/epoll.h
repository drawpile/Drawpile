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

#ifndef EventEpoll_INCLUDED
#define EventEpoll_INCLUDED

#include "interface.h"
#include "traits.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
#endif
#include <sys/epoll.h>

template <int max_events>
class EventEpoll
	: EventInterface<EventEpoll>
{
private:
	uint _timeout;
	int nfds;
	int evfd;
	epoll_event events[max_events];
public:
	EventEpoll() throw(std::exception);
	~EventEpoll() throw();
	
	void timeout(uint msecs) throw();
	int wait() throw();
	int add(fd_t fd, int events) throw();
	int remove(fd_t fd) throw();
	int modify(fd_t fd, int events) throw();
	bool getEvent(fd_t &fd, int &events) throw();
};

/* traits */

template <> struct event_has_hangup<EventEpoll> { static const bool value; };
template <> struct event_has_error<EventEpoll> { static const bool value; };
template <> struct event_read<EventEpoll> { static const int value; };
template <> struct event_write<EventEpoll> { static const int value; };
template <> struct event_error<EventEpoll> { static const int value; };
template <> struct event_hangup<EventEpoll> { static const int value; };

#endif // EventEpoll_INCLUDED

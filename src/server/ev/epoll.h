/*******************************************************************************

   Copyright (C) 2007, 2008 M.K.A. <wyrmchild@users.sourceforge.net>
   
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

#pragma once

#ifndef EventEpoll_H
#define EventEpoll_H

#include "config.h"
#include "abstract.h"

#include <sys/epoll.h> // epoll_event

#define EV_HAS_HANGUP true
#define EV_HAS_ERROR true
#define EV_NAME L"epoll"

//! epoll(4)
/**
 * @see man 4 epoll
 */
class Event
	: public Interface
{
public:
	enum {
		Read=EPOLLIN,
		Write=EPOLLOUT,
		Error=EPOLLERR,
		Hangup=EPOLLHUP
	};
	
protected:
	int evfd;
	epoll_event events[10];
	int nfds;
	uint m_timeout;
public:
	Event() NOTHROW;
	~Event() NOTHROW;
	
	void timeout(uint msecs) NOTHROW;
	int wait() NOTHROW;
	int add(SOCKET fd, event_t events) NOTHROW;
	int remove(SOCKET fd) NOTHROW;
	int modify(SOCKET fd, event_t events) NOTHROW;
	bool getEvent(SOCKET &fd, event_t &events) NOTHROW;
};

#endif // EventEpoll_H

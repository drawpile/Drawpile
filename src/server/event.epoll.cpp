/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior writeitten authorization.
   
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

#include "config.h"
#include "../shared/templates.h"
#include "event.h"

#ifndef EV_EPOLL
	#error EV_EPOLL not defined
#endif

#include <iostream>
#include <cerrno> // errno
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

/* Because MinGW is buggy, we have to do this fuglyness */
const uint32_t
	Event::read = EPOLLIN,
	Event::write = EPOLLOUT,
	Event::error = EPOLLERR,
	Event::hangup = EPOLLHUP;

Event::Event() throw()
	: evfd(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event()" << endl;
	#endif
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~Event()" << endl;
	#endif
	
	if (evfd != -1)
		finish();
	
	// Make sure the event fd was closed.
	assert(evfd == -1);
}

// Errors: ENFILE, ENOMEM
bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).init()" << endl;
	#endif
	
	evfd = epoll_create(max_events);
	
	if (evfd == -1)
	{
		_error = errno;
		
		assert(_error != EINVAL); // max_events is not positive integer
		
		return false;
	}
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).finish()" << endl;
	#endif
	
	close(evfd);
	evfd = -1;
}

int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).wait()" << endl;
	#endif
	
	nfds = epoll_wait(evfd, events, max_events, _timeout);
	
	if (nfds == -1)
	{
		_error = errno;
		
		if (_error == EINTR)
			return 0;
		
		assert(_error != EBADF);
		assert(_error != EFAULT); // events not writable
		assert(_error != EINVAL); // invalif evfd, or max_events <= 0
	}
	
	return nfds;
}

// Errors: ENOMEM
int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev_info);
	
	if (r == -1)
	{
		_error = errno;
		
		assert(_error != EBADF);
		assert(_error != EINVAL); // epoll fd is invalid, or fd is same as epoll fd
		assert(_error != EEXIST); // fd already in set
		assert(_error != EPERM); // target fd not supported by epoll
		
		return false;
	}
	
	return true;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev_info);
	
	if (r == -1)
	{
		_error = errno;
		
		assert(_error != EBADF); // epoll fd is invalid
		assert(_error != EINVAL); // evfd is invalid or fd is the same as evfd
		assert(_error != ENOENT); // fd not in set
		
		return false;
	}
	
	return true;
}

// Errors: ENOMEM
int Event::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	
	if (r == -1)
	{
		_error = errno;
		
		assert(_error != EBADF); // evfd is invalid
		assert(_error != EINVAL); // evfd is invalid, or evfd is the same as fd
		assert(_error != ENOENT); // fd not in set
		
		return false;
	}
	
	return true;
}

bool Event::getEvent(fd_t &fd, uint32_t &r_events) throw()
{
	if (nfds == -1)
		return false;
	
	fd = events[nfds].data.fd;
	r_events = events[nfds].events;
	--nfds;
	
	return true;
}

uint32_t Event::getEvents(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(epoll).getEvents(FD: " << fd << ")" << endl;
	#endif
	
	for (int n=0; n != nfds; ++n)
		if (events[n].data.fd == fd)
			return events[n].events;
	
	return 0;
}

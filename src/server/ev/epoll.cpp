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

#include "epoll.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <cerrno> // errno
#include <cassert> // assert()

template <> const bool event_has_hangup<EventEpoll>::value = true;
template <> const bool event_has_error<EventEpoll>::value = true;
template <> const int event_read<EventEpoll>::value = EPOLLIN;
template <> const int event_write<EventEpoll>::value = EPOLLOUT;
template <> const int event_error<EventEpoll>::value = EPOLLERR;
template <> const int event_hangup<EventEpoll>::value = EPOLLHUP;
template <> const std::string event_system<EventEpoll>::value("epoll");

EventEpoll::EventEpoll() throw(std::exception)
	: evfd(0), nfds(-1)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event()" << endl;
	#endif
	
	evfd = epoll_create(max_events);
	
	if (evfd == -1)
	{
		_error = errno;
		
		// max_events is not positive integer
		assert(_error != EINVAL);
		
		throw std::exception;
	}
}

EventEpoll::~EventEpoll() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~epoll()" << endl;
	#endif
	
	if (evfd != -1)
	{
		close(evfd);
		evfd = -1;
	}
	
	// Make sure the event fd was closed.
	assert(evfd == -1);
}

int EventEpoll::wait() throw()
{
	assert(evfd != 0);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.wait()" << endl;
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
int EventEpoll::add(fd_t fd, int events) throw()
{
	assert(evfd != 0);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = events;
	
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

int EventEpoll::modify(fd_t fd, int events) throw()
{
	assert(evfd != 0);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = events;
	
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
int EventEpoll::remove(fd_t fd) throw()
{
	assert(evfd != 0);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.remove(FD: " << fd << ")" << endl;
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

bool EventEpoll::getEvent(fd_t &fd, int &r_events) throw()
{
	assert(evfd != 0);
	
	if (nfds == -1)
		return false;
	
	fd = events[nfds].data.fd;
	r_events = events[nfds].events;
	--nfds;
	
	return true;
}

void EventEpoll::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	cout << "epoll.timeout(msecs: " << msecs << ")" << endl;
	#endif
	
	_timeout = msecs;
}

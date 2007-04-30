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

#ifndef EV_KEVENT
	#error EV_KEVENT not defined
#endif

#include <iostream>
#include <cerrno> // errno
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

/* Because MinGW is buggy, we have to do this fuglyness */
const uint32_t
	Event::read = KEVENT_SOCKET_RECV,
	Event::write = KEVENT_SOCKET_SEND;
	//Event::accept = KEVENT_SOCKET_ACCEPT;

Event::Event() throw()
	: evfd(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent)" << endl;
	#endif
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~Event(kevent)" << endl;
	#endif
	
	if (evfd != -1)
		finish();
	
	// Make sure the event fd was closed.
	assert(evfd == -1);
}

bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).init()" << endl;
	#endif
	
	kevent_user_control ctl;
	ctl.cmd = KEVENT_CTL_INIT;
	evfd = kevent_ctl(0, &ctl);
	
	if (r == -1)
	{
		_error = errno;
		
		switch (_error)
		{
		case EINVAL:
			#ifndef NDEBUG
			cerr << "Size not positive." << endl;
			#endif
			assert(1);
			break;
		case ENFILE:
			cerr << "System open FD limit reached." << endl;
			return false;
			break;
		case ENOMEM:
			cerr << "Out of memory" << endl;
			throw std::bad_alloc();
			break;
		default:
			#ifndef NDEBUG
			cerr << "Event(kevent).init() : Unknown error(" << _error << ")" << endl;
			#endif
			assert(1);
			break;
		}
	}
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).finish()" << endl;
	#endif
	
	close(evfd);
	evfd = -1;
}

int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).wait()" << endl;
	#endif
	
	nfds = epoll_wait(evfd, events, max_events, _timeout);
	_error = errno;
	
	if (nfds == -1)
	{
		assert(_error != EBADF);
		assert(_error != EFAULT); // events not writable
		assert(_error != EINVAL); // invalif evfd, or max_events <= 0
		switch (_error)
		{
		case EINTR:
			#ifndef NDEBUG
			cerr << "Interrupted by signal/timeout." << endl;
			#endif
			nfds = 0;
			break;
		default:
			cerr << "Event(kevent).wait() : Unknown error(" << _error << ")" << endl;
			// TODO
			break;
		}
	}
	
	return nfds;
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		assert(_error != EBADF);
		assert(_error != EINVAL); // epoll fd is invalid, or fd is same as epoll fd
		assert(_error != EEXIST); // fd already in set
		assert(_error != EPERM); // target fd not supported by epoll
		
		switch (_error)
		{
		case ENOMEM:
			cerr << "Out of memory" << endl;
			throw new std::bad_alloc;
			break;
		default:
			cerr << "Event(kevent).add() : Unknown error(" << _error << ")" << endl;
			break;
		}
	}
	
	return true;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		assert(_error != EBADF); // epoll fd is invalid
		assert(_error != EINVAL); // evfd is invalid or fd is the same as evfd
		assert(_error != ENOENT); // fd not in set
		
		switch (_error)
		{
		case ENOMEM:
			cerr << "Out of memory" << endl;
			throw new std::bad_alloc;
			break;
		default:
			cerr << "Event(kevent).modify() : Unknown error (" << _error << ")" << endl;
			break;
		}
	}
	
	return 0;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	_error = errno;
	if (r == -1)
	{
		assert(_error != EBADF); // evfd is invalid
		assert(_error != EINVAL); // evfd is invalid, or evfd is the same as fd
		assert(_error != ENOENT); // fd not in set
		
		switch (_error)
		{
		case ENOMEM:
			cerr << "Out of memory" << endl;
			throw new std::bad_alloc;
			break;
		default:
			cerr << "Event(kevent).remove() : Unknown error(" << _error << ")" << endl;
			break;
		}
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

uint32_t Event::getEvents(fd_t fd) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).getEvents(FD: " << fd << ")" << endl;
	#endif
	
	for (int n=0; n != nfds; ++n)
		if (events[n].data.fd == fd)
			return events[n].events;
	
	return 0;
}

bool Event::isset(fd_t fd, uint32_t ev) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kevent).isset(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (fIsSet(getEvents(fd), ev))
		return true;
	
	return false;
}

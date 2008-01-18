/*******************************************************************************

   Copyright (C) 2006, 2007, 2008 M.K.A. <wyrmchild@users.sourceforge.net>
   
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

#include <cerrno> // errno
#include <cassert> // assert()

Event::Event()
	: evfd(-1),
	nfds(-1)
{
	evfd = epoll_create(10);
	
	if (evfd == -1)
	{
		error = errno;
		
		// max_events is not positive integer
		assert(error != EINVAL);
	}
}

Event::~Event()
{
	close(evfd);
}

int Event::wait()
{
	assert(evfd != -1);
	
	nfds = epoll_wait(evfd, events, 10, m_timeout);
	
	if (nfds == -1)
	{
		error = errno;
		
		if (error == EINTR)
			return 0;
		
		assert(error != EBADF);
		assert(error != EFAULT); // events not writable
		assert(error != EINVAL); // invalif evfd, or max_events <= 0
	}
	
	return nfds--;
}

// Errors: ENOMEM
int Event::add(SOCKET fd, event_t events)
{
	assert(evfd != -1);
	
	assert(fd != -1);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = events;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev_info);
	
	if (r == -1)
	{
		error = errno;
		
		assert(error != EBADF);
		assert(error != EINVAL); // epoll fd is invalid, or fd is same as epoll fd
		assert(error != EEXIST); // fd already in set
		assert(error != EPERM); // target fd not supported by epoll
		
		return false;
	}
	
	return true;
}

int Event::modify(SOCKET fd, event_t events)
{
	assert(evfd != -1);
	
	assert(fd != -1);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = events;
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev_info);
	
	if (r == -1)
	{
		error = errno;
		
		assert(error != EBADF); // epoll fd is invalid
		assert(error != EINVAL); // evfd is invalid or fd is the same as evfd
		assert(error != ENOENT); // fd not in set
		
		return false;
	}
	
	return true;
}

// Errors: ENOMEM
int Event::remove(SOCKET fd)
{
	assert(evfd != -1);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != -1);
	
	const int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	
	if (r == -1)
	{
		error = errno;
		
		assert(error != EBADF); // evfd is invalid
		assert(error != EINVAL); // evfd is invalid, or evfd is the same as fd
		assert(error != ENOENT); // fd not in set
		
		return false;
	}
	
	return true;
}

bool Event::getEvent(SOCKET &r_fd, event_t &r_events)
{
	assert(evfd != -1);
	
	if (nfds < 0)
		return false;
	
	r_fd = events[nfds].data.fd;
	r_events = events[nfds].events;
	nfds--;
	
	assert(fd != -1); // shouldn't happen
	
	return true;
}

void Event::timeout(uint msecs)
{
	m_timeout = msecs;
}

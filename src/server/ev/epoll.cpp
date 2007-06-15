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

namespace event {

const bool has_hangup<Epoll>::value = true;
const bool has_error<Epoll>::value = true;
const int read<Epoll>::value = EPOLLIN;
const int write<Epoll>::value = EPOLLOUT;
const int error<Epoll>::value = EPOLLERR;
const int hangup<Epoll>::value = EPOLLHUP;
const std::string system<Epoll>::value("epoll");

Epoll::Epoll() throw(std::exception)
	: nfds(-1), evfd(invalid_fd<Epoll>::value)
{
	evfd = epoll_create(10);
	
	if (evfd == invalid_fd<Epoll>::value)
	{
		error = errno;
		
		// max_events is not positive integer
		assert(error != EINVAL);
		
		throw std::exception();
	}
}

Epoll::~Epoll() throw()
{
	if (evfd != invalid_fd<Epoll>::value)
	{
		close(evfd);
		evfd = -1;
	}
	
	// Make sure the event fd was closed.
	assert(evfd == invalid_fd<Epoll>::value);
}

int Epoll::wait() throw()
{
	assert(evfd != invalid_fd<Epoll>::value);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.wait()" << endl;
	#endif
	
	nfds = epoll_wait(evfd, events, 10, _timeout);
	
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
int Epoll::add(fd_t fd, ev_t events) throw()
{
	assert(evfd != invalid_fd<Epoll>::value);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != invalid_fd<Epoll>::value);
	
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

int Epoll::modify(fd_t fd, ev_t events) throw()
{
	assert(evfd != invalid_fd<Epoll>::value);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != invalid_fd<Epoll>::value);
	
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
int Epoll::remove(fd_t fd) throw()
{
	assert(evfd != invalid_fd<Epoll>::value);
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != invalid_fd<Epoll>::value);
	
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

bool Epoll::getEvent(fd_t &fd, ev_t &r_events) throw()
{
	assert(evfd != invalid_fd<Epoll>::value);
	
	if (nfds == -1)
		return false;
	
	fd = events[nfds].data.fd;
	r_events = events[nfds].events;
	--nfds;
	
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "epoll.getEvent(FD: " << fd << ", events: " << r_events << ")" << endl;
	#endif
	
	assert(fd != invalid_fd<Epoll>::value); // shouldn't happen
	
	return true;
}

void Epoll::timeout(uint msecs) throw()
{
	_timeout = msecs;
}

} // namespace:event

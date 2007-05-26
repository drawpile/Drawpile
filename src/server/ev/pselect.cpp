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

#include "pselect.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <cerrno> // errno
#include <cassert> // assert()

template <> const bool event_has_error<EventPselect>::value = true;
template <> const bool event_has_sigmask<EventPselect>::value = true;
template <> const int event_read<EventPselect>::value = 1;
template <> const int event_write<EventPselect>::value = 2;
template <> const int event_error<EventPselect>::value = 4;
template <> const std::string event_system<EventPselect>::value("pselect");

EventPselect::EventPselect() throw()
	: nfds_r(0),
	nfds_w(0),
	nfds_e(0)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect()" << endl;
	#endif
	
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
	
	sigemptyset(&_sigmask); // prepare sigmask
}

EventPselect::~EventPselect() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~pselect()" << endl;
	#endif
}

// Errors: WSAENETDOWN
int EventPselect::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.wait()" << endl;
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r);
	FD_COPY(&fds_w, &t_fds_w);
	FD_COPY(&fds_e, &t_fds_e);
	#else
	memcpy(&t_fds_r, &fds_r, sizeof(fds_r));
	memcpy(&t_fds_w, &fds_w, sizeof(fds_w));
	memcpy(&t_fds_e, &fds_e, sizeof(fds_e));
	#endif // HAVE_SELECT_COPY
	
	// save sigmask
	sigprocmask(SIG_SETMASK, &_sigmask, &_sigsaved);
	
	using std::max;
	const fd_t ubnfds = max(max(nfds_w,nfds_r), nfds_e);
	
	nfds = pselect((ubnfds==0?0:ubnfds+1), &t_fds_r, &t_fds_w, &t_fds_e, &_timeout, &_sigmask);
	error = errno;
	sigprocmask(SIG_SETMASK, &_sigsaved, 0); // restore mask
	
	switch (nfds)
	{
		case -1:
			if (error == EINTR)
				return nfds = 0;
			
			assert(error != EBADF);
			assert(error != ENOTSOCK);
			assert(error != EINVAL);
			assert(error != EFAULT);
			
			break;
		case 0:
			// do nothing
			break;
		default:
			fd_iter = fd_list.begin();
			break;
	}
	
	return nfds;
}

int EventPselect::add(fd_t fd, int events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.add(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	bool rc=false;
	
	if (fIsSet(events, event_read<EventPselect>::value))
	{
		FD_SET(fd, &fds_r);
		read_set.insert(read_set.end(), fd);
		nfds_r = *(read_set.end());
		rc = true;
	}
	if (fIsSet(events, event_write<EventPselect>::value))
	{
		FD_SET(fd, &fds_w);
		write_set.insert(write_set.end(), fd);
		nfds_w = *(--write_set.end());
		rc = true;
	}
	if (fIsSet(events, event_error<EventPselect>::value))
	{
		FD_SET(fd, &fds_e);
		error_set.insert(error_set.end(), fd);
		nfds_e = *(--error_set.end());
		rc = true;
	}
	
	// maintain fd_list
	fd_list[fd] = events;
	
	return rc;
}

int EventPselect::modify(fd_t fd, int events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.modify(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (!fIsSet(events, event_read<EventPselect>::value))
	{
		FD_CLR(fd, &fds_r);
		read_set.erase(fd);
		nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : 0);
	}
	
	if (!fIsSet(events, event_write<EventPselect>::value))
	{
		FD_CLR(fd, &fds_w);
		write_set.erase(fd);
		nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : 0);
	}
	
	if (!fIsSet(events, event_error<EventPselect>::value))
	{
		FD_CLR(fd, &fds_e);
		error_set.erase(fd);
		nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : 0);
	}
	
	return 0;
}

int EventPselect::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	std::map<fd_t,uint>::iterator iter(fd_list.find(fd));
	if (iter == fd_list.end())
		return false;
	
	FD_CLR(fd, &fds_r);
	read_set.erase(fd);
	nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : 0);
	
	FD_CLR(fd, &fds_w);
	write_set.erase(fd);
	nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : 0);
	
	FD_CLR(fd, &fds_e);
	error_set.erase(fd);
	nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : 0);
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool EventPselect::getEvent(fd_t &fd, int &events) throw()
{
	while (fd_iter != fd_list.end())
	{
		fd = fd_iter->first;
		++fd_iter;
		
		events = 0;
		
		if (FD_ISSET(fd, &t_fds_r) != 0)
			fSet(events, event_read<EventPselect>::value);
		if (FD_ISSET(fd, &t_fds_w) != 0)
			fSet(events, event_write<EventPselect>::value);
		if (FD_ISSET(fd, &t_fds_e) != 0)
			fSet(events, event_error<EventPselect>::value);
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void EventPselect::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "pselect.timeout(msecs: " << msecs << ")" << std::endl;
	#endif
	
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_nsec = msecs * 1000000;
}

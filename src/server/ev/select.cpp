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

#include "select.h"
#include "../shared/templates.h"

#ifndef NDEBUG
	#include <iostream>
#endif
#include <cerrno> // errno
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

/* Because MinGW is buggy, we have to do this fuglyness */
const EvSelect::ev_t
	Event::read = 0x01,
	Event::write = 0x02,
	Event::error = 0x04;

EvSelect::EvSelect() throw()
	#ifndef WIN32
	: nfds_r(0),
	nfds_w(0),
	nfds_e(0)
	#endif
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select()" << endl;
	#endif
	
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
}

EvSelect::~EvSelect() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~select()" << endl;
	#endif
}

// Errors: WSAENETDOWN
int EvSelect::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.wait()" << endl;
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r),
	FD_COPY(&fds_w, &t_fds_w),
	FD_COPY(&fds_e, &t_fds_e);
	#else
	t_fds_r = fds_r;
	t_fds_w = fds_w;
	t_fds_e = fds_e;
	//memcpy(&t_fds_r, &fds_r, sizeof(fd_set)),
	//memcpy(&t_fds_w, &fds_w, sizeof(fd_set)),
	//memcpy(&t_fds_e, &fds_e, sizeof(fd_set));
	#endif // HAVE_SELECT_COPY
	
	#if defined(EV_PSELECT)
	// save sigmask
	sigprocmask(SIG_SETMASK, &_sigmask, &_sigsaved);
	#endif // EV_PSELECT
	
	#ifndef WIN32
	const fd_t largest_nfds = std::max(std::max(nfds_w, nfds_r), nfds_e);
	#endif
	
	nfds =
	#if defined(EV_PSELECT)
		pselect(
	#else
		select(
	#endif // EV_PSELECT
	#ifdef WIN32
		0,
	#else // !WIN32
		(largest_nfds + 1),
	#endif // WIN32
		&t_fds_r,
		&t_fds_w,
		&t_fds_e,
		&_timeout
	#if defined(EV_PSELECT)
		, &_sigmask
	#endif // EV_PSELECT
		);
	#ifndef WIN32
	_error = errno;
	#ifdef EV_PSELECT
	sigprocmask(SIG_SETMASK, &_sigsaved, 0); // restore mask
	#endif
	#endif
	
	switch (nfds)
	{
		case -1:
			#ifdef WIN32
			_error = WSAGetLastError();
			#endif
			
			if (_error == EINTR)
				return nfds = 0;
			
			#ifdef WIN32
			assert(_error != WSANOTINITIALISED);
			#endif
			assert(_error != EBADF);
			assert(_error != ENOTSOCK);
			assert(_error != EINVAL);
			assert(_error != EFAULT);
			
			#if defined(WIN32) and !defined(NDEBUG)
			if (_error == WSAENETDOWN)
				cerr << "The network subsystem has failed." << endl;
			#endif
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

int EvSelect::add(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.add(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	bool rc=false;
	
	if (fIsSet(events, read))
	{
		FD_SET(fd, &fds_r);
		#if !defined(WIN32) // win32 ignores the argument
		read_set.insert(read_set.end(), fd);
		nfds_r = *(read_set.end());
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(events, write))
	{
		FD_SET(fd, &fds_w);
		#if !defined(WIN32) // win32 ignores the argument
		write_set.insert(write_set.end(), fd);
		nfds_w = *(--write_set.end());
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(events, error))
	{
		FD_SET(fd, &fds_e);
		#if !defined(WIN32) // win32 ignores the argument
		error_set.insert(error_set.end(), fd);
		nfds_e = *(--error_set.end());
		#endif // !Win32
		rc = true;
	}
	
	// maintain fd_list
	fd_list[fd] = events;
	
	return rc;
}

int EvSelect::modify(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.modify(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (!fIsSet(events, read))
	{
		FD_CLR(fd, &fds_r);
		#ifndef WIN32
		read_set.erase(fd);
		nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : 0);
		#endif // WIN32
	}
	
	if (!fIsSet(events, write))
	{
		FD_CLR(fd, &fds_w);
		#ifndef WIN32
		write_set.erase(fd);
		nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : 0);
		#endif // WIN32
	}
	
	if (!fIsSet(events, error))
	{
		FD_CLR(fd, &fds_e);
		#ifndef WIN32
		error_set.erase(fd);
		nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : 0);
		#endif // WIN32
	}
	
	return 0;
}

int EvSelect::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "select.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	std::map<fd_t,uint>::iterator iter(fd_list.find(fd));
	if (iter == fd_list.end())
		return false;
	
	FD_CLR(fd, &fds_r);
	#ifndef WIN32
	read_set.erase(fd);
	nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : 0);
	#endif // WIN32
	
	FD_CLR(fd, &fds_w);
	#ifndef WIN32
	write_set.erase(fd);
	nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : 0);
	#endif // WIN32
	
	FD_CLR(fd, &fds_e);
	#ifndef WIN32
	error_set.erase(fd);
	nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : 0);
	#endif // WIN32
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool EvSelect::getEvent(fd_t &fd, ev_t &events) throw()
{
	while (fd_iter != fd_list.end())
	{
		fd = fd_iter->first;
		++fd_iter;
		
		events = 0;
		
		if (FD_ISSET(fd, &t_fds_r) != 0)
			fSet(events, read);
		if (FD_ISSET(fd, &t_fds_w) != 0)
			fSet(events, write);
		if (FD_ISSET(fd, &t_fds_e) != 0)
			fSet(events, error);
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void EvSelect::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "select.timeout(msecs: " << msecs << ")" << std::endl;
	#endif
	
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_usec = msecs * 1000; // microseconds
}

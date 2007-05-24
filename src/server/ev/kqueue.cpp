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

#include "kqueue.h"

#ifndef NDEBUG
	#include <iostream>
#endif
#include <cerrno> // errno
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

/* Because MinGW is buggy, we have to do this fuglyness */
const EvKqueue::ev_t
	EvKqueue::read = EVFILT_READ,
	EvKqueue::write = EVFILT_WRITE;

EvKqueue::EvKqueue() throw(std::bad_alloc)
	: evfd(0),
	chlist_count(0),
	evtrigr_size(max_events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "kqueue()" << endl;
	#endif
	
	evfd = kqueue();
	if (kq == -1)
	{
		_error = errno;
		return false;
	}
	
	evtrigr = new kevent[evtrigr_size];
}

EvKqueue::~EvKqueue() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~kqueue()" << endl;
	#endif
	
	if (evfd != -1)
	{
		close(evfd);
		evfd = -1;
	}
	
	// Make sure the event fd was closed.
	assert(evfd == -1);
	
	delete [] evtrigr;
}

// Errors: EACCES, ENOMEM
int EvKqueue::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "kqueue.wait()" << endl;
	#endif
	
	nfds = kevent(evfd, &chlist, chlist_count, evtrigr, evtrigr_size, _timeout);
	chlist_count = 0;
	
	if (nfds == -1)
	{
		_error = errno;
		
		if (_error == EINTR)
			return 0;
		
		assert(_error != EBADF); // FD is invalid
		assert(_error != ENOENT); // specified event could not be found
		assert(_error != EINVAL); // filter or time limit is invalid
		assert(_error != EFAULT); // kevent struct is not writable
		
		#ifndef NDEBUG
		cerr << "Error in event system: " << _error << endl;
		#endif
		return -1;
	}
	
	return nfds;
}

int EvKqueue::add(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "kqueue.add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, events, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int EvKqueue::modify(fd_t fd, ev_t events) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "kqueue.modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, events, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int EvKqueue::remove(fd_t fd) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "kqueue.remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, 0, EV_DELETE|EV_DISABLE, 0, 0, 0);
	
	return true;
}

bool EvKqueue::getEvent(fd_t &fd, ev_t &r_events) throw()
{
	if (nfds == evtrigr_size)
		return false;
	
	assert(!(evtrigr[nfds].flags & EV_ERROR));
	
	fd = evtrigr[nfds].ident;
	r_events = evtrigr[nfds].filter;
	
	++nfds;
	
	return true;
}

void EvKqueue::timeout(uint msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "kqueue.timeout(msecs: " << msecs << ")" << std::endl;
	#endif
	
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_nsec = msecs * 1000000; // nanoseconds
}

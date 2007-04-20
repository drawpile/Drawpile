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

#include "../config.h"
#include "../shared/templates.h"
#include "event.h"

#ifndef EV_KQUEUE
	#error EV_KQUEUE not defined
#endif

#include <iostream>

#include <cerrno> // errno
#include <cassert> // assert()

using std::cout;
using std::endl;
using std::cerr;

/* Because MinGW is buggy, we have to do this fuglyness */
const uint32_t
	Event::read = EVFILT_READ,
	Event::write = EVFILT_WRITE;

Event::Event() throw()
	: evfd(0),
	chlist_count(0),
	evtrigr_size(max_events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue)" << endl;
	#endif
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "~Event(kqueue)" << endl;
	#endif
	
	if (evfd != -1)
		finish();
	
	// Make sure the event fd was closed.
	assert(evfd == -1);
	
	delete [] evtrigr;
}

// Errors: ENOMEM, EMFILE, ENFILE
bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).init()" << endl;
	#endif
	
	evfd = kqueue();
	if (kq == -1)
	{
		_error = errno;
		return false;
	}
	
	evtrigr = new kevent[evtrigr_size];
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).finish()" << endl;
	#endif
	
	close(evfd);
	evfd = -1;
}

// Errors: EACCES, ENOMEM
int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).wait()" << endl;
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
		
		cerr << "Error in event system: " << _error << endl;
		return -1;
	}
	
	return nfds;
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).add(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, ev, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).modify(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, ev, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).remove(FD: " << fd << ")" << endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (chlist_count == max_events)
		return false;
	
	EV_SET(&chlist[chlist_count++], fd, 0, EV_DELETE|EV_DISABLE, 0, 0, 0);
	
	return true;
}

bool Event::getEvent(fd_t &fd, uint32_t &r_events) throw()
{
	if (nfds == evtrigr_size)
		return false;
	
	assert(!(evtrigr[nfds].flags & EV_ERROR));
	
	fd = evtrigr[nfds].ident;
	r_events = evtrigr[nfds].filter;
	
	++nfds;
	
	return true;
}

uint32_t Event::getEvents(fd_t fd) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "Event(kqueue).getEvents(FD: " << fd << ")" << endl;
	#endif
	
	for (int i=0; i < evtrigr_size)
		if (evtrigr[i].ident == fd)
			return evtrigr[i].filter;
	
	return 0;
}

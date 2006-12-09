/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   
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

#include "../shared/templates.h"
#include "../shared/sockets.h"
#include "event.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include <memory> // memcpy()
//#include <cassert> // assert()

Event::Event()
	: read(0x01), write(0x02), sigmask(0)
{
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

Event::~Event() throw()
{
	#ifndef NDEBUG
	std::cout << "~Event()" << std::flush << std::endl;
	#endif

	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

int Event::inSet(int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return ( ev == read ? 0 : ev == write ? 1 : -1);
	#endif // EV_*
}


void Event::init() throw()
{
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	FD_ZERO(&fds[inSet(read)]);
	FD_ZERO(&fds[inSet(write)]);
	#endif // EV_*
}

void Event::finish() throw()
{
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

EvList Event::getEvents( int count ) const
{
	EvList ls;
}

int Event::wait(uint32_t secs, uint32_t nsecs) throw()
{
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	
	#ifdef HAVE_SELECT_COPY
	FD_COPY(&fds[read], &t_fds[read]),
	FD_COPY(&fds[write], &t_fds[write]);
	#else
	memcpy(&t_fds[inSet(read)], &fds[inSet(read)], sizeof(fd_set)),
	memcpy(&t_fds[inSet(write)], &fds[inSet(write)], sizeof(fd_set));
	#endif // HAVE_SELECT_COPY
	
	int nfds=0;
	
	#if defined(EV_SELECT)
	timeval tv;
	#elif defined(EV_PSELECT)
	timespec tv;
	#endif // EV_[P]SELECT
	
	tv.tv_sec = secs;
	
	#if defined(EV_SELECT)
	tv.tv_usec = nsecs / 1000; // microseconds
	
	return select(
		nfds,
		&fds[inSet(read)],
		&fds[inSet(write)],
		NULL,
		&tv);
	#elif defined(EV_PSELECT)
	tv.tv_nsec = nsecs;
	
	sigset_t sigsaved;
	sigprocmask(SIG_SETMASK, sigmask, &sigsaved); // save mask
	
	return pselect(
		nfds,
		&fds[inSet(read)],
		&fds[inSet(write)],
		NULL,
		&tv,
		&sigmask);
	
	sigprocmask(SIG_SETMASK, &sigsaved, NULL); // restore mask
	#endif // EV_[P]SELECT
	
	return nfds;
	#endif // EV_*
}

int Event::add(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert(fd != INVALID_SOCKET);
	
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read)) FD_SET(fd, &fds[read]);
	if (fIsSet(ev, write)) FD_SET(fd, &fds[write]);
	#endif // EV_*
	
	EventInfo i;
	i.fd = fd;
	i.events = ev;
	fd_list.push_back( i );
	
	return true;
}

int Event::remove(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert(fd != INVALID_SOCKET);
	
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read)) FD_CLR(fd, &fds[read]);
	if (fIsSet(ev, write)) FD_CLR(fd, &fds[write]);
	#endif // EV_*

	EventInfo i;
	i.fd = fd;
	i.events = ev;
	fd_list.push_back( i );
	
	return true;
}

int Event::isset(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write );
	assert(fd != INVALID_SOCKET);
	
	#if defined(EV_EPOLL)
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return (FD_ISSET(fd, &t_fds[ev]) != 0);
	#endif // EV_*
}

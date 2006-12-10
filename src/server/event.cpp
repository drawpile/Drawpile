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

#include "../shared/templates.h"
#include "event.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include <memory> // memcpy()
//#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const int
	//! identifier for 'read' event
	Event::read
		#if defined (EV_EPOLL)
		= EPOLLIN,
		#else
		= 0x01,
		#endif
	//! identifier for 'write' event
	Event::write
		#if defined (EV_EPOLL)
		= EPOLLOUT;
		#else
		= 0x02;
		#endif

Event::Event()
	#if defined( EV_USE_SIGMASK)
	: _sigmask(0)
	#endif
{
	#ifndef NDEBUG
	std::cout << "Event()" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	// needs nothing
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

Event::~Event() throw()
{
	#ifndef NDEBUG
	std::cout << "~Event()" << std::endl;
	#endif

	#if defined(EV_EPOLL)
	// needs nothing
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

int Event::inSet(int ev) throw()
{
	assert( ev == read or ev == write );
	
	#ifndef NDEBUG
	//std::cout << "Event::inSet(" << ev << ")" << std::flush << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	// needs nothing
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return ( ev == read ? 0 : ev == write ? 1 : -1);
	#endif // EV_*
}


void Event::init()
{
	#ifndef NDEBUG
	std::cout << "Event::init()" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	evfd = epoll_create(10);
	events = new epoll_events[10];
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	FD_ZERO(&fds[inSet(read)]);
	FD_ZERO(&fds[inSet(write)]);
	#endif // EV_*
}

void Event::finish() throw()
{
	#ifndef NDEBUG
	std::cout << "Event::finish()" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	delete [] events;
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

EvList Event::getEvents( int count ) const
{
	#ifndef NDEBUG
	std::cout << "Event::getEvents(" << count << ")" << std::endl;
	#endif
	
	EvList ls;
}

int Event::wait(uint32_t msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::wait(" << msecs << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	// timeout in milliseconds
	epoll_wait(evfd, events, 10, msecs);
	#elif defined(EV_KQUEUE)
	// 
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	
	#ifdef HAVE_SELECT_COPY
	FD_COPY(&fds[inSet(read)], &t_fds[inSet(read)]),
	FD_COPY(&fds[inSet(write)], &t_fds[inSet(write)]);
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
	
	tv.tv_sec = 0;
	
	#if defined(EV_SELECT)
	tv.tv_usec = msecs * 1000; // microseconds
	
	return select(
		nfds,
		&t_fds[inSet(read)],
		&t_fds[inSet(write)],
		NULL,
		&tv);
	#elif defined(EV_PSELECT)
	tv.tv_nsec = msecs * 1000000;
	
	sigset_t sigsaved;
	sigprocmask(SIG_SETMASK, _sigmask, &sigsaved); // save mask
	
	return pselect(
		nfds,
		&t_fds[inSet(read)],
		&t_fds[inSet(write)],
		NULL,
		&tv,
		_sigmask);
	
	sigprocmask(SIG_SETMASK, &sigsaved, NULL); // restore mask
	#endif // EV_[P]SELECT
	
	return nfds;
	#endif // EV_*
}

int Event::add(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::add(" << fd << ", " << ev << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = ev;
	
	epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev);
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read)) FD_SET(fd, &fds[inSet(read)]);
	if (fIsSet(ev, write)) FD_SET(fd, &fds[inSet(write)]);
	#endif // EV_*
	
	EventInfo i;
	i.fd = fd;
	i.events = ev;
	fd_list.push_back( i );
	
	return true;
}

int Event::modify(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#if defined(EV_EPOLL)
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = ev;
	
	epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev);
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// too complicated for now.
	#endif // EV_*
	
	return 0;
}

int Event::remove(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::remove(" << fd << ", " << ev << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read)) FD_CLR(fd, &fds[inSet(read)]);
	if (fIsSet(ev, write)) FD_CLR(fd, &fds[inSet(write)]);
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
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::isset(" << fd << ", " << ev << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return (FD_ISSET(fd, &t_fds[ev]) != -1);
	#endif // EV_*
}

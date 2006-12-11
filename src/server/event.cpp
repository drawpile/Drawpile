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

#include <ios>
#include <cerrno>
#include <memory> // memcpy()
#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const int
	//! identifier for 'read' event
	Event::read
		#if defined( EV_EPOLL )
		= EPOLLIN,
		#else // EV_[P]SELECT
		= 0x01,
		#endif // EV_*
	//! identifier for 'write' event
	Event::write
		#if defined( EV_EPOLL )
		= EPOLLOUT;
		#else // EV_[P]SELECT
		= 0x02;
		#endif // EV_*

Event::Event()
	#if defined( EV_EPOLL )
	: evfd(0),
	events(0)
	#elif (defined( EV_SELECT ) or defined( EV_PSELECT )) and !defined( WIN32 )
	: nfds_r(0),
	nfds_w(0)
	#if defined( EV_USE_SIGMASK )
	, _sigmask(0)
	#endif // EV_USE_SIGMASK
	#endif // EV_*
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
	// call finish() before destructing!
	assert(events == 0);
	assert(evfd == -1);
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

int Event::inSet(const int ev) const throw()
{
	assert( ev == read or ev == write );
	
	#ifndef NDEBUG
	//std::cout << "Event::inSet(" << ev << ")" << std::flush << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	// needs nothing
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return ( ev == read ? 0 : 1 );
	#endif // EV_*
}


void Event::init()
{
	#ifndef NDEBUG
	std::cout << "Event::init()" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	evfd = epoll_create(10);
	error = errno;
	if (evfd == -1)
	{
		switch (error)
		{
		case EINVAL:
			std::cerr << "Size not positive." << std::endl;
			assert(1);
			break;
		case ENFILE:
			std::cerr << "System open FD limit reached." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Unknown error." << std::endl;
			assert(1);
			break;
		}
	}
	
	events = new epoll_event[10];
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
	events = 0;
	close(evfd);
	evfd = -1;
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// needs nothing
	#endif // EV_*
}

EvList Event::getEvents() const throw()
{
	#ifndef NDEBUG
	std::cout << "Event::getEvents()" << std::endl;
	#endif
	
	EvList ls;
	ls.reserve( nfds );
	
	#if defined( EV_EPOLL )
	for (int n=0; n != nfds; n++)
	{
		/*
		EventInfo evinfo;
		evinfo.events = events[n].events;
		evinfo.fd = events[n].data.fd;
		*/
		ls.push_back( EventInfo(events[n].data.fd, events[n].events) );
	}
	#else
	#endif
	
	return ls;
}

int Event::wait(uint32_t msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::wait(" << msecs << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	// timeout in milliseconds
	nfds = epoll_wait(evfd, events, 10, msecs);
	error = errno;
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
	
	#ifndef WIN32
	nfds = (nfds_w > nfds_r ? nfds_w : nfds_r) + 1;
	#else
	nfds = 0;
	#endif // !WIN32
	
	#if defined(EV_SELECT)
	timeval tv;
	#elif defined(EV_PSELECT)
	timespec tv;
	#endif // EV_[P]SELECT
	
	tv.tv_sec = 0;
	
	#if defined(EV_SELECT)
	tv.tv_usec = msecs * 1000; // microseconds
	
	nfds = select(
		nfds,
		&t_fds[inSet(read)],
		&t_fds[inSet(write)],
		NULL,
		&tv);
	
	error = errno;
	#elif defined(EV_PSELECT)
	tv.tv_nsec = msecs * 1000000;
	
	sigset_t sigsaved;
	sigprocmask(SIG_SETMASK, _sigmask, &sigsaved); // save mask
	
	nfds = pselect(
		nfds,
		&t_fds[inSet(read)],
		&t_fds[inSet(write)],
		NULL,
		&tv,
		_sigmask);
	
	error = errno;
	
	sigprocmask(SIG_SETMASK, &sigsaved, NULL); // restore mask
	#endif // EV_[P]SELECT
	
	if (nfds == -1)
	{
		switch (error)
		{
		#if defined(EV_SELECT) or defined(EV_PSELECT)
		case EBADF:
			std::cerr << "Bad FD in set." << std::endl;
			break;
		case EINTR:
			std::cerr << "Interrupted by signal." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Timeout or sigmask invalid." << std::endl;
			assert(1);
			break;
		#elif defined(EV_EPOLL)
		case EBADF:
			std::cerr << "Bad epoll FD." << std::endl;
			assert(1);
			break;
		case EFAULT:
			std::cerr << "Events not writable." << std::endl;
			assert(1);
			break;
		case EINTR:
			std::cerr << "Interrupted by signal/timeout." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Epoll FD is not an epoll FD.. or maxevents is <= 0" << std::endl;
			assert(1);
			break;
		#endif
		default:
			std::cerr << "Unknown error." << std::endl;
			break;
		}
	}
	
	return nfds;
	#endif // EV_*
}

int Event::add(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::add(" << fd << ", ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev);
	error = errno;
	if (r == -1)
	{
		switch (error)
		{
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		case EEXIST:
			std::cerr << "FD already in set." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		}
	}
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read)) 
	{
		std::cout << "set read" << std::endl;
		FD_SET(fd, &fds[inSet(read)]);
		#ifndef WIN32
		select_set_r.insert(select_set_r.end(), fd);
		std::cout << nfds_r << " -> ";
		nfds_r = *(--select_set_r.end());
		std::cout << nfds_r << std::endl;
		#endif
	}
	if (fIsSet(ev, write))
	{
		std::cout << "set write" << std::endl;
		FD_SET(fd, &fds[inSet(write)]);
		#ifndef WIN32
		select_set_w.insert(select_set_w.end(), fd);
		std::cout << nfds_w << " -> ";
		nfds_w = *(--select_set_w.end());
		std::cout << nfds_w << std::endl;
		#endif
	}
	#else
	#endif // EV_*
	
	/*
	EventInfo i;
	i.fd = fd;
	i.events = ev;
	fd_list.push_back( i );
	*/
	
	return true;
}

int Event::modify(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::modify(" << fd << ", ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev);
	error = errno;
	if (r == -1)
	{
		switch (error)
		{
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		case EEXIST:
			std::cerr << "FD already in set." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		}
	}
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	// act like a wrapper.
	if (fIsSet(ev, read) || fIsSet(ev, write))
		add(fd, ev);
	
	if (!fIsSet(ev, read))
		remove(fd, read);
	if (!fIsSet(ev, write))
		remove(fd, write);
	#endif // EV_*
	
	return 0;
}

int Event::remove(uint32_t fd, int ev) throw()
{
	assert( ev == read or ev == write or ev == read|write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::remove(" << fd << ", ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	error = errno;
	if (r == -1)
	{
		switch (error)
		{
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		case EEXIST:
			std::cerr << "FD already in set." << std::endl;
			break;
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		}
	}
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	if (fIsSet(ev, read))
	{
		FD_CLR(fd, &fds[inSet(read)]);
		#ifndef WIN32
		select_set_r.erase(fd);
		std::cout << nfds_r << " -> ";
		nfds_r = *--select_set_r.end();
		std::cout << nfds_r << std::endl;
		#endif
	}
	if (fIsSet(ev, write))
	{
		FD_CLR(fd, &fds[inSet(write)]);
		#ifndef WIN32
		select_set_w.erase(fd);
		std::cout << nfds_w << " -> ";
		nfds_w = *--select_set_w.end();
		std::cout << nfds_w << std::endl;
		#endif
	}
	#endif // EV_*
	
	/*
	EventInfo i;
	i.fd = fd;
	i.events = ev;
	fd_list.push_back( i );
	*/
	
	return true;
}

bool Event::isset(uint32_t fd, int ev) const throw()
{
	assert( ev == read or ev == write );
	assert( fd > 0 );
	
	#ifndef NDEBUG
	std::cout << "Event::isset(" << fd << ", ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	#if defined(EV_EPOLL)
	for (int n=0; n != nfds; n++)
	{
		if (events[n].data.fd == fd)
		{
			if (fIsSet(events[n].events, ev))
				return true;
			else
			{
				#ifndef NDEBUG
				std::cout << "Descriptor in set, but not for that event." << std::endl;
				#endif
			}
		}
	}
	// no equivalent :<
	#elif defined(EV_KQUEUE)
	#elif defined(EV_PSELECT) or defined(EV_SELECT)
	return (FD_ISSET(fd, &t_fds[inSet(ev)]) != 0);
	#endif // EV_*
	
	return false;
}

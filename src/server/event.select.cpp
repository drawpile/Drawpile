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

#include "../shared/templates.h"
#include "event.h"

#include <iostream>

#include <ios>
#include <cerrno>
#include <memory> // memcpy()
#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const int
	Event::read = 0x01,
	Event::write = 0x02,
	Event::error = 0x04,
	Event::hangup = 0x04;

Event::Event() throw()
	#if !defined(WIN32)
	: nfds_r(0),
	nfds_w(0),
	nfds_e(0)
	#if defined(EV_PSELECT)
	, _sigmask(0)
	#endif // EV_PSELECT
	#endif // !WIN32
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event()" << std::endl;
	#endif
	#endif
}

Event::~Event() throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "~Event()" << std::endl;
	#endif
	#endif
}

bool Event::init() throw(std::bad_alloc)
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::init()" << std::endl;
	#endif
	#endif
	
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
	
	return true;
}

void Event::finish() throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::finish()" << std::endl;
	#endif
	#endif
}

int Event::wait(uint32_t msecs) throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::wait(msecs: " << msecs << ")" << std::endl;
	#endif
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r),
	FD_COPY(&fds_w, &t_fds_w);
	FD_COPY(&fds_e, &t_fds_e);
	#else
	memcpy(&t_fds_r, &fds_r, sizeof(fd_set)),
	memcpy(&t_fds_w, &fds_w, sizeof(fd_set));
	memcpy(&t_fds_e, &fds_e, sizeof(fd_set));
	#endif // HAVE_SELECT_COPY
	
	#if defined(EV_SELECT)
	timeval tv;
	tv.tv_usec = msecs * 1000; // microseconds
	#elif defined(EV_PSELECT)
	timespec tv;
	tv.tv_nsec = msecs * 1000000;
	
	sigset_t sigsaved;
	sigprocmask(SIG_SETMASK, _sigmask, &sigsaved); // save mask
	#endif // EV_[P]SELECT
	
	tv.tv_sec = 0;
	
	#ifndef WIN32
	int largest_nfds = (nfds_w > nfds_r ? nfds_w : nfds_r );
	if (largest_nfds < nfds_e) largest_nfds = nfds_e;
	#endif
	
	nfds =
	#if defined(EV_SELECT)
		select(
	#elif defined(EV_PSELECT)
	nfds = pselect(
	#endif
	#ifdef WIN32
		0,
	#else
		largest_nfds + 1,
	#endif
		&t_fds_r,
		&t_fds_w,
		&t_fds_e,
		&tv
	#if defined(EV_PSELECT)
		, _sigmask
	#endif
		);
	
	_error = errno;
	
	#if defined(EV_PSELECT)
	sigprocmask(SIG_SETMASK, &sigsaved, NULL); // restore mask
	#endif // EV_[P]SELECT
	
	if (nfds == -1)
	{
		switch (_error)
		{
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			std::cerr << "Bad FD in set." << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Timeout or sigmask invalid." << std::endl;
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			break;
		default:
			std::cerr << "Unknown error." << std::endl;
			break;
		}
	}
	
	return nfds;
}

int Event::add(int fd, int ev) throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	#endif
	
	assert( fd >= 0 );
	
	if (fIsSet(ev, read)) 
	{
		#ifdef DEBUG_EVENTS
		#ifndef NDEBUG
		std::cout << "set read" << std::endl;
		#endif
		#endif // DEBUG_EVENTS
		FD_SET(fd, &fds_r);
		#ifndef WIN32
		select_set_r.insert(select_set_r.end(), fd);
		std::cout << nfds_r << " -> ";
		nfds_r = *(--select_set_r.end());
		std::cout << nfds_r << std::endl;
		#endif
	}
	if (fIsSet(ev, write))
	{
		#ifdef DEBUG_EVENTS
		#ifndef NDEBUG
		std::cout << "set write" << std::endl;
		#endif
		#endif
		FD_SET(fd, &fds_w);
		#ifndef WIN32
		select_set_w.insert(select_set_w.end(), fd);
		std::cout << nfds_w << " -> ";
		nfds_w = *(--select_set_w.end());
		std::cout << nfds_w << std::endl;
		#endif
	}
	if (fIsSet(ev, error))
	{
		#ifdef DEBUG_EVENTS
		#ifndef NDEBUG
		std::cout << "set error" << std::endl;
		#endif
		#endif
		FD_SET(fd, &fds_e);
		#ifndef WIN32
		select_set_e.insert(select_set_e.end(), fd);
		#if 0
		std::cout << nfds_e << " -> ";
		#endif // 0
		nfds_e = *(--select_set_e.end());
		#if 0
		std::cout << nfds_e << std::endl;
		#endif // 0
		#endif
	}
	
	return true;
}

int Event::modify(int fd, int ev) throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	#endif
	
	assert( fd >= 0 );
	
	// act like a wrapper.
	if (fIsSet(ev, read) || fIsSet(ev, write) || fIsSet(ev, error))
		add(fd, ev);
	
	if (!fIsSet(ev, read))
		remove(fd, read);
	if (!fIsSet(ev, write))
		remove(fd, write);
	if (!fIsSet(ev, error))
		remove(fd, error);
	
	return 0;
}

int Event::remove(int fd, int ev) throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	#endif
	
	assert( fd >= 0 );
	
	if (fIsSet(ev, read))
	{
		FD_CLR(fd, &fds_r);
		#ifndef WIN32
		select_set_r.erase(fd);
		#if 0
		std::cout << nfds_r << " -> ";
		#endif // 0
		nfds_r = (select_set_r.size() > 0 ? *(--select_set_r.end()) : 0);
		#if 0
		std::cout << nfds_r << std::endl;
		#endif // 0
		#endif
	}
	
	if (fIsSet(ev, write))
	{
		FD_CLR(fd, &fds_w);
		#ifndef WIN32
		select_set_w.erase(fd);
		#if 0
		std::cout << nfds_w << " -> ";
		#endif // 0
		nfds_w = (select_set_w.size() > 0 ? *(--select_set_w.end()) : 0);
		#if 0
		std::cout << nfds_w << std::endl;
		#endif // 0
		#endif
	}
	
	if (fIsSet(ev, error))
	{
		FD_CLR(fd, &fds_e);
		#ifndef WIN32
		select_set_e.erase(fd);
		#if 0
		std::cout << nfds_e << " -> ";
		#endif // 0
		nfds_w = (select_set_e.size() > 0 ? *(--select_set_e.end()) : 0);
		#if 0
		std::cout << nfds_e << std::endl;
		#endif // 0
		#endif
	}
	
	return true;
}

bool Event::isset(int fd, int ev) const throw()
{
	#ifdef DEBUG_EVENTS
	#ifndef NDEBUG
	std::cout << "Event::isset(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	#endif
	
	assert( fd >= 0 );
	switch (ev)
	{
	case read:
		return (FD_ISSET(fd, &t_fds_r) != 0);
	case write:
		return (FD_ISSET(fd, &t_fds_w) != 0);
	case error:
		return (FD_ISSET(fd, &t_fds_e) != 0);
	default:
		assert(!"switch(ev) defaulted even when it should be impossible");
		return false;
	}
}

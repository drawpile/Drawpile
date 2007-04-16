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

#include "../config.h"
#include "../shared/templates.h"
#include "event.h"

#ifndef EV_SELECT
	#error EV_SELECT not defined
#endif

#ifndef NDEBUG
	#include <iostream>
	#include <ios>
#endif

#include <cerrno> // errno
#include <memory> // memcpy()
#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const uint32_t
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
	, _sigmask(0), _sigsaved(0)
	#endif // EV_PSELECT
	#endif // !WIN32
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select)()" << std::endl;
	#endif
}

Event::~Event() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "~Event(select)()" << std::endl;
	#endif
}

bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).init()" << std::endl;
	#endif
	
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).finish()" << std::endl;
	#endif
}

int Event::wait() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).wait()" << std::endl;
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r),
	FD_COPY(&fds_w, &t_fds_w),
	FD_COPY(&fds_e, &t_fds_e);
	#else
	memcpy(&t_fds_r, &fds_r, sizeof(fd_set)),
	memcpy(&t_fds_w, &fds_w, sizeof(fd_set)),
	memcpy(&t_fds_e, &fds_e, sizeof(fd_set));
	#endif // HAVE_SELECT_COPY
	
	#if defined(EV_PSELECT)
	// save sigmask
	sigprocmask(SIG_SETMASK, _sigmask, _sigsaved);
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
		, _sigmask
	#endif // EV_PSELECT
		);
	
	#ifdef WIN32
	_error = WSAGetLastError();
	#else
	_error = errno;
	#endif
	
	#if defined(EV_PSELECT)
	// restore mask
	sigprocmask(SIG_SETMASK, &sigsaved, NULL);
	#endif // EV_PSELECT
	
	if (nfds == -1)
	{
		#ifdef WIN32
		assert(_error != WSANOTINITIALISED);
		#endif
		assert(_error != EBADF);
		assert(_error != ENOTSOCK);
		assert(_error != EINVAL);
		assert(_error != EFAULT);
		
		switch (_error)
		{
		#ifdef WIN32
		case WSAENETDOWN:
			#ifndef NDEBUG
			std::cerr << "The network subsystem has failed." << std::endl;
			#endif
			break;
		#endif
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal." << std::endl;
			#endif
			nfds = 0;
			break;
		default:
			std::cerr << "Event(select).wait() Unknown error: " << _error << std::endl;
			break;
		}
	}
	else if (nfds > 0)
	{
		fd_iter = fd_list.begin();
	}
	
	return nfds;
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	bool rc=false;
	
	if (fIsSet(ev, read))
	{
		FD_SET(fd, &fds_r);
		#ifndef WIN32 // win32 ignores the argument
		read_set.insert(read_set.end(), fd);
		nfds_r = *(--read_set.end());
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(ev, write))
	{
		FD_SET(fd, &fds_w);
		#ifndef WIN32 // win32 ignores the argument
		write_set.insert(write_set.end(), fd);
		nfds_w = *(--write_set.end());
		#endif // !Win32
		rc = true;
	}
	if (fIsSet(ev, error))
	{
		FD_SET(fd, &fds_e);
		#ifndef WIN32 // win32 ignores the argument
		error_set.insert(error_set.end(), fd);
		nfds_e = *(--error_set.end());
		#endif // !Win32
		rc = true;
	}
	
	// maintain fd_list
	fd_list[fd] = ev;
	
	return rc;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event::modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	// act like a wrapper.
	if (fIsSet(ev, read) or fIsSet(ev, write) or fIsSet(ev, error))
		add(fd, ev);
	
	if (!fIsSet(ev, read))
		remove(fd, read);
	
	if (!fIsSet(ev, write))
		remove(fd, write);
	
	if (!fIsSet(ev, error))
		remove(fd, error);
	
	return 0;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<fd_t,uint32_t>::iterator iter(fd_list.find(fd));
	#else
	std::map<fd_t,uint32_t>::iterator iter(fd_list.find(fd));
	#endif
	if (iter == fd_list.end())
		return false;
	
	if (fIsSet(ev, read))
	{
		FD_CLR(fd, &fds_r);
		#ifndef WIN32
		read_set.erase(fd);
		nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : 0);
		#endif // WIN32
		
		fClr(iter->second, read);
	}
	
	if (fIsSet(ev, write))
	{
		FD_CLR(fd, &fds_w);
		#ifndef WIN32
		write_set.erase(fd);
		nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : 0);
		#endif // WIN32
		
		fClr(iter->second, write);
	}
	
	if (fIsSet(ev, error|hangup))
	{
		FD_CLR(fd, &fds_e);
		#ifndef WIN32
		error_set.erase(fd);
		nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : 0);
		#endif // WIN32
		
		fClr(iter->second, error);
	}
	
	if (iter->second == 0)
	{
		fd_iter = iter;
		fd_list.erase(iter);
		fd_iter = fd_list.begin();
		/*
		if (fd_iter != fd_list.begin())
		{
			//--fd_iter; // make sure the iterator doesn't get invalidated
			fd_list.erase(iter);
			fd_iter = fd_list.begin();
			//++fd_iter; // increment to the next
		}
		else
		{
			fd_list.erase(iter);
			fd_iter = fd_list.end();
		}
		*/
	}
	
	return true;
}

bool Event::getEvent(fd_t &fd, uint32_t &events) throw()
{
	while (fd_iter != fd_list.end())
	{
		fd = fd_iter->first;
		++fd_iter;
		
		events = getEvents(fd);
		
		if (events == 0)
			continue;
		else
			return true;
	}
	
	return false;
}

uint32_t Event::getEvents(fd_t fd) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).getEvents(fd: " << fd << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	uint32_t events = 0;
	
	if (FD_ISSET(fd, &t_fds_r) != 0)
		fSet(events, read);
	if (FD_ISSET(fd, &t_fds_w) != 0)
		fSet(events, write);
	if (FD_ISSET(fd, &t_fds_e) != 0)
		fSet(events, error);
	
	return events;
}

bool Event::isset(fd_t fd, uint32_t ev) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(select).isset(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
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

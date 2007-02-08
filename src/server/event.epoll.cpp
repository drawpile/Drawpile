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
#ifndef NDEBUG
	#include <ios>
#endif

#include <cerrno> // errno
#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const uint32_t
	Event::read = EPOLLIN,
	Event::write = EPOLLOUT,
	Event::error = EPOLLERR,
	Event::hangup = EPOLLHUP;

Event::Event() throw()
	: evfd(0)//,
	//events(0)
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
	
	assert(events == 0);
	assert(evfd == -1);
}

bool Event::init() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).init()" << std::endl;
	#endif
	
	evfd = epoll_create(10);
	_error = errno;
	if (evfd == -1)
	{
		switch (_error)
		{
		case EINVAL:
			#ifndef NDEBUG
			std::cerr << "Size not positive." << std::endl;
			#endif
			assert(1);
			break;
		case ENFILE:
			std::cerr << "System open FD limit reached." << std::endl;
			return false;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw std::bad_alloc();
			break;
		default:
			#ifndef NDEBUG
			std::cerr << "Event(epoll).init() : Unknown error" << std::endl;
			#endif
			assert(1);
			break;
		}
	}
	
	//events = new epoll_event[10];
	
	return true;
}

void Event::finish() throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).finish()" << std::endl;
	#endif
	
	//delete [] events;
	//events = 0;
	close(evfd);
	evfd = -1;
}

int Event::wait(uint32_t msecs) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).wait(msecs: " << msecs << ")" << std::endl;
	#endif
	
	// timeout in milliseconds
	nfds = epoll_wait(evfd, events, 10, msecs);
	_error = errno;
	
	if (nfds == -1)
	{
		switch (_error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Bad epoll FD." << std::endl;
			assert(1);
			break;
		case EFAULT:
			std::cerr << "Events not writable." << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Epoll FD is not an epoll FD.. or maxevents is <= 0" << std::endl;
			assert(1);
			break;
		#endif
		case EINTR:
			#ifndef NDEBUG
			std::cerr << "Interrupted by signal/timeout." << std::endl;
			#endif
			nfds = 0;
			break;
		default:
			std::cerr << "Event(epoll).wait() : Unknown error" << std::endl;
			// TODO
			break;
		}
	}
	
	return nfds;
}

int Event::add(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifndef NDEBUG
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
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		#endif
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Event(epoll).add() : Unknown error" << std::endl;
			break;
		}
	}
	
	return true;
}

int Event::modify(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		#endif
		case ENOENT:
			#ifndef NDEBUG
			std::cerr << "FD not in set." << std::endl;
			#endif
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Event(epoll).modify() : Unknown error" << std::endl;
			break;
		}
	}
	
	return 0;
}

int Event::remove(fd_t fd, uint32_t ev) throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifndef NDEBUG
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		#endif
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Event(epoll).remove() : Unknown error" << std::endl;
			break;
		}
	}
	
	return true;
}

std::pair<fd_t, uint32_t> Event::getEvent(int ev_index) const throw()
{
	return std::make_pair(events[ev_index].data.fd, events[ev_index].events);
}

uint32_t Event::getEvents(fd_t fd) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).getEvents(fd: " << fd << ")" << std::endl;
	#endif
	
	for (int n=0; n != nfds; n++)
	{
		if (events[n].data.fd == fd)
		{
			return events[n].events;
		}
	}
	
	return 0;
}

bool Event::isset(fd_t fd, uint32_t ev) const throw()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	std::cout << "Event(epoll).isset(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert(fd != INVALID_SOCKET);
	
	if (fIsSet(getEvents(fd), ev))
		return true;
	
	return false;
}

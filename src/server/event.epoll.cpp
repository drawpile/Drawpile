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

//#include "../shared/templates.h"
#include "event.h"

#include <iostream>
#ifndef NDEBUG
	#include <ios>
#endif

#include <cerrno>
#include <cassert> // assert()

/* Because MinGW is buggy, we have to do this fuglyness */
const int
	Event::read = EPOLLIN,
	Event::write = EPOLLOUT,
	Event::error = EPOLLERR,
	Event::hangup = EPOLLHUP;

Event::Event() throw()
	: evfd(0),
	events(0)
{
	#ifndef NDEBUG
	std::cout << "Event()" << std::endl;
	#endif
}

Event::~Event() throw()
{
	#ifndef NDEBUG
	std::cout << "~Event()" << std::endl;
	#endif
	
	assert(events == 0);
	assert(evfd == -1);
}

bool Event::init() throw(std::bad_alloc)
{
	#ifndef NDEBUG
	std::cout << "Event::init()" << std::endl;
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
			std::cerr << "Unknown error." << std::endl;
			#endif
			assert(1);
			break;
		}
	}
	
	events = new epoll_event[10];
	
	return true;
}

void Event::finish() throw()
{
	#ifndef NDEBUG
	std::cout << "Event::finish()" << std::endl;
	#endif
	
	delete [] events;
	events = 0;
	close(evfd);
	evfd = -1;
}

int Event::wait(uint32_t msecs) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::wait(msecs: " << msecs << ")" << std::endl;
	#endif
	
	// timeout in milliseconds
	nfds = epoll_wait(evfd, events, 10, msecs);
	_error = errno;
	
	if (nfds == -1)
	{
		switch (_error)
		{
		#if defined( TRAP_CODER_ERROR )
		case EBADF:
			#ifndef NDEBUG
			std::cerr << "Bad epoll FD." << std::endl;
			#endif
			assert(1);
			break;
		case EFAULT:
			#ifndef NDEBUG
			std::cerr << "Events not writable." << std::endl;
			#endif
			assert(1);
			break;
		case EINVAL:
			#ifndef NDEBUG
			std::cerr << "Epoll FD is not an epoll FD.. or maxevents is <= 0" << std::endl;
			#endif
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EINTR:
			std::cerr << "Interrupted by signal/timeout." << std::endl;
			break;
		default:
			std::cerr << "Unknown error." << std::endl;
			// TODO
			break;
		}
	}
	
	return nfds;
}

int Event::add(int fd, int ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::add(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifdef TRAP_CODER_ERROR
		case EBADF:
			std::cerr << "Epoll FD is invalid." << std::endl;
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		case EEXIST:
			std::cerr << "FD already in set." << std::endl;
			break;
		#ifdef TRAP_CODER_ERROR
		case EINVAL:
			std::cerr << "Epoll FD is invalid, or FD is the same as epoll FD." << std::endl;
			assert(1);
			break;
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		#endif // TRAP_CODER_ERROR
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		#ifdef TRAP_CODER_ERROR
		case EPERM:
			std::cerr << "Target FD does not support epoll." << std::endl;
			assert(1);
			break;
		#endif // TRAP_CODER_ERROR
		default:
			std::cerr << "Unkown error" << std::endl;
			break;
		}
	}
	
	return true;
}

int Event::modify(int fd, int ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::modify(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	epoll_event ev_info;
	ev_info.data.fd = fd;
	ev_info.events = ev;
	
	int r = epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev_info);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifdef TRAP_CODER_ERROR
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
		#endif // TRAP_CODER_ERROR
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Unknown error" << std::endl;
			break;
		}
	}
	
	return 0;
}

int Event::remove(int fd, int ev) throw()
{
	#ifndef NDEBUG
	std::cout << "Event::remove(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	int r = epoll_ctl(evfd, EPOLL_CTL_DEL, fd, 0);
	_error = errno;
	if (r == -1)
	{
		switch (_error)
		{
		#ifdef TRAP_CODER_ERROR
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
		#endif // TRAP_CODER_ERROR
		case ENOENT:
			std::cerr << "FD not in set." << std::endl;
			break;
		case ENOMEM:
			std::cerr << "Out of memory" << std::endl;
			throw new std::bad_alloc;
			break;
		default:
			std::cerr << "Unknown error"  << std::endl;
			break;
		}
	}
	
	return true;
}

bool Event::isset(int fd, int ev) const throw()
{
	#ifndef NDEBUG
	std::cout << "Event::isset(fd: " << fd << ", event: ";
	std::cout.setf ( std::ios_base::hex, std::ios_base::basefield );
	std::cout.setf ( std::ios_base::showbase );
	std::cout << ev;
	std::cout.setf ( std::ios_base::dec );
	std::cout.setf ( ~std::ios_base::showbase );
	std::cout << ")" << std::endl;
	#endif
	
	assert( fd >= 0 );
	
	for (int n=0; n != nfds; n++)
	{
		if (events[n].data.fd == fd)
		{
			if (events[n].events == static_cast<uint32_t>(ev))
				return true;
			else
			{
				#ifndef NDEBUG
				std::cout << "Descriptor in set, but not for that event." << std::endl;
				#endif
			}
		}
	}
	
	return false;
}

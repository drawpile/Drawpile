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

#ifndef EVENT_H_INCLUDED
#define EVENT_H_INCLUDED

#include "../../config.h"

// #include <set>
#include <stdint.h>

#include <signal.h>

//#include <sys/time.h>

#include <set>
//#include <list>
#include <vector>

#if defined(EV_EPOLL)
	#define EV_HAVE_HANGUP
	#include <sys/epoll.h>
#elif defined(EV_KQUEUE)
	#error kqueue() not implemented.
#elif defined(EV_PSELECT)
	#include <sys/select.h> // fd_set, FD* macros, etc.
#else
	#if !defined(EV_SELECT)
		#define EV_SELECT
	#endif
	
	#ifdef WIN32
		#include "sockets.h"
	#else
		#include <sys/select.h> // fd_set, FD* macros, etc.
	#endif
#endif

#if defined(EV_SELECT) || defined(EV_PSELECT)
	#define EVENT_BY_FD
#elif defined(EV_EPOLL)
	#define EVENT_BY_ORDER
	#define EVENT_HAS_ALL
#endif

#ifndef WIN32
typedef int fd_t;
#endif

#if defined( EV_PSELECT )
	#define EV_USE_SIGMASK
#endif

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

//! Event I/O abstraction
class Event
{
protected:
	#if defined(EV_EPOLL)
	fd_t evfd;
	epoll_event events[10]; // stack allocation
	#elif defined(EV_KQUEUE)
	//
	#elif defined(EV_PSELECT) || defined(EV_SELECT)
	fd_set fds_r, fds_w, fds_e, t_fds_r, t_fds_w, t_fds_e;
	
	#ifndef WIN32
	std::set<fd_t> select_set_r;
	std::set<fd_t> select_set_w;
	std::set<fd_t> select_set_e;
	
	fd_t nfds_r, nfds_w, nfds_e;
	#endif // !WIN32
	
	#endif // EV_*
	
	#if defined(EV_USE_SIGMASK)
	sigset_t *_sigmask;
	#endif // EV_USE_SIGMASK
	
	int _error, nfds;
	
public:
	
	// MinGW is buggy... think happy thoughts :D
	static const uint32_t
		//! Identifier for 'read' event
		read,
		//! Identifier for 'write' event
		write,
		//! Identifier for 'error' event
		error,
		//! Identifier for 'hangup' event
		hangup;
	
	//! ctor
	Event() throw();
	
	//! dtor
	/**
	 * finish() MUST be called before destructor!
	 */
	~Event() throw();
	
	#if defined(EV_PSELECT)
	//! Set signal mask.
	/**
	 * Only used by pselect() so far
	 */
	void setMask(sigset_t* mask) throw() { _sigmask = mask; }
	#endif // EV_PSELECT
	
	//! Initialize event system.
	bool init() throw();
	
	//! Finish event system.
	void finish() throw();
	
	//! Wait for events.
	/**
	 * @param msecs milliseconds to wait.
	 *
	 * @return number of file descriptors triggered, -1 on error, and 0 otherwise.
	 */
	int wait(uint32_t msecs) throw();
	
	//! Adds file descriptor to event polling.
	/**
	 * @param fd is the file descriptor to be added to event stack.
	 * @param ev is the events in which fd is to be found.
	 *
	 * @return true if the fd was added, false if not
	 */
	int add(fd_t fd, uint32_t ev) throw();
	
	//! Removes file descriptor from event set.
	/**
	 * @param fd is the file descriptor to be removed from a event set.
	 * @param ev is the event sets in which fd is to be found.
	 *
	 * @return true if the fd was removed, false if not (or was not part of the event set)
	 */
	int remove(fd_t fd, uint32_t ev) throw();
	
	//! Modifies previously added fd for different events.
	/**
	 * This should be preferred over add and remove!
	 *
	 * @param fd is the file descriptor to be modified.
	 * @param ev has the event flags.
	 *
	 * @return something undefined
	 */
	int modify(fd_t fd, uint32_t ev) throw();
	
	//! Fetches next triggered event.
	std::pair<fd_t, uint32_t> getEvent(int ev_index) const throw();
	
	//! Fetches triggered events for FD.
	uint32_t getEvents(fd_t fd) const throw();
	
	//! Tests if the file descriptor was triggered in event set.
	/**
	 * @param fd is the file descriptor to be tested in a set.
	 * @param ev is the fd set in which fd is to be found.
	 *
	 * @return bool
	 */
	bool isset(fd_t fd, uint32_t ev) const throw();
};

#endif // EVENT_H_INCLUDED

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

#include "../config.h"

#include <stdint.h>

#if defined(HAVE_HASH_MAP)
	#include <ext/hash_map>
#else
	#include <map>
#endif

#if defined(EV_EPOLL)
	#include <sys/epoll.h>
#elif defined(EV_KQUEUE)
	#error kqueue() not implemented.
#elif defined(EV_PSELECT)
	#include <sys/select.h> // fd_set, FD* macros, etc.
#else
	#if !defined(EV_WSA)
		#define EV_SELECT
	#endif
	
	#ifdef WIN32
		#include "sockets.h"
	#else
		#include <sys/select.h> // fd_set, FD* macros, etc.
	#endif
#endif

#if defined(EV_SELECT) or defined(EV_PSELECT)
	#if defined(HAVE_HASH_SET)
		#include <ext/hash_set>
	#else
		#include <set>
	#endif
	#define EVENT_BY_FD
#elif defined(EV_EPOLL)
	#define EV_HAVE_HANGUP
	#define EVENT_BY_INDEX
	#define EVENT_HAS_ALL
#elif defined(EV_WSA)
	#define EVENT_BY_FD
#endif

#ifndef WIN32
typedef int fd_t;
#endif

#if defined( EV_PSELECT )
	#include <signal.h>
	#define EV_USE_SIGMASK
#endif

#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

#ifndef EV_WSA
	#define EV_HAS_ERROR
#endif

// The meaning of this changes a bit with the event systems.
const uint32_t max_events =
#if defined(EV_EPOLL)
	10;
#elif defined(EV_WSA)
	WSA_MAXIMUM_WAIT_EVENTS;
#else
	std::numeric_limits<uint32_t>::max();
#endif

//! Event I/O abstraction
class Event
{
protected:
	#if defined(EV_EPOLL)
	fd_t evfd;
	epoll_event events[max_events]; // stack allocation
	#endif // EV_EPOLL
	
	#if defined(EV_PSELECT) or defined(EV_SELECT)
	fd_set fds_r, fds_w, fds_e, t_fds_r, t_fds_w, t_fds_e;
	
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<fd_t, uint32_t> fd_list; // events set for FD
	__gnu_cxx::hash_map<fd_t, uint32_t>::iterator fd_iter;
	#else
	std::map<fd_t, uint32_t> fd_list; // events set for FD
	std::map<fd_t, uint32_t>::iterator fd_iter;
	#endif
	
	#ifndef WIN32
	#if defined(HAVE_HASH_SET)
	__gnu_cxx::hash_set<fd_t> read_set, write_set, error_set;
	#else
	std::set<fd_t> read_set, write_set, error_set;
	#endif
	fd_t nfds_r, nfds_w, nfds_e;
	#endif // !WIN32
	#endif // EV_[P]SELECT
	
	#if defined(EV_USE_SIGMASK)
	sigset_t _sigmask, _sigsaved;
	#endif // EV_USE_SIGMASK
	
	#if defined(EV_WSA)
	uint32_t last_event;
	
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<fd_t, uint32_t> fd_to_ev;
	__gnu_cxx::hash_map<uint32_t, fd_t> ev_to_fd;
	__gnu_cxx::hash_map<fd_t, uint32_t>::iterator ev_iter;
	#else
	std::map<fd_t, uint32_t> fd_to_ev;
	std::map<uint32_t, fd_t> ev_to_fd;
	std::map<fd_t, uint32_t>::iterator ev_iter;
	#endif
	
	WSAEVENT w_ev[max_events];
	#endif
	
	#if defined(EV_PSELECT)
	timespec _timeout;
	#elif defined(EV_EPOLL) or defined(EV_WSA)
	uint32_t _timeout;
	#else
	timeval _timeout;
	#endif
	
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
	void setMask(const sigset_t& mask) throw() { _sigmask = mask; }
	#endif // EV_PSELECT
	
	//! Initialize event system.
	bool init() throw();
	
	//! Finish event system.
	void finish() throw();
	
	//! Set timeout for wait()
	/**
	 * @param msecs milliseconds to wait.
	 */
	void timeout(uint32_t msecs) throw()
	{
		#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
		std::cout << "Event(-).timeout(msecs: " << msecs << ")" << std::endl;
		#endif
		
		#if defined(EV_EPOLL) or defined(EV_WSA)
		_timeout = msecs;
		#else
		if (msecs > 1000)
		{
			_timeout.tv_sec = msecs/1000;
			msecs -= _timeout.tv_sec*1000;
		}
		else
		{
			_timeout.tv_sec = 0;
		}
		
		#if defined(EV_PSELECT)
		_timeout.tv_nsec = msecs * 1000000; // nanoseconds
		#else
		_timeout.tv_usec = msecs * 1000; // microseconds
		#endif // EV_PSELECT
		#endif // EV_EPOLL/WSA
	}
	
	//! Wait for events.
	/**
	 * @return number of file descriptors triggered, -1 on error, and 0 otherwise.
	 */
	int wait() throw();
	
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
	bool getEvent(fd_t &fd, uint32_t &events) throw();
	
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

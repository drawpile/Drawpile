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

#ifndef EventInterface_INCLUDED
#define EventInterface_INCLUDED

#include "config.h"
#include "../common.h"

#include "traits.h"

#include <ctime>

#ifdef WIN32
#include <winsock2.h>
typedef SOCKET fd_t;
#else
typedef int fd_t;
#endif

//! Event I/O abstraction
template <typename Evs>
class EventInterface
{
protected:
	int error; // errno;
public:
	typedef typename event_type<Evs>::ev_t ev_t;
	
	//! ctor
	EventInterface() throw() { }
	
	//! dtor
	virtual ~EventInterface() throw() { }
	
	//! Set timeout for wait()
	/**
	 * @param msecs milliseconds to wait.
	 */
	virtual void timeout(uint msecs) throw() = 0;
	
	//! Wait for events.
	/**
	 * @return number of file descriptors triggered, -1 on error, and 0 otherwise.
	 */
	virtual int wait() throw() = 0;
	
	//! Adds file descriptor to event polling.
	/**
	 * @param fd is the file descriptor to be added to event stack.
	 * @param events is the events in which fd is to be found.
	 *
	 * @return true if the fd was added, false if not
	 */
	virtual int add(fd_t fd, ev_t events) throw() = 0;
	
	//! Removes file descriptor from event set.
	/**
	 * @param fd is the file descriptor to be removed from a event set.
	 *
	 * @return true if the fd was removed, false if not (or was not part of the event set)
	 */
	virtual int remove(fd_t fd) throw() = 0;
	
	//! Modifies previously added fd for different events.
	/**
	 * @param fd is the file descriptor to be modified.
	 * @param events has the event flags.
	 *
	 * @return something undefined
	 */
	virtual int modify(fd_t fd, ev_t events) throw() = 0;
	
	//! Fetches next triggered event.
	virtual bool getEvent(fd_t &fd, ev_t &events) throw() = 0;
	
	int getError() const throw();
};

#endif // EventInterface_INCLUDED

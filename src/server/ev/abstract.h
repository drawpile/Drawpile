/*******************************************************************************

   Copyright (C) 2007, 2008 M.K.A. <wyrmchild@users.sourceforge.net>
   
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

#pragma once

#ifndef EventAbstract_H
#define EventAbstract_H

#include "config.h"
#include "../types.h"

//! Event I/O abstraction
/**
 * @see http://www.kegel.com/c10k.html The C10k problem
 */
//! Abstract event system interface
template <typename T=int>
class AbstractEventInterface
{
protected:
	 //! Last error number (errno)
	int error;
	
	//! Constructor
	AbstractEventInterface() NOTHROW;
	
	//! Destructor
	virtual ~AbstractEventInterface() NOTHROW = 0;
public:
	//! Type for events
	typedef T event_t;
	
	
	//! Set timeout for wait()
	/**
	 * @param[in] msecs milliseconds to wait.
	 */
	virtual void timeout(uint msecs) NOTHROW = 0;
	
	//! Wait for events.
	/**
	 * @return number of file descriptors triggered
	 * @return -1 on error
	 * @return 0 otherwise
	 */
	virtual int wait() NOTHROW = 0;
	
	//! Adds file descriptor to event polling.
	/**
	 * @param[in] fd is the file descriptor to be added to event stack.
	 * @param[in] events is the events in which fd is to be found.
	 *
	 * @return \b true if the FD was added
	 * @return \b false if not
	 */
	virtual int add(SOCKET fd, event_t events) = 0;
	
	//! Removes file descriptor from event set.
	/**
	 * @param[in] fd is the file descriptor to be removed from a event set.
	 *
	 * @return \b true if the fd was removed
	 * @return \b false if not (or was not part of the event set)
	 */
	virtual int remove(SOCKET fd) = 0;
	
	//! Modifies previously added fd for different events.
	/**
	 * @param[in] fd is the file descriptor to be modified.
	 * @param[in] events has the event flags.
	 *
	 * @return ?
	 */
	virtual int modify(SOCKET fd, event_t events) = 0;
	
	//! Fetches next triggered event.
	/**
	 * @param[out] fd triggered file descriptor
	 * @param[out] events triggered events
	 * 
	 * @return \b true if FD was triggered; fd and events parameters were filled
	 * @return \b false otherwise
	 */
	virtual bool getEvent(SOCKET &fd, event_t &events) NOTHROW = 0;
	
	//! Get last error number (errno)
	/**
	 * @return last errno
	 */
	int getError() const NOTHROW;
};

template <typename T> AbstractEventInterface<T>::AbstractEventInterface()
	: error(0)
{ }

template <typename T> AbstractEventInterface<T>::~AbstractEventInterface() { /* Abstract */ }

template <typename T>
int AbstractEventInterface<T>::getError() const
{
	return error;
}

#endif // EventAbstract_H

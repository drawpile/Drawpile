/*******************************************************************************

   Copyright (C) 2006, 2007, 2008 M.K.A. <wyrmchild@users.sourceforge.net>
   
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

#include "select.h"

#include "../../shared/templates.h"

#ifndef WIN32
	#include <cerrno> // errno
#endif
#include <cassert> // assert()

#include "../socket.h"

Event::Event()
	: triggered(-1)
	#ifndef WIN32
	, highestReadFD(Socket::InvalidHandle)
	, highestWriteFD(Socket::InvalidHandle)
	#endif
{
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
}

Event::~Event()
{
}

void Event::addToSet(fd_set &fdset, SOCKET fd
	#ifndef WIN32
	, std::set<SOCKET>& l_set, SOCKET &largest
	#endif
)
{
	FD_SET(fd, &fdset);
	#ifndef WIN32
	l_set.insert(l_set.end(), fd);
	if (fd > largest) largest = fd;
	#endif
}

void Event::removeFromSet(fd_set &fdset, SOCKET fd
	#ifndef WIN32
	, std::set<SOCKET>& l_set, SOCKET &largest
	#endif
)
{
	FD_CLR(fd, &fdset);
	#ifndef WIN32
	l_set.erase(fd);
	if (fd == largest)
		largest = (l_set.size() > 0 ? *(l_set.rend()) : Socket::InvalidHandle);
	#endif
}

// Errors: NetSubsystemDown
int Event::wait()
{
	#ifdef EV_SELECT_COPY
	FD_COPY(&readSet, &t_readSet);
	FD_COPY(&writeSet, &t_writeSet);
	#else
	t_readSet = readSet;
	t_writeSet = writeSet;
	#endif // HAVE_SELECT_COPY
	
	#ifdef WIN32
	SOCKET highestFD = 0; // not used
	#else
	SOCKET highestFD = std::max(highestWriteFD, highestReadFD);
	if (highestFD != Socket::InvalidHandle)
		highestFD++; // highest_fd + 1 expected
	#endif
	
	triggered = select(highestFD, &t_readSet, &t_writeSet, 0, &_timeout);
	
	switch (triggered)
	{
		case -1:
			#ifdef WIN32
			error = WSAGetLastError();
			#else
			error = errno;
			#endif
			
			if (error == EINTR)
				return triggered = 0;
			
			#ifdef WIN32
			assert(error != WSANOTINITIALISED);
			assert(error != WSAEBADF);
			assert(error != WSAENOTSOCK);
			assert(error != WSAEINVAL);
			assert(error != WSAEFAULT);
			#else
			assert(error != EBADF);
			assert(error != ENOTSOCK);
			assert(error != EINVAL);
			assert(error != EFAULT);
			#endif
			
			#if defined(WIN32)
			/** @todo Do we need to do anything here? */
			if (error == NetSubsystemDown)
				return 0;
			#endif
			break;
		case 0:
			// do nothing
			break;
		default:
			fd_iter = fd_list.begin();
			break;
	}
	
	return triggered;
}

int Event::add(SOCKET fd, event_t events)
{
	assert(fd != Socket::InvalidHandle);
	
	bool rc=false;
	
	if (events & Read)
	{
		addToSet(readSet, fd
			#ifndef WIN32
			, readList, highestReadFD
			#endif
		);
		rc = true;
	}
	
	if (events & Write)
	{
		addToSet(writeSet, fd
			#ifndef WIN32
			, writeSet, highestWriteFD
			#endif
		);
		rc = true;
	}
	
	assert(rc);
	
	// maintain fd_list
	fd_list.insert(std::pair<SOCKET,event_t>(fd,events));
	
	return rc;
}

int Event::modify(SOCKET fd, event_t events)
{
	assert(fd != Socket::InvalidHandle);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (events & Read)
		removeFromSet(readSet, fd
			#ifndef WIN32
			, readList, highestReadFD
			#endif
		);
	
	if (!(events & Write))
		removeFromSet(writeSet, fd
			#ifndef WIN32
			, writeList, highestWriteFD
			#endif
		);
	
	return 0;
}

int Event::remove(SOCKET fd)
{
	assert(fd != Socket::InvalidHandle);
	
	std::map<SOCKET,uint>::iterator iter(fd_list.find(fd));
	assert(iter != fd_list.end());
	
	removeFromSet(readSet, fd
		#ifndef WIN32
		, readList, highestReadFD
		#endif
	);
	removeFromSet(writeSet, fd
		#ifndef WIN32
		, writeList, highestWriteFD
		#endif
	);
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool Event::getEvent(SOCKET &fd, event_t &events)
{
	assert(fd_list.size() > 0);
	
	while (fd_iter != fd_list.end())
	{
		fd = (fd_iter++)->first;
		
		events = 0;
		
		if (FD_ISSET(fd, &t_readSet) != 0)
			events |= Read;
		if (FD_ISSET(fd, &t_writeSet) != 0)
			events |= Write;
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void Event::timeout(uint msecs)
{
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_usec = msecs * 1000;
}

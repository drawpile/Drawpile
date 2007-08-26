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

#include "wsa.h"

#include "../../shared/templates.h" // fIsSet() and friends

#include "../errors.h"
#include "../socket.errors.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif

#include <cassert> // assert()

namespace event {

const ev_t read<WSA>::value = POLLRDNORM;
const ev_t write<WSA>::value = POLLWRNORM;
const std::string system<WSA>::value("wsapoll");

using namespace socket_error;

const SOCKET invalid_fd<WSA>::value = InvalidHandle;

WSAPoll::WSAPoll()
	: nfds(0), t_event(0), limit(8)
{
	descriptors = new WSAPOLLFD[limit];
	// make sure the new pollfds are invalid
	for (int i=0; i != limit; i++)
	{
		descriptors[i].fd = InvalidDescriptor;
		descriptors[i].events = 0;
	}
}

WSAPoll::~WSAPoll()
{
	delete [] descriptors;
}

int WSAPoll::wait()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsapoll.wait()" << endl;
	#endif
	
	assert(fd_to_index.size() != 0);
	
	nfds = ::WSAPoll(descriptors, fd_to_index.size(), m_timeout);
	if (nfds == Error)
	{
		error = WSAGetLastError();
		
		assert(error != Fault);
		assert(error != WSAEINVAL);
		
		return -1;
	}
	else
		return nfds;
}

int WSAPoll::add(fd_t fd, ev_t events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsapoll.add(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != InvalidHandle);
	
	#ifndef NDEBUG
	// make sure the fd is not already there
	index_i itr = fd_to_index.find(fd);
	assert(itr == fd_to_index.end());
	#endif
	
	if (fd_to_index.size() == limit)
	{
		#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
		cout << "wsapoll / increasing pollfd array size to " << limit*2 << endl;
		#endif
		
		WSAPOLLFD *ndesc = new WSAPOLLFD[limit*2];
		memcpy(ndesc, descriptors, limit);
		delete [] descriptors;
		descriptors = ndesc;
		int i = limit;
		limit *= 2;
		
		// make sure the new pollfds are invalid
		for (; i != limit; i++)
		{
			descriptors[i].fd = InvalidDescriptor;
			descriptors[i].events = 0;
		}
	}
	
	for (int i=0; i != limit; i++)
		if (descriptors[i].fd != InvalidSocket)
		{
			descriptors[i].fd = fd;
			descriptors[i].events = events;
			fd_to_index.insert(std::pair<fd_t,int>(fd,i));
			return true;
		}
	
	return false;
}

int WSAPoll::modify(fd_t fd, ev_t events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsapoll.modify(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != InvalidHandle);
	
	index_i i = fd_to_index.find(fd);
	
	if (i != fd_to_index.end())
	{
		assert(descriptors[i->second].fd == fd);
		descriptors[i->second].events = events;
		return true;
	}
	else
		return false;
}

int WSAPoll::remove(fd_t fd)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "wsapoll.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != InvalidHandle);
	
	index_i i = fd_to_index.find(fd);
	
	if (i != fd_to_index.end())
	{
		descriptors[i->second].fd = InvalidSocket;
		descriptors[i->second].events = 0;
		fd_to_index.erase(i);
	}
	
	return true;
}

bool WSAPoll::getEvent(fd_t &fd, ev_t &events)
{
	for (; t_event != limit; t_event++)
		if (descriptors[t_event].revents != 0)
		{
			fd = descriptors[t_event].fd;
			events = descriptors[t_event].revents;
			return true;
		}
	
	return false;
}

void WSAPoll::timeout(uint msecs)
{
	m_timeout = msecs;
}

} // namespace:event

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

#include "select.h"

#include "../../shared/templates.h"

#ifndef WIN32
	#include <cerrno> // errno
#endif
#include <cassert> // assert()

#include "../socket.h"

namespace event {

const bool has_error<Select>::value = true;
const int read<Select>::value = 1;
const int write<Select>::value = 2;
const int error<Select>::value = 4;
const std::string system<Select>::value("select");

Select::Select()
	#ifndef WIN32
	: nfds_r(socket_error::InvalidHandle),
	nfds_w(socket_error::InvalidHandle),
	nfds_e(socket_error::InvalidHandle)
	#endif
{
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
}

Select::~Select()
{
}

void Select::addToSet(fd_set &fdset, fd_t fd
	#ifndef WIN32
	, std::set<fd_t>& l_set, fd_t &largest
	#endif
)
{
	FD_SET(fd, &fdset);
	#ifndef WIN32
	l_set.insert(l_set.end(), fd);
	if (fd > largest) largest = fd;
	#endif
}

void Select::removeFromSet(fd_set &fdset, fd_t fd
	#ifndef WIN32
	, std::set<fd_t>& l_set, fd_t &largest
	#endif
)
{
	FD_CLR(fd, &fdset);
	#ifndef WIN32
	l_set.erase(fd);
	if (fd == largest)
		largest = (l_set.size() > 0 ? *(l_set.rend()) : socket_error::InvalidHandle);
	#endif
}

// Errors: WSAENETDOWN
int Select::wait()
{
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r);
	FD_COPY(&fds_w, &t_fds_w);
	FD_COPY(&fds_e, &t_fds_e);
	#else
	t_fds_r = fds_r;
	t_fds_w = fds_w;
	t_fds_e = fds_e;
	#endif // HAVE_SELECT_COPY
	
	#ifdef WIN32
	static const fd_t ubnfds = 0; // not used
	#else
	using std::max;
	
	fd_t ubnfds = max(max(nfds_w,nfds_r), nfds_e);
	
	if (ubnfds != socket_error::InvalidHandle)
		ubnfds++;
	#endif
	
	nfds = select(ubnfds, &t_fds_r, &t_fds_w, &t_fds_e, &_timeout);
	
	switch (nfds)
	{
		case -1:
			#ifdef WIN32
			error = WSAGetLastError();
			#else
			error = errno;
			#endif
			
			if (error == EINTR)
				return nfds = 0;
			
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
			if (error == WSAENETDOWN)
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
	
	return nfds;
}

int Select::add(fd_t fd, ev_t events)
{
	assert(fd != socket_error::InvalidHandle);
	
	bool rc=false;
	
	if (fIsSet(events, event::read<Select>::value))
	{
		addToSet(fds_r, fd
			#ifndef WIN32
			, read_set, nfds_r
			#endif
		);
		rc = true;
	}
	
	if (fIsSet(events, event::write<Select>::value))
	{
		addToSet(fds_w, fd
			#ifndef WIN32
			, write_set, nfds_w
			#endif
		);
		rc = true;
	}
	
	if (fIsSet(events, event::error<Select>::value))
	{
		addToSet(fds_e, fd
			#ifndef WIN32
			, error_set, nfds_e
			#endif
		);
		rc = true;
	}
	
	assert(rc);
	
	// maintain fd_list
	fd_list.insert(std::pair<fd_t,ev_t>(fd,events));
	
	return rc;
}

int Select::modify(fd_t fd, ev_t events)
{
	assert(fd != socket_error::InvalidHandle);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (!fIsSet(events, event::read<Select>::value))
		removeFromSet(fds_r, fd
			#ifndef WIN32
			, read_set, nfds_r
			#endif
		);
	
	if (!fIsSet(events, event::write<Select>::value))
		removeFromSet(fds_w, fd
			#ifndef WIN32
			, write_set, nfds_w
			#endif
		);
	
	if (!fIsSet(events, event::error<Select>::value))
		removeFromSet(fds_e, fd
			#ifndef WIN32
			, error_set,  nfds_e
			#endif
		);
	
	return 0;
}

int Select::remove(fd_t fd)
{
	assert(fd != socket_error::InvalidHandle);
	
	std::map<fd_t,uint>::iterator iter(fd_list.find(fd));
	assert(iter != fd_list.end());
	
	removeFromSet(fds_r, fd
		#ifndef WIN32
		, read_set, nfds_r
		#endif
	);
	removeFromSet(fds_w, fd
		#ifndef WIN32
		, write_set, nfds_w
		#endif
	);
	removeFromSet(fds_e, fd
		#ifndef WIN32
		, error_set, nfds_e
		#endif
	);
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool Select::getEvent(fd_t &fd, ev_t &events)
{
	assert(fd_list.size() > 0);
	
	while (fd_iter != fd_list.end())
	{
		fd = (fd_iter++)->first;
		
		events = 0;
		
		if (FD_ISSET(fd, &t_fds_r) != 0)
			fSet(events, event::read<Select>::value);
		if (FD_ISSET(fd, &t_fds_w) != 0)
			fSet(events, event::write<Select>::value);
		if (FD_ISSET(fd, &t_fds_e) != 0)
			fSet(events, event::error<Select>::value);
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void Select::timeout(uint msecs)
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

} // namespace::event

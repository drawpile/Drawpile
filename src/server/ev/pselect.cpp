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

#include "pselect.h"

#include "../../shared/templates.h" // fIsSet()

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
	using std::cerr;
#endif
#include <cerrno> // errno
#include <cassert> // assert()

namespace event {

const bool has_error<Pselect>::value = true;
const int read<Pselect>::value = 1;
const int write<Pselect>::value = 2;
const int error<Pselect>::value = 4;
const std::string system<Pselect>::value("pselect");

Pselect::Pselect()
	: nfds_r(-1),
	nfds_w(-1),
	nfds_e(-1)
{
	FD_ZERO(&fds_r);
	FD_ZERO(&fds_w);
	FD_ZERO(&fds_e);
	
	sigemptyset(&sigmask); // prepare sigmask
}

Pselect::~Pselect()
{
}

// Errors: WSAENETDOWN
int Pselect::wait()
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.wait()" << endl;
	#endif
	
	#ifdef EV_SELECT_COPY
	FD_COPY(&fds_r, &t_fds_r);
	FD_COPY(&fds_w, &t_fds_w);
	FD_COPY(&fds_e, &t_fds_e);
	#else
	t_fds_r = fds_r;
	t_fds_w = fds_w;
	t_fds_e = fds_e;
	#endif // HAVE_SELECT_COPY
	
	using std::max;
	const fd_t ubnfds = max(max(nfds_w,nfds_r), nfds_e);
	
	nfds = pselect((ubnfds==-1?-1:ubnfds+1), &t_fds_r, &t_fds_w, &t_fds_e, &_timeout, &sigmask);
	
	switch (nfds)
	{
		case -1:
			error = errno;
			if (error == EINTR)
				return nfds = 0;
			
			assert(error != EBADF);
			assert(error != ENOTSOCK);
			assert(error != EINVAL);
			assert(error != EFAULT);
			
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

int Pselect::add(fd_t fd, int events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.add(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != -1);
	
	bool rc=false;
	
	if (fIsSet(events, event::read<Pselect>::value))
	{
		FD_SET(fd, &fds_r);
		read_set.insert(read_set.end(), fd);
		if (fd > nfds_r) nfds_r = fd;
		rc = true;
	}
	if (fIsSet(events, event::write<Pselect>::value))
	{
		FD_SET(fd, &fds_w);
		write_set.insert(write_set.end(), fd);
		if (fd > nfds_w) nfds_w = fd;
		rc = true;
	}
	if (fIsSet(events, event::error<Pselect>::value))
	{
		FD_SET(fd, &fds_e);
		error_set.insert(error_set.end(), fd);
		if (fd > nfds_e) nfds_e = fd;
		rc = true;
	}
	
	assert(rc);
	
	// maintain fd_list
	fd_list[fd] = events;
	
	return rc;
}

int Pselect::modify(fd_t fd, int events)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.modify(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != -1);
	
	// act like a wrapper.
	if (events != 0)
		add(fd, events);
	
	if (!fIsSet(events, event::read<Pselect>::value))
	{
		FD_CLR(fd, &fds_r);
		read_set.erase(fd);
		if (fd == nfds_r)
			nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : -1);
	}
	
	if (!fIsSet(events, event::write<Pselect>::value))
	{
		FD_CLR(fd, &fds_w);
		write_set.erase(fd);
		if (fd == nfds_w)
			nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : -1);
	}
	
	if (!fIsSet(events, event::error<Pselect>::value))
	{
		FD_CLR(fd, &fds_e);
		error_set.erase(fd);
		if (fd == nfds_e)
			nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : -1);
	}
	
	return 0;
}

int Pselect::remove(fd_t fd)
{
	#if defined(DEBUG_EVENTS) and !defined(NDEBUG)
	cout << "pselect.remove(fd: " << fd << ")" << endl;
	#endif
	
	assert(fd != -1);
	
	std::map<fd_t,uint>::iterator iter(fd_list.find(fd));
	assert(iter != fd_list.end());
	
	FD_CLR(fd, &fds_r);
	read_set.erase(fd);
	if (fd == nfds_r)
		nfds_r = (read_set.size() > 0 ? *(--read_set.end()) : -1);
	
	FD_CLR(fd, &fds_w);
	write_set.erase(fd);
	if (fd == nfds_w)
		nfds_w = (write_set.size() > 0 ? *(--write_set.end()) : -1);
	
	FD_CLR(fd, &fds_e);
	error_set.erase(fd);
	if (fd == nfds_e)
		nfds_e = (error_set.size() > 0 ? *(--error_set.end()) : -1);
	
	fd_list.erase(iter);
	fd_iter = fd_list.begin();
	
	return true;
}

bool Pselect::getEvent(fd_t &fd, int &events)
{
	while (fd_iter != fd_list.end())
	{
		fd = fd_iter->first;
		++fd_iter;
		
		assert(fd != -1);
		events = 0;
		
		if (FD_ISSET(fd, &t_fds_r) != 0)
			fSet(events, event::read<Pselect>::value);
		if (FD_ISSET(fd, &t_fds_w) != 0)
			fSet(events, event::write<Pselect>::value);
		if (FD_ISSET(fd, &t_fds_e) != 0)
			fSet(events, event::error<Pselect>::value);
		
		if (events != 0)
			return true;
	}
	
	return false;
}

void Pselect::timeout(uint msecs)
{
	if (msecs > 1000)
	{
		_timeout.tv_sec = msecs/1000;
		msecs -= _timeout.tv_sec*1000;
	}
	else
		_timeout.tv_sec = 0;
	
	_timeout.tv_nsec = msecs * 1000000;
}

} // namespace:event

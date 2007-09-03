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

#include "kqueue.h"

#include <cerrno> // errno
#include <cassert> // assert()

namespace event {

const int read<Kqueue>::value = EVFILT_READ;
const int write<Kqueue>::value = EVFILT_WRITE;
const std::string system<Kqueue>::value("kqueue");

Kqueue::Kqueue()
	: evfd(-1),
	chlist_size(8),
	chlist_c(0),
	evtrigr_size(8),
	evtrigr_c(0)
{
	if ((evfd = kqueue()) == -1)
		error = errno;
	else
	{
		evtrigr = new kevent[evtrigr_size];
		chlist = new kevent[chlist_size];
	}
}

Kqueue::~Kqueue()
{
	if (evfd != -1)
		close(evfd);
	
	delete [] evtrigr;
	delete [] chlist;
}

void Kqueue::resizeChlist(int new_size)
{
	assert(new_size > chlist_size);
	
	kevent *nchl = new kevent[new_size];
	memcpy(nchl, chlist, sizeof(kevent)*chlist_c);
	delete [] chlist;
	chlist = nchl;
	chlist_size = new_size;
}


// Errors: EACCES, ENOMEM
int Kqueue::wait()
{
	assert(evfd != -1);
	
	nfds = kevent(evfd, chlist, chlist_c, evtrigr, evtrigr_c, &m_timeout);
	chlist_c = 0;
	
	if (nfds == -1)
	{
		error = errno;
		
		if (error == EINTR)
			return 0;
		
		assert(error != EBADF); // FD is invalid
		assert(error != ENOENT); // specified event could not be found
		assert(error != EINVAL); // filter or time limit is invalid
		assert(error != EFAULT); // kevent struct is not writable
		
		return -1;
	}
	
	return nfds;
}

int Kqueue::add(fd_t fd, int events)
{
	assert(fd != -1);
	
	if (chlist_c == chlist_size)
		resizeChlist(chlist_size*2);
	
	EV_SET(&chlist[chlist_c++], fd, events, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int Kqueue::modify(fd_t fd, int events)
{
	assert(fd != -1);
	
	if (chlist_c == chlist_size)
		resizeChlist(chlist_size*2);
	
	EV_SET(&chlist[chlist_c++], fd, events, EV_ADD|EV_ENABLE, 0, 0, 0);
	
	return true;
}

int Kqueue::remove(fd_t fd)
{
	assert(fd != -1);
	
	if (chlist_c == chlist_size)
		resizeChlist(chlist_size*2);
	
	EV_SET(&chlist[chlist_c++], fd, 0, EV_DELETE|EV_DISABLE, 0, 0, 0);
	
	return true;
}

bool Kqueue::getEvent(fd_t &fd, int &r_events)
{
	assert(nfds >= 0);
	
	getevent:
	nfds--;
	
	if (nfds < 0)
		return false;
	
	if (fIsSet(evtrigr[nfds].flags, EV_ERROR))
	{
		// an error occured while processing specific kevent
		const int l_err = evtrigr[nfds].data;
		
		// coding errors
		assert(l_err != ENOENT);
		assert(l_err != EINVAL);
		assert(l_err != EBADF);
		assert(l_err != EFAULT);
		assert(l_err != EACCESS);
		
		// out of memory
		if (l_err == ENOMEM)
			throw std::bad_alloc;
		
		// try next
		goto getevent;
	}
	
	fd = evtrigr[nfds].ident;
	r_events = evtrigr[nfds].filter;
	
	return true;
}

void Kqueue::timeout(uint msecs)
{
	if (msecs > 1000)
	{
		m_timeout.tv_sec = msecs/1000;
		msecs -= m_timeout.tv_sec*1000;
	}
	else
		m_timeout.tv_sec = 0;
	
	m_timeout.tv_nsec = msecs * 1000000; // nanoseconds
}

} // namespace:event

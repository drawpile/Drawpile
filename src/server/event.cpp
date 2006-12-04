/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#include "event.h"

#include <cassert>

const int
	Event::read = 0,
	Event::write = 1,
	Event::except = 2;

Event::Event()
{
}

Event::~Event() throw()
{
}

void Event::init()
{
	FD_ZERO(&fds[read]);
	FD_ZERO(&fds[write]);
	FD_ZERO(&fds[except]);
}

void Event::finish() throw()
{
}

int Event::wait(uint32_t secs, uint32_t nsecs) throw()
{
	memcpy(&t_fds[read], &fds[read], sizeof(fd_set)),
	memcpy(&t_fds[write], &fds[write], sizeof(fd_set)),
	memcpy(&t_fds[except], &fds[except], sizeof(fd_set));
	
	timeval tv;
	tv.tv_sec = secs;
	tv.tv_usec = nsecs / 1000; // timeval uses microseconds?
	
	int nfds=0;
	return select(nfds, &fds[read], &fds[write], &fds[except], &tv);
}

int Event::add(uint32_t fd, int set) throw()
{
	assert(set >= 0 && set <= 2);
	assert(fd != INVALID_SOCKET);
	/*
	if (fdc[set].find(fd) == fdc[set].end())
	{
		fdc[set].insert(fdc[set].end(), fd);
		*/
		FD_SET(fd, &fds[set]);
		return true;
		/*
	}
	else // already in list
		return false;
	*/
}

int Event::remove(uint32_t fd, int set) throw()
{
	assert(set >= 0 && set <= 2);
	assert(fd != INVALID_SOCKET);
	
	/*
	if (fdc[set].find(fd) == fdc[set].end())
		return false;
	else
	{
		*/
		FD_CLR(fd, &fds[set]);
		return true;
		/*
	}
	*/
}

int Event::isset(uint32_t fd, int set) throw()
{
	assert(set >= 0 && set <= 2);
	assert(fd != INVALID_SOCKET);
	
	return (FD_ISSET(fd, &t_fds[set]) != 0);
}

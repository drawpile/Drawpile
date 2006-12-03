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

#ifndef EVENT_H_INCLUDED
#define EVENT_H_INCLUDED

// #include <set>
#include <stdint.h>

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <sys/time.h>
	#include <sys/select.h> // fd_set, FD* macros, etc.
#endif

//! Event I/O
/**
 * Needs work, similar structure is not optimal for event systems like epoll(4).
 */
class Event
{
protected:
	//std::set<uint32_t> fdc[3];
	
	fd_set fds[3];
	fd_set t_fds[3];
	
	/**
	 * Initialize event system.
	 */
	void init();
	
	/**
	 * Finish.
	 */
	void finish() throw();
public:
	
	static const int
		/** identifier for 'read' fd set */
		read,
		/** identifier for 'write' fd set */
		write,
		/** identifier for 'exception' fd set */
		except;
	
	/**
	 * ctor (calls init())
	 */
	Event();
	
	/**
	 * dtor (calls finish())
	 */
	~Event() throw();
	
	/**
	 * Wait for events.
	 *
	 * @param secs seconds to wait.
	 * @param nsecs nanoseconds to wait (may be truncated to milli/microseconds
	 * depending on event system)
	 *
	 * @return number of file descriptors triggered, -1 on error, and 0 otherwise.
	 */
	int wait(uint32_t secs, uint32_t nsecs) throw();
	
	/**
	 * Adds file descriptor to set.
	 *
	 * @param fd is the file descriptor to be added to a set.
	 * @param set is the fd set in which fd is to be found.
	 *
	 * @return 
	 */
	int add(uint32_t fd, int set) throw();
	
	/**
	 * Removes file descriptor from set.
	 *
	 * @param fd is the file descriptor to be removed from a set.
	 * @param set is the fd set in which fd is to be found.
	 *
	 * @return true if the fd was removed, false if not (or was not part of the set)
	 */
	int remove(uint32_t fd, int set) throw();
	
	/**
	 * Tests if the file descriptor was triggered in set.
	 *
	 * @param fd is the file descriptor to be tested in a set.
	 * @param set is the fd set in which fd is to be found.
	 *
	 * @return true if fd was triggered, false if not (or was not part of the set)
	 */
	int isset(uint32_t fd, int set) throw();
};

#endif // EVENT_H_INCLUDED

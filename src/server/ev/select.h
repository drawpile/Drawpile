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

#ifndef EventSelect_H
#define EventSelect_H

#include "config.h"
#include "abstract.h"
#include "../types.h"

#include <ctime> // timeval

#include <map> // std::map
#ifdef WIN32
	#include <winsock2.h> // SOCKET, fd_set, etc.
#else
	#include <set> // std::set
	#include <sys/select.h> // fd_set
#endif

#define EV_NAME L"select"

//! select(2)
/**
 * @see man 2 select
 * @see http://msdn2.microsoft.com/en-us/library/ms740141.aspx
 */
class Event
	: public AbstractEventInterface<uint>
{
public:
	enum {
		Read = 1,
		Write = 2
	};
protected:
	int triggered;
	
	timeval _timeout;
	
	fd_set readSet, writeSet, t_readSet, t_writeSet;
	
	std::map<SOCKET, uint> fd_list; // events set for FD
	typedef std::map<SOCKET, uint>::iterator fd_list_iter;
	fd_list_iter fd_iter;
	
	#ifndef WIN32
	std::set<SOCKET> readList, writeList;
	SOCKET highestReadFD, highestWriteFD;
	#endif // !WIN32
	
	void addToSet(fd_set &fset, SOCKET fd
		#ifndef WIN32
		, std::set<SOCKET>& l_set, SOCKET &largest
		#endif
	);
	void removeFromSet(fd_set &fset, SOCKET fd
		#ifndef WIN32
		, std::set<SOCKET>& l_set, SOCKET &largest
		#endif
	);
public:
	Event() NOTHROW;
	~Event() NOTHROW;
	
	void timeout(uint msecs) NOTHROW;
	int wait() NOTHROW;
	int add(SOCKET fd, event_t events) NOTHROW;
	int remove(SOCKET fd) NOTHROW;
	int modify(SOCKET fd, event_t events) NOTHROW;
	bool getEvent(SOCKET &fd, event_t &events) NOTHROW;
};

#endif // EventSelect_H

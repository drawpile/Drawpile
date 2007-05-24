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

#ifndef EventPselect_INCLUDED
#define EventPselect_INCLUDED

#include "interface.h"
#include "traits.h"

#ifndef NDEBUG
	#include <iostream>
	using std::cout;
	using std::endl;
#endif

#include <sys/select.h>
#include <signal.h>

#include <map>
#include <set>

class EventPselect
	: EventInterface<int>
{
private:
	timespec _timeout;
	int nfds;
	sigset_t sigmask, sigsaved;
	fd_set fds_r, fds_w, fds_e, t_fds_r, t_fds_w, t_fds_e;
	std::map<int, uint> fd_list; // events set for FD
	std::map<int, uint>::iterator fd_iter;
	std::set<int> read_set, write_set, error_set;
	int nfds_r, nfds_w, nfds_e;
public:
	EventPselect() throw();
	~EventPselect() throw();
	
	//! Set signal mask.
	/**
	 * Only used by pselect() so far
	 */
	void setMask(const sigset_t& mask) throw() { sigmask = mask; }
	
	void timeout(uint msecs) throw();
	int wait() throw();
	int add(fd_t fd, int events) throw();
	int remove(fd_t fd) throw();
	int modify(fd_t fd, int events) throw();
	bool getEvent(fd_t &fd, int &events) throw();
};

template <>
struct EventTraits<EventPselect>
{
	typedef int ev_t;
	
	static const bool hasHangup = false;
	static const bool hasError = true;
	static const bool hasAccept = false;
	static const bool hasConnect = false;
	static const bool usesSigmask = true;
	
	static const int
		Read=1,
		Write=2,
		Error=4;
	
	static const int
		Accept,
		Connect,
		Hangup;
};

#endif // EventPselect_INCLUDED

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

#ifndef EventSelect_INCLUDED
#define EventSelect_INCLUDED

#include "interface.h"
#include "traits.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#ifdef WIN32
	#include "../sockets.h"
#else
	#include <sys/select.h>
#endif

#include <map>
#include <set>

namespace event {

#ifdef WIN32
class Select;
template <> struct fd_type<Select> { typedef SOCKET fd_t; };
#endif

//! select(2)
/**
 * 
 */
class Select
	: public Interface<Select>
{
private:
	timeval _timeout;
	int nfds;
	fd_set fds_r, fds_w, fds_e, t_fds_r, t_fds_w, t_fds_e;
	std::map<fd_t, uint> fd_list; // events set for FD
	std::map<fd_t, uint>::iterator fd_iter;
	#ifndef WIN32
	std::set<fd_t> read_set, write_set, error_set;
	fd_t nfds_r, nfds_w, nfds_e;
	#endif // !WIN32
public:
	Select() throw();
	~Select() throw() { }
	
	void timeout(uint msecs) throw();
	int wait() throw();
	int add(fd_t fd, ev_t events) throw();
	int remove(fd_t fd) throw();
	int modify(fd_t fd, ev_t events) throw();
	bool getEvent(fd_t &fd, ev_t &events) throw();
};

/* traits */

template <> struct has_error<Select> { static const bool value; };
template <> struct read<Select> { static const int value; };
template <> struct write<Select> { static const int value; };
template <> struct error<Select> { static const int value; };
template <> struct system<Select> { static const std::string value; };

} // namespace:event

#endif // EventSelect_INCLUDED

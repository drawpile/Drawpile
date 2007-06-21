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

#ifndef EventWSA_INCLUDED
#define EventWSA_INCLUDED

#include "interface.h"
#include "traits.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include <map>
#include <winsock2.h>

namespace event {

class WSA;
template <> struct ev_type<WSA> { typedef long ev_t; };
template <> struct fd_type<WSA> { typedef SOCKET fd_t; };

const uint max_events = 10;

//! WSA
/**
 * 
 */
class WSA
	: Interface<WSA>
{
private:
	uint _timeout;
	uint nfds;
	uint last_event;
	std::map<fd_t, uint> fd_to_ev;
	typedef std::map<fd_t, uint>::iterator ev_iter;
	WSAEVENT w_ev[max_events];
	SOCKET fdl[max_events];
public:
	WSA() throw();
	~WSA() throw() { }
	
	void timeout(uint msecs) throw();
	int wait() throw();
	int add(fd_t fd, long events) throw();
	int remove(fd_t fd) throw();
	int modify(fd_t fd, long events) throw();
	bool getEvent(fd_t &fd, long &events) throw();
};

/* traits */

template <> struct has_hangup<WSA> { static const bool value; };
template <> struct has_connect<WSA> { static const bool value; };
template <> struct has_accept<WSA> { static const bool value; };
template <> struct read<WSA> { static const long value; };
template <> struct write<WSA> { static const long value; };
template <> struct hangup<WSA> { static const long value; };
template <> struct accept<WSA> { static const long value; };
template <> struct connect<WSA> { static const long value; };
template <> struct system<WSA> { static const std::string value; };

template <> struct invalid_fd<WSA> { static const fd_type<WSA>::fd_t value; };

// unused, but required because GCC is less than bright
template <> struct error<WSA> { static const long value; };

} // namespace:event

#endif // EventWSA_INCLUDED

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
#include "../socket.types.h" // fd_t, WSAEVENT
#include <winsock2.h>

#include <map>

namespace event {

class WSAPoll;
template <> struct ev_type<WSAPoll> { typedef short ev_t; };
template <> struct fd_type<WSAPoll> { typedef SOCKET fd_t; };

const uint max_events = std::numeric_limits<ulong>::max();

//! WSA Poll
/**
 * @note Works only on Vista and Windows Server 2003 and later.
 * @bug The used function is also called WSAPoll, so saying using namespace event will break things.
 */
class WSAPoll
	: public Interface<SOCKET, short>
{
private:
	int m_timeout;
	int t_event;
	int limit;
	
	WSAPOLLFD *descriptors;
	
	std::map<fd_t,int> fd_to_index;
	typedef std::map<fd_t,int>::iterator index_i;
public:
	WSAPoll() NOTHROW;
	~WSAPoll() NOTHROW;
	
	void timeout(uint msecs) NOTHROW;
	int wait() NOTHROW;
	int add(fd_t fd, ev_t events) NOTHROW;
	int remove(fd_t fd) NOTHROW;
	int modify(fd_t fd, ev_t events) NOTHROW;
	bool getEvent(fd_t &fd, ev_t &events) NOTHROW;
};

/* traits */

template <> struct read<WSAPoll> { static const ev_type<WSAPoll>::ev_t value; };
template <> struct write<WSAPoll> { static const ev_type<WSAPoll>::ev_t value; };
template <> struct system<WSAPoll> { static const std::string value; };
template <> struct invalid_fd<WSAPoll> { static const fd_type<WSAPoll>::fd_t value; };

// unused, but required because GCC is less than bright
template <> struct error<WSAPoll> { static const long value; };
/*
template <> struct hangup<WSAPoll> { static const long value; };
template <> struct accept<WSAPoll> { static const long value; };
template <> struct connect<WSAPoll> { static const long value; };
*/

} // namespace:event

#endif // EventWSA_INCLUDED

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

#ifndef EventEpoll_INCLUDED
#define EventEpoll_INCLUDED

#include "interface.h"
#include "traits.h"

#include <stdexcept>

#include <sys/epoll.h> // epoll_event

namespace event {

//! epoll(4)
/**
 * 
 */
class Epoll
	: public Interface
{
private:
	uint m_timeout;
	int evfd;
	epoll_event events[10];
public:
	/**
	 * @throw std::exception
	 */
	Epoll() NOTHROW;
	~Epoll() NOTHROW;
	
	void timeout(uint msecs) NOTHROW;
	int wait() NOTHROW;
	int add(fd_t fd, ev_t events) NOTHROW;
	int remove(fd_t fd) NOTHROW;
	int modify(fd_t fd, ev_t events) NOTHROW;
	bool getEvent(fd_t &fd, ev_t &events) NOTHROW;
};

/* traits */

template <> struct has_hangup<Epoll> { static const bool value; };
template <> struct has_error<Epoll> { static const bool value; };
template <> struct read<Epoll> { static const int value; };
template <> struct write<Epoll> { static const int value; };
template <> struct error<Epoll> { static const int value; };
template <> struct hangup<Epoll> { static const int value; };
template <> struct system<Epoll> { static const std::string value; };

} // namespace:event

#endif // EventEpoll_INCLUDED

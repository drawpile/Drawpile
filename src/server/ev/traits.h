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

#ifndef EventTraits_INCLUDED
#define EventTraits_INCLUDED

#include <string>

/* traits */

template <typename Evs>
struct event_type
{
	typedef int ev_t;
};

template <typename Evs>
struct event_fd_type
{
	typedef int fd_t;
};

template <typename Evs>
struct event_has_hangup
{
	static const bool value = false;
};

template <typename Evs>
struct event_has_error
{
	static const bool value = false;
};

template <typename Evs>
struct event_has_connect
{
	static const bool value = false;
};

template <typename Evs>
struct event_has_accept
{
	static const bool value = false;
};

/* events */

template <typename Evs>
struct event_read
{
	static const int value = 0;
};

template <typename Evs>
struct event_write
{
	static const int value = 0;
};

template <typename Evs>
struct event_error
{
	static const int value = 0;
};

template <typename Evs>
struct event_hangup
{
	static const int value = 0;
};

template <typename Evs>
struct event_accept
{
	static const int value = 0;
};

template <typename Evs>
struct event_connect
{
	static const int value = 0;
};

template <typename Evs>
struct event_system
{
	static const std::string value;
};

template <typename Evs>
struct event_invalid_fd
{
	static const typename event_fd_type<Evs>::fd_t value = -1;
};

#endif // EventTraits_INCLUDED

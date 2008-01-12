/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#pragma once

#ifndef EventTraits_INCLUDED
#define EventTraits_INCLUDED

#include <string>

/* traits */

namespace event {

//! Event type
template <typename Evs>
struct ev_type
{
	typedef int ev_t;
};

//! Handle type
template <typename Evs>
struct fd_type
{
	typedef int fd_t;
};

//! Event handler has \b event::hangup event type
template <typename Evs>
struct has_hangup
{
	static const bool value = false;
};

//! Event handler has \b event::error event type
template <typename Evs>
struct has_error
{
	static const bool value = false;
};

//! Event handler has \b event::connect event type
template <typename Evs>
struct has_connect
{
	static const bool value = false;
};

//! Event handler has \b event::accept event type
template <typename Evs>
struct has_accept
{
	static const bool value = false;
};

/* events */

//! Value of Read event
template <typename Evs>
struct read
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Value of Write event
template <typename Evs>
struct write
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Value of Error event
template <typename Evs>
struct error
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Value of Hangup event
template <typename Evs>
struct hangup
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Value of Accept event
template <typename Evs>
struct accept
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Value of Connect event
template <typename Evs>
struct connect
{
	static const typename ev_type<Evs>::ev_t value;
};

//! Event handler name
template <typename Evs>
struct system
{
	static const std::string value;
};

template <typename Evs>
struct invalid_fd
{
	static const typename fd_type<Evs>::fd_t value = -1;
};

} // namespace:event

#endif // EventTraits_INCLUDED

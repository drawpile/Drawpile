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

#ifndef NetworkAddress_INCLUDED
#define NetworkAddress_INCLUDED

#include "config.h"

#include <string> // std::string

#include "network.h"
#include "socket.types.h"
#include "types.h"

//! Address
struct Address
{
	Address();
	
	Network::Family::type family;
	
	union {
		//! base address
		sockaddr addr;
		//! IPv4 address
		sockaddr_in IPv4;
		#ifdef IPV6_SUPPORT
		//! IPv6 address
		sockaddr_in6 IPv6;
		#endif
	};
	
	socklen_t size() const __attribute__ ((nothrow));
	
	ushort port() const __attribute__ ((nothrow));
	
	void port(ushort _port) __attribute__ ((nothrow));
	
	void setFamily(Network::Family::type _family) __attribute__ ((nothrow));
	
	//! Assign operator
	Address& operator= (const Address& naddr) __attribute__ ((nothrow));
	
	//! Is-equal operator
	bool operator== (const Address& naddr) const __attribute__ ((nothrow,warn_unused_result));
	
	//! Convert address to string representation of it
	std::string toString() const /*__attribute__ ((warn_unused_result))*/;
	
	//! Convert string to address
	/**
	 * @param[in] address string to convert
	 */
	static Address fromString(std::string const& address) __attribute__ ((nothrow,warn_unused_result));
};

#endif // NetworkAddress_INCLUDED

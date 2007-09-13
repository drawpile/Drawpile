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
#include "socket.porting.h" // sockaddr_in
#include "socket.types.h"
#include "types.h"

//! Address
class Address
{
public:
	Address(const std::string& address=std::string(), ushort port=0);
	
	union {
		//! base address
		sockaddr raw_addr;
		//! Either IPv4 or IPv6 address
		Network::sockaddr_in_t ipv_addr;
	};
	
	socklen_t size() const NOTHROW;
	
	ushort port() const NOTHROW;
	
	void port(ushort _port) NOTHROW;
	
	int family() const NOTHROW;
	void family(int _family) NOTHROW;
	
	//! Assign operator
	Address& operator= (const Address& naddr) NOTHROW;
	
	//! Is-equal operator
	bool operator== (const Address& naddr) const NOTHROW;
	
	//! Convert address to string representation of it
	std::string toString() const;
	
private:
	//! Return pointer to sin_addr or sin6_addr
	Network::in_addr_t& in_addr() NOTHROW;
	
	//! Return const pointer to sin_addr or sin6_addr
	const Network::in_addr_t& in_addr() const NOTHROW;
	
	//! Convert string to address
	/**
	 * @param[in] address string to convert
	 */
	void fromString(std::string const& address) NOTHROW;
};

#endif // NetworkAddress_INCLUDED

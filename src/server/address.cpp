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

#include "address.h"

#include "../shared/templates.h"
#include "network.h"

#ifndef WIN32
	#include <cstdio> // snprintf/sprintf
#endif

Address::Address(const std::string& address, ushort _port)
{
	family(Network::family);
	
	if (address.empty())
	{
		#ifdef IPV6_SUPPORT
		memcpy(in_addr().s6_addr, Network::UnspecifiedAddress, sizeof(Network::UnspecifiedAddress));
		#else
		memcpy(&in_addr().s_addr, &Network::UnspecifiedAddress, sizeof(Network::UnspecifiedAddress));
		#endif
	}
	else
		fromString(address);
	
	port(_port);
}

socklen_t Address::size() const
{
	return sizeof(ipv_addr);
}

ushort Address::port() const
{
	#ifdef IPV6_SUPPORT
	return bswap_const(ipv_addr.sin6_port);
	#else
	return bswap_const(ipv_addr.sin_port);
	#endif
}

void Address::port(ushort _port)
{
	#ifdef IPV6_SUPPORT
	ipv_addr.sin6_port = bswap(_port);
	#else
	ipv_addr.sin_port = bswap(_port);
	#endif
}

int Address::family() const
{
	#ifdef IPV6_SUPPORT
	return ipv_addr.sin6_family;
	#else
	return ipv_addr.sin_family;
	#endif
}

void Address::family(int _family)
{
	#ifdef IPV6_SUPPORT
	ipv_addr.sin6_family = _family;
	#else
	ipv_addr.sin_family = _family;
	#endif
}

Address& Address::operator= (const Address& naddr)
{
	family(naddr.family());
	
	in_addr() = naddr.in_addr();
	
	port(naddr.port());
	
	return *this;
}

bool Address::operator== (const Address& naddr) const
{
	if (naddr.family() != family())
		return false;
	
	return (memcmp(&in_addr(), &naddr.in_addr(), size()) == 0);
}

/* string functions */

std::string Address::toString() const
{
	const uint length = Network::AddrLength + Network::PortLength + Network::AddrPadding;
	#ifndef WIN32
	#ifdef IPV6_SUPPORT
	const char format_string[] = "[%s]:%d";
	#else // IPv4
	const char format_string[] = "%s:%d";
	#endif
	#endif
	
	char buf[length];
	
	#ifdef WIN32
	u_long len = length;
	Address sa = *this;
	WSAAddressToString(&sa.raw_addr, sa.size(), 0, buf, &len);
	#else // POSIX
	char straddr[length];
	inet_ntop(family(), &in_addr(), straddr, length);
	
	#ifdef HAVE_SNPRINTF
	snprintf(buf, length, format_string, straddr, port());
	#else
	sprintf(buf, format_string, straddr, port());
	#endif // HAVE_SNPRINTF
	#endif
	return std::string(buf);
}

Network::in_addr_t& Address::in_addr()
{
	#ifdef IPV6_SUPPORT
	return ipv_addr.sin6_addr;
	#else
	return ipv_addr.sin_addr;
	#endif
}

const Network::in_addr_t& Address::in_addr() const
{
	#ifdef IPV6_SUPPORT
	return ipv_addr.sin6_addr;
	#else
	return ipv_addr.sin_addr;
	#endif
}

void Address::fromString(const std::string& address)
{
	#ifdef WIN32
	// Win32 doesn't have inet_pton
	char buf[Network::AddrLength + Network::PortLength + 4];
	memcpy(buf, address.c_str(), address.length());
	int _size = size();
	WSAStringToAddress(buf, family(), 0, &raw_addr, &_size);
	#else // POSIX
	inet_pton(family(), address.c_str(), in_addr());
	#endif
}

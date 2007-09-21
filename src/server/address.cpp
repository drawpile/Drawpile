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
	if (address.empty())
	{
		family(Network::family);
		#ifdef USE_IPV6
		memcpy(in_addr().s6_addr, Network::UnspecifiedAddress, sizeof(Network::UnspecifiedAddress));
		#else
		memcpy(&in_addr().s_addr, &Network::UnspecifiedAddress, sizeof(Network::UnspecifiedAddress));
		#endif
	}
	else
		fromString(address);
	
	port(_port);
	
	scope(Network::Scope::Default);
}

socklen_t Address::size() const
{
	return sizeof(addr.ipv);
}

ushort Address::port() const
{
	return bswap_const(addr.ipv.SIN_PORT);
}

void Address::port(ushort _port)
{
	addr.ipv.SIN_PORT = bswap(_port);
}

int Address::family() const
{
	return addr.ipv.SIN_FAMILY;
}

void Address::family(int _family)
{
	addr.ipv.SIN_FAMILY = _family;
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

std::string Address::toString() const
{
	const uint length = Network::AddrLength + Network::PortLength + 2;
	char buf[length];
	
	#ifdef WIN32
	u_long len = length;
	Address sa = *this;
	WSAAddressToString(&sa.addr.raw, sa.size(), 0, buf, &len);
	#else // POSIX
	char straddr[length];
	inet_ntop(family(), &in_addr(), straddr, length);
	
	#ifdef USE_IPV6
	const char format_string[] = "[%s]:%d";
	#else // IPv4
	const char format_string[] = "%s:%d";
	#endif
	
	#ifdef HAVE_SNPRINTF
	snprintf(buf, length, format_string, straddr, port());
	#else
	sprintf(buf, format_string, straddr, port());
	#endif
	#endif
	return std::string(buf);
}

Network::in_addr_t& Address::in_addr()
{
	return addr.ipv.SIN_ADDR;
}

const Network::in_addr_t& Address::in_addr() const
{
	return addr.ipv.SIN_ADDR;
}

sockaddr& Address::raw()
{
	return addr.raw;
}

void Address::fromString(const std::string& address)
{
	#ifdef WIN32
	// Win32 doesn't have inet_pton
	char buf[Network::AddrLength + Network::PortLength + 4];
	memcpy(buf, address.c_str(), address.length());
	int _size = size();
	int fam =family();
	WSAStringToAddress(buf, fam, 0, &addr.raw, &_size);
	// clears the family part for some reason
	family(fam);
	#else // POSIX
	inet_pton(family(), address.c_str(), in_addr());
	#endif
}

ulong Address::scope() const
{
	#ifdef USE_IPV6
	return addr.ipv.sin6_scope_id;
	#else
	return 0;
	#endif
}

void Address::scope(ulong _scope)
{
	#ifdef USE_IPV6
	addr.ipv.sin6_scope_id = _scope;
	#else
	// no scopes for IPv4
	#endif
}

#ifndef NEBUG
std::ostream& operator<< (std::ostream& os, const Address& addr)
{
	return os << addr.toString();
}
#endif

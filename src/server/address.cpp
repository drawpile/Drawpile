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

Address::Address()
	#ifdef IPV6_SUPPORT
	: family(Network::Family::IPv6)
	#else
	: family(Network::Family::IPv4)
	#endif
{
	#ifdef IPV6_SUPPORT
	IPv6.sin6_family = family;
	#else
	IPv4.sin_family = family;
	#endif
}

socklen_t Address::size() const throw()
{
	#ifdef IPV6_SUPPORT
	if (family == Network::Family::IPv6)
		return sizeof(IPv6);
	else
	#endif
		return sizeof(IPv4);
}

ushort Address::port() const throw()
{
	#ifdef IPV6_SUPPORT
	if (family == Network::Family::IPv6)
		return bswap_const(IPv6.sin6_port);
	else
	#endif
		return bswap_const(IPv4.sin_port);
}

void Address::port(ushort _port) throw()
{
	#ifdef IPV6_SUPPORT
	if (family == Network::Family::IPv6)
		IPv6.sin6_port = bswap(_port);
	else
	#endif
		IPv4.sin_port = bswap(_port);
}

void Address::setFamily(Network::Family::type _family) throw()
{
	family = _family;
	
	#ifdef IPV6_SUPPORT
	if (family == Network::Family::IPv6)
		IPv6.sin6_family = family;
	else
	#endif
		IPv4.sin_family = family;
}

Address& Address::operator= (const Address& naddr) throw()
{
	#ifdef IPV6_SUPPORT
	if (naddr.family == Network::Family::IPv6)
		memcpy(IPv6.sin6_addr.s6_addr, naddr.IPv6.sin6_addr.s6_addr, sizeof(IPv6.sin6_addr.s6_addr));
	else
	#endif
		IPv4.sin_addr.s_addr = naddr.IPv4.sin_addr.s_addr;
	
	family = naddr.family;
	port(naddr.port());
	
	return *this;
}

bool Address::operator== (const Address& naddr) const throw()
{
	if (naddr.family != family)
		return false;
	
	#ifdef IPV6_SUPPORT
	if (naddr.family == Network::Family::IPv6)
		return (memcmp(IPv6.sin6_addr.s6_addr, naddr.IPv6.sin6_addr.s6_addr, sizeof(IPv6.sin6_addr.s6_addr)) == 0);
	else
	#endif
		return (IPv4.sin_addr.s_addr == naddr.IPv4.sin_addr.s_addr);
}

/* string functions */

std::string Address::toString() const throw()
{
	#ifdef IPV6_SUPPORT
	const uint length = Network::IPv6::AddrLength + Network::PortLength + 4;
	const char format_string[] = "[%s]:%d";
	#else
	const uint length = Network::IPv4::AddrLength + Network::PortLength + 2;
	const char format_string[] = "%s:%d";
	#endif
	
	char buf[length];
	
	#ifdef WIN32
	DWORD len = length;
	Address sa = *this;
	WSAAddressToString(&sa.addr, sa.size(), 0, buf, &len);
	#else // POSIX
	char straddr[length];
	//inet_ntop(raddr.sin_family, getAddress(addr), straddr, length);
	#ifdef IPV6_SUPPRT
	inet_ntop(family, &IPv6.sin6_addr, straddr, length);
	#else
	inet_ntop(family, &IPv4.sin_addr, straddr, length);
	#endif
	
	#ifdef HAVE_SNPRINTF
	snprintf(buf, length, format_string, straddr, port());
	#else
	sprintf(buf, format_string, straddr, port());
	#endif // HAVE_SNPRINTF
	#endif
	return std::string(buf);
}

Address Address::fromString(const std::string& address) throw()
{
	Address addr;
	
	#ifdef WIN32
	// Win32 doesn't have inet_pton
	char buf[Network::IPv6::AddrLength + Network::PortLength + 4];
	memcpy(buf, address.c_str(), address.length());
	int size = addr.size();
	WSAStringToAddress(buf, addr.family, 0, &addr.addr, &size);
	#else // POSIX
	#ifdef IPV6_SUPPORT
	inet_pton(addr.family, address.c_str(), &addr.IPv6.sin6_addr);
	#else
	inet_pton(addr.family, address.c_str(), &addr.IPv4.sin_addr);
	#endif
	#endif
	
	return addr;
}

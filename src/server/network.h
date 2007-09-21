/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

#ifndef Network_INCLUDED
#define Network_INCLUDED

#include "config.h"
#include "types.h"

#include "socket.porting.h" // sockaddr_in/sockaddr_in6
#include "socket.types.h" // AF_INET & AF_INET6

#ifdef USE_IPV6
#define IPVNAMESPACE Network::IPv6
#define SIN_ADDR sin6_addr
#define SIN_PORT sin6_port
#define S_ADDR s6_addr
#define SIN_FAMILY sin6_family
#else
#define IPVNAMESPACE Network::IPv4
#define SIN_ADDR sin_addr
#define SIN_PORT sin_port
#define S_ADDR s_addr
#define SIN_FAMILY sin_family
#endif

//! Network constants
namespace Network {

//! Starts network sub-system on systems where such is necessary
/**
 * Does nothing on most systems.
 *
 * @note On Windows O/S, this is thread-local.
 */
bool start() NOTHROW;

//! Stops any started network sub-system
/**
 * Does nothing on most systems.
 */
void stop() NOTHROW;

//! Address families
namespace Family {

//! Family type
enum family_t {
	None,
	//! version 4
	IPv4 = AF_INET,
	//! version 6
	IPv6 = AF_INET6
};

} // namespace:Family

//! Network scopes suitable for passing to Address.scope() function
namespace Scope {

//! Default scope
static const ulong Default = 0;

} // namespace:Scope

#ifdef USE_IPV6
//! IPv6 related constants
namespace IPv6 {

//! Address type
typedef sockaddr_in6 sockaddr_in_t;
//! IP address
typedef in6_addr in_addr_t;
//! Associated family
const Network::Family::family_t family = Network::Family::IPv6;

//! Localhost address
/**
 * Equivalent of IPv4 \b 127.0.0.1
 */
const char Localhost[] = "[::1]";

//! Localhost address as uint[4]
/**
 * Uses 128 bit host mask
 */
const uint LocalhostAddress[4] = {0,0,0,1};

//! Unspecified address
/**
 * Equivalent of IPv4 \b 0.0.0.0
 *
 * ::/128
 */
const char Unspecified[] = "[::]";

//! Unspecified address as uint[4]
const uint UnspecifiedAddress[4] = {0};

//const char Broadcast[] = "FFFF:FFFF:FFFF:FFFF::"; // ?

//! Maximum length of IPv6 address
/**
 * e.g. [ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]
 * = 8*4 (actual address) + 7 (for delimiters) + 2 (for square brackets)
 */
const uint AddrLength = 8*4 + 7 + 2;

//! Raw address length in octets
const uint RawLength = 16;

//! IPv4 compatibility address
/**
 * e.g. IPv6 ::ffff:1.2.3.4 = IPv4 1.2.3.4
 * more precisely, IPv6 ::ffff:0102:0304
 */
const char IPv4Compat[] = "::ffff:";

//! IPv4 compatibility address as uint[4]
/**
 * @note Copy the IPv4 address to the last uint
 */
const uint IPv4CompatAddress[4] = {0,0,0xffffffff,0};

//! 7 bit host mask
/**
 * Used for ULA addresses
 */

//! 8 bit host mask
/**
 * Used for multicast address
 */
const uint HostMask8[4] = {0xff000000, 0};

//! 10 bit host mask
/**
 * Used for site-local addresses
 */
const uint HostMask10[4] = {0xffc00000, 0};

//! 16 bit host mask
/**
 * Used for 6to4 addressing
 */
const uint HostMask16[4] = {0xffff0000, 0};

//! 64 bit host mask
/**
 * Used for link-local addresses
 */
const uint HostMask64[4] = {0xffffffff,0xffffffff,0};

//! 96 bit host mask
/**
 * Used for IPv4 compatibility addresses
 */
const uint HostMask96[4] = {0xffffffff,0xffffffff,0xffffffff,0};

//! 128 bit host mask
/**
 * Used for loopback address and the unspecified address
 */
const uint HostMask128[4] = {0xffffffff};

//! Multicast address prefix
const uint MulticastAddress[4] = {0xffff0000, 0};

//! ULA address (Unique Local Address)
const uint ULAAddress[4] = {0xfc00000,0};

//! Link local address prefix
/**
 * Uses 64 bit host mask
 * Same as IPv4::AutoconfigAddress
 */
const uint LinkLocalAddress[4] = {0xfe800000,0};

//! Site local address prefix
/**
 * Uses 10 bit host mask
 */
const uint SiteLocalAddress[4] = {0xfec00000,0};

} // namespace:IPv6
#endif

//! IPv4 related constants
namespace IPv4 {

//! Address type
typedef sockaddr_in sockaddr_in_t;
//! IP address
typedef in_addr in_addr_t;
//! Associated family type
const Network::Family::family_t family = Network::Family::IPv4;

//! Localhost address
const char Localhost[] = "127.0.0.1";

//! Unspecified address as uint
const uint LocalhostAddress = 0x7f000001;

//! Unspecified address
const char Unspecified[] = "0.0.0.0";

//! Unspecified address as uint
const uint UnspecifiedAddress = 0;

//! Maximum length of IPv4 address
/**
 * e.g. 123.231.213.123
 */
const uint AddrLength = 16;

//! Raw address length in octets
const uint RawLength = 4;

//! Broadcast address
const char Broadcast[] = "255.255.255.255";

//! Broadcast address as uint
const uint BroadcastAddress = 0xffffffff;

//! Autoconfiguration address (169.254.0.0/16)
/**
 * Uses 16 bit host mask
 */
const uint AutoconfigAddress = 0xa9fe0000;

//! 8 bit host mask (255.0.0.0 = /8)
const uint HostMask8 = 0xff000000;

//! 16 bit host mask (255.255.0.0 = /16)
const uint HostMask16 = 0xffff0000;

//! 24 bit host mask (255.255.255.0 = /24)
const uint HostMask24 = 0xffffff00;

//! 32 bit host mask (255.255.255.255 = /32)
const uint HostMask32 = 0xffffffff;

} // namespace:IPv4

using IPVNAMESPACE::Unspecified;
using IPVNAMESPACE::UnspecifiedAddress;
using IPVNAMESPACE::Localhost;
using IPVNAMESPACE::LocalhostAddress;
using IPVNAMESPACE::AddrLength;
using IPVNAMESPACE::RawLength;
using IPVNAMESPACE::sockaddr_in_t;
using IPVNAMESPACE::family;
using IPVNAMESPACE::in_addr_t;

//! Super user port upper bound
/**
 * port <= SuperUser_Port == SU only
 */
const uint SuperUser_Port = 1023;

//! Maximum length of port number as string
const uint PortLength = 5;

//! Highest port number
const uint PortUpperBound = 65535;

//! Lowest port number
const uint PortLowerBound = 0;

} // namespace:Network

#endif // Network_INCLUDED

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

#include "types.h"

//! Network constants
namespace Network {

//! IPv6 related constants
namespace IPv6 {

//! Localhost address
/**
 * Equivalent of IPv4 \b 127.0.0.1
 */
const char Localhost[] = "::1";

//! Unspecified address
/**
 * Equivalent of IPv4 \b 0.0.0.0
 */
const char Unspecified[] = "::";

//! Maximum length of IPv6 address
/**
 * ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
 */
const uint AddrLength = 39;

} // namespace:IPv6

//! IPv4 related constants
namespace IPv4 {

//! Localhost address
const char Localhost[] = "127.0.0.1";

//! Unspecified address
const char Unspecified[] = "0.0.0.0";

//! Maximum length of IPv4 address
/**
 * 123.456.789.012
 */
const uint AddrLength = 16;

} // namespace:IPv4

//! Super user port upper bound
const uint SuperUser_Port = 1023;

//! Maximum length of either IPv4 or IPv6 address
const uint AddrLength = IPv6::AddrLength;

//! Maximum length of port number as string
const uint PortLength = 5;

//! Highest port number
const uint PortUpperBound = 65535;

//! Lowest port number
const uint PortLowerBound = 0;

namespace error {

//const int WouldBlock = EWOULDBLOCK;

} // namespace:error

} // namespace:Network

#endif // Network_INCLUDED

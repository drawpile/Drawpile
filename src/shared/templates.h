/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

#ifndef Shared_Templates_INCLUDED
#define Shared_Templates_INCLUDED

#include "../config.h"

#include <cmath>
#include <memory> // memcpy()
#include <boost/cstdint.hpp>
#include <cassert>

/* swapping endianess */

//! Base template for byte swapping
template <typename T>
T& bswap(T& x) throw()
{
	#if !defined(IS_BIG_ENDIAN) and !defined(NDEBUG)
	/* // boost uses deprecated stuff (undeclared name)
	BOOST_STATIC_ASSERT((sizeof(T) == 1));
	*/
	#endif
	
	return x;
}

// no specializations for Big Endian systems
#if !defined(IS_BIG_ENDIAN)

//! uint32_t specialization of bswap()
template <> inline
uint32_t& bswap<uint32_t>(uint32_t& x) throw()
{
	// Code snippet (c) 2003 Juan Carlos Cobas
	return x = ((x&0x000000FF)<<24) | ((x&0x0000FF00)<<8) |
		((x&0x00FF0000)>>8) | ((x&0xFF000000)>>24);
}

//! uint16_t specialization of bswap()
template <> inline
uint16_t& bswap<uint16_t>(uint16_t& x) throw()
{
	// Code snippet (c) 2003 Juan Carlos Cobas
	return x = ((x >> 8)) | (x << 8);
}

//! uint8_t specialization of bswap()
template <> inline
uint8_t& bswap<uint8_t>(uint8_t& x) throw()
{
	return x;
}

#endif // IS_BIG_ENDIAN

/* memmory */

// copies src& bytes to dst*
// automation of memcpy(dst*, &src, sizeof(src));
template <typename T>
char* memcpy_t(char* dst, const T& src) throw()
{
	assert(dst != 0);
	memcpy(dst, &src, sizeof(T));
	return dst;
}

// copies sizeof(dst) bytes from src* to dst
// automation of memcpy(&dst, src*, sizeof(dst));
template <typename T>
T& memcpy_t(T& dst, const char* src) throw()
{
	assert(src != 0);
	
	memcpy(&dst, src, sizeof(T));
	return dst;
}

/* Bit operations */

//! Set bit flag
/**
 * @param u flag container.
 * @param x flag to be set.
 *
 * @return modified flag container.
 */
template <typename T>
T& fSet(T& u, const T& x) throw()
{
	return u |= x;
}

//! Clear bit flag
/**
 * @param u flag container.
 * @param x flag to be cleared.
 *
 * @return modified flag container.
 */
template <typename T>
T& fClr(T& u, const T& x) throw()
{
	return u &= ~x;
}

//! Test bit flag
/**
 * @param u flag container.
 * @param x flag to be tested.
 *
 * @return test result
 */
template <typename T>
bool fIsSet(const T& u, const T& x) throw()
{
	return (u & x) == x;
}

/* type ops */

template <typename T, typename U>
bool inBoundsOf(const U& u) throw()
{
	return (static_cast<T>(u) == u);
}

/* integer ops */

//! Round \b number to next \b boundary
/**
 * e.g.
 * roundToNext(1200, 1000) = 2000
 * roundToNext(28175, 300) = 27200
 */
template <typename T>
T roundToNext(const T& number, const T& boundary) throw()
{
	return (number / boundary + 1) * boundary;
}

template <> inline
double roundToNext<double>(const double& number, const double& boundary) throw()
{
	return (::floor(number) / boundary + 1) * boundary;
}

//! Round \b number to previous \b boundary
/**
 * e.g.
 * roundToPrev(1200, 1000) = 1000
 * roundToPrev(28175, 300) = 27900
 */
template <typename T>
T roundToPrev(const T& number, const T& boundary) throw()
{
	return (number / boundary) * boundary;
}

template <> inline
double roundToPrev<double>(const double& number, const double& boundary) throw()
{
	return (::floor(number) / boundary) * boundary;
}

//! Round to closest non-decimal number
/**
 * \note does this work with negative numbers?
 */
template <typename T>
T round(const T& num) throw()
{
	const T fl = ::floor(num);
	return ((num - fl) < .5 ? fl : fl + 1);
}

#endif

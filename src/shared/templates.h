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

//#include "sockets.h"
#include <memory> // memcpy()
#include <stdint.h> // [u]int#_t
#include <cassert>

/* simple template to return copy of the parameter */
template <class T>
T copy(T& x) { return x; }

/* swapping endianess */

template <class T>
T& bswap(T& x)
{
	assert("Default template should never be used!");
	return x;
}

// unsigned

template <> inline
uint32_t& bswap<uint32_t>(uint32_t& x)
{
	// (c) 2003 Juan Carlos Cobas
	return x = (((x&0x000000FF)<<24) + ((x&0x0000FF00)<<8) +
		((x&0x00FF0000)>>8) + ((x&0xFF000000)>>24));
}

template <> inline
uint16_t& bswap<uint16_t>(uint16_t& x)
{
	// (c) 2003 Juan Carlos Cobas
	return x = (((x >> 8)) | (x << 8));
}

template <> inline
uint8_t& bswap<uint8_t>(uint8_t& x)
{
	// (c) 2003 Juan Carlos Cobas
	return x;
}

/* memmory */

template <class X>
char* memcpy_t(char* dst, const X& src)
{
	assert(dst != 0);
	memcpy(dst, &src, sizeof(X));
	return dst;
}

template <class T>
T& memcpy_t(T& dst, const char* src)
{
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
template <class T, class X>
T& fSet(T& u, X& x)
{
	assert( sizeof(T) == sizeof(X) );
	return u |= x;
}

//! Clear bit flag
/**
 * @param u flag container.
 * @param x flag to be cleared.
 *
 * @return modified flag container.
 */
template <class T, class X>
T& fClr(T& u, X& x)
{
	assert( sizeof(T) == sizeof(X) );
	return u ^= x;
}

//! Test bit flag
/**
 * @param u flag container.
 * @param x flag to be tested.
 *
 * @return test result
 */
template <class T>
bool fIsSet(const T& u, const T& x)
{
	return (u & x) == x;
}

#endif

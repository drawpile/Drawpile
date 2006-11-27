/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*******************************************************************************/

#ifndef Shared_Templates_INCLUDED
#define Shared_Templates_INCLUDED

//#include "sockets.h"
#include <memory> // memcpy()
#include <stdint.h> // [u]int#_t

/* swapping endianess */

template <class T>
struct bswap_t
{
	static
	T& swap(T&);
};

/* wrapper for bswap_t */

template <class T>
T& bswap(T& x)
{
	return bswap_t<T>::swap(x);
}

//! Wrapper for memcpy() and bswap().
/**
 * @param x target variable.
 * @param buf source char* buffer.
 *
 * @return reference to x after writing sizeof(x) bytes from buffer to it, and swapping
 * the endianess.
 */
template <class T>
T& bswap_mem(T& x, const char* buf)
{
	assert(buf != 0);
	memcpy(&x, buf, sizeof(T));
	return bswap(x);
}

//! Wrapper for memcpy() and bswap()
/**
 * @param buf target char* buffer.
 * @param u source integer.
 *
 * @return modified buf with u written in it, in swapped byte order.
 */
template <class T>
char* bswap_mem(char* buf, T& u)
{
	assert(buf != 0);
	memcpy(buf, &bswap(u), sizeof(T));
	return buf;
}


template <class T>
T bswap_mem(const char* buf)
{
	assert(buf != 0);
	T x;
	memcpy(&x, buf, sizeof(T));
	return bswap(x);
}

/* Bit operations */

//! Set bit flag
/**
 * @param u flag container.
 * @param x flag to be set.
 *
 * @return modified flag container.
 */
template <class T>
T& bSetFlag(T& u, T& x)
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
template <class T>
T& bClrFlag(T& u, T& x)
{
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
bool bIsFlag(T& u, T& x)
{
	return (u & x) == x;
}

#endif

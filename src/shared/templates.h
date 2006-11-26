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

#include "sockets.h" // ntohl(), etc.
#include <memory> // memcpy()
#include <stdint.h> // [u]int#_t

/* Swapping endianess.. wrapper for ntohl(), ntohs(), etc. */

//! Swaps endianess (default template).
/**
 * Should only be used with 8 bit types, if others use this, it's an error.
 *
 * @param x scalar to be byte swapped.
 *
 * @return x in its original form. 8 bit types can't be swapped.
 */
template <class T>
T& bswap(T& x)
{
	return x;
}

//! Swaps endianess (16bit specialization)
/**
 * @param x scalar to be byte swapped.
 *
 * @return x with its byte order swapped.
 */
template <uint16_t>
uint16_t bswap(uint16_t& x)
{
	return ntohs(x);
}

//! Swaps endianess (32bit specialization)
/**
 * @param x scalar to be byte swapped.
 *
 * @return x with its byte order swapped.
 */
template <uint32_t>
uint32_t bswap(uint32_t& x)
{
	return ntohl(x);
}

/* // unused
template <class T>
char* bswap_mem(char* buf, T& x)
{
	assert(buf != 0);
	memcpy(buf, bswap(x), sizeof(T));
	return buf;
}
*/

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

/* doesn't work, whines about const'ness */
/*
template <const char*, class T>
T& bswap_mem_t(const char* buf, T& u)
{
	T x;
	memcpy(&x, &bswap<T>(&const_cast<char*>(*buf)), sizeof(T));
	return x;
}
*/

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

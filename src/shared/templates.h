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

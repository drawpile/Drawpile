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

template <class T> T& bswap(T& x)
{
	return x;
}

template <uint16_t>
uint16_t bswap(uint16_t& x)
{
	return ntohs(x);
}

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

/* // unused
template <class T>
T& bswap_mem(T& x, char* buf)
{
	assert(buf != 0);
	memcpy(&x, bswap<T>(*buf), sizeof(T));
	return x;
}
*/

/* doesn't work, whines about const'ness */
template <const char*, class T>
T& bswap_mem_t(const char* buf, T& u)
{
	T x;
	memcpy(&x, bswap<T>(const_cast<char*>(buf)), sizeof(T));
	return x;
}


#endif

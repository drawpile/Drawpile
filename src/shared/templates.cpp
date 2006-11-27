/*******************************************************************************

Copyright (c) 2006 Miika K. Ahonen

Version: Pre-alpha

Authors:
   Miika K. Ahonen (JID: wyrmchild@jabber.org)

*******************************************************************************/

#include "templates.h"

#ifndef BSWAP_T_INCLUDED
#define BSWAP_T_INCLUDED

template <class T>
T& bswap_t::swap(T& x)
{
	return x;
}

template <>
uint32_t& bswap_t<uint32_t>::swap(uint32_t& x)
{
	// (c) 2003 Juan Carlos Cobas
	return x = (((x&0x000000FF)<<24) + ((x&0x0000FF00)<<8) +
		((x&0x00FF0000)>>8) + ((x&0xFF000000)>>24));
}

template <>
uint16_t& bswap_t<uint16_t>::swap(uint16_t& x)
{
	// (c) 2003 Juan Carlos Cobas
	return x = (((x >> 8)) | (x << 8));
}

/*
template <>
uint8_t& bswap_t<uint8_t>::swap(uint8_t& x)
{
	return x;
}
*/

#endif // BSWAP_T_INCLUDED

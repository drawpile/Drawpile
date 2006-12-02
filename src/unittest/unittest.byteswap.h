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

#ifndef UNITTEST_TEMPLATES_H_INCLUDED
#define UNITTEST_TEMPLATES_H_INCLUDED

#include "testing.h"
#include "../shared/templates.h"

/** for unittest related things */
namespace test
{

/** bswap() template */
void byteswapping()
{
	testname("Byte swapping");
	
	testing("bswap(int16)");
	int16_t int16_in = 0x11FF, int16_out = 0xFF11;
	testvars(int16_in, bswap(int16_in), int16_out, true);
	
	testing("bswap(uint16)");
	uint16_t uint16_in = 0x11FF, uint16_out = 0xFF11;
	testvars(uint16_in, bswap(uint16_in), uint16_out);
	
	testing("bswap(int32)");
	int32_t int32_in = 0x113399FF;
	int32_t int32_out = 0xFF993311;
	testvars(int32_in, bswap(int32_in), int32_out, true);
	
	testing("bswap(uint32)");
	uint32_t uint32_in = 0x113399FF;
	uint32_t uint32_out = 0xFF993311;
	testvars(uint32_in, bswap(uint32_in), uint32_out);
	
	testing("bswap(short)");
	short s_in = 0x11FF;
	short s_out = 0xFF11;
	testvars(s_in, bswap(s_in), s_out, true);

	testing("bswap(ushort)");
	unsigned short us_in = 0x11FF;
	unsigned short us_out = 0xFF11;
	testvars(us_in, bswap(us_in), us_out);
}

} // namespace tests

#endif // UNITTEST_TEMPLATES_H_INCLUDED

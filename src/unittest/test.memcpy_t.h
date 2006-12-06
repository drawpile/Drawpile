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

#ifndef UNITTEST_MEMCPY_T_H_INCLUDED
#define UNITTEST_MEMCPY_T_H_INCLUDED

#include "testing.h"
#include "../shared/templates.h"

/** for unittest related things */
namespace test
{

void simple_memcpy()
{
	testname("Memory copy w/o size");
	
	{
		testing("uint8_t -> uint8_t");
		
		uint8_t int8_o = 0;
		uint8_t int8_i = 0xF1;
		
		memcpy_t((char*)&int8_o, int8_i);
		testvars( 
			int8_i,
			int8_o,
			int8_i
		);
	}
	
	{
		testing("uint8_t -> int8_t");
		
		uint8_t int8_o = 0;
		int8_t int8_i = 0xF1;
		
		memcpy_t((char*)&int8_o, int8_i);
		testvars(
			int8_i,
			(int8_t)int8_o,
			int8_i
		);
	}
	
	{
		testing("uint32_t -> char*");
		char char_i[4] = {0x11,0x22,0x33,0x44};
		char char_out[4] = {0,0,0,0};
		uint32_t int_i = 0x11223344;
		
		memcpy_t((char*)&char_out, int_i);
		testvars(
			int_i,
			&char_out,
			&char_i,
			false,
			4
		);
	}
		
	{
		testing("char* -> uint32_t");
		char char_i[4] = {0x11,0x22,0x33,0x44};
		uint32_t int_i = 0x11223344;
		uint32_t int_out = 0;
		
		memcpy_t(int_out, (char*)&char_i);
		testvars(
			&char_i,
			int_out,
			int_i,
			false,
			4
		);
	}
}

} // namespace test

#endif // UNITTEST_MEMCPY_T_H_INCLUDED

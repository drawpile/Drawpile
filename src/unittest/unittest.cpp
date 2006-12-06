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

#include <iostream>

#include "test.sha1.h"
#include "test.bswap.h"
#include "test.memcpy_t.h"

int main(int argc, char** argv)
{
	std::ios::sync_with_stdio(false);
	
	std::cout << "[ DrawPile Unittesting ]\n" << std::endl;
	
	/* start tests */
	
	test::byteswapping();
	test::simple_memcpy();
	test::hashing();
	
	/* end tests*/
	
	/* stats */
	
	std::cout
		<< "\nTests run:    " << total
		<< "\nExpected to fail: " << xpectfail << std::endl;
	
	std::cout
		<< "\nTests passed: " << passed
		<< "\nFail passed:  " << mustfail << std::endl;
	
	std::cout << "\nErrors:"
		<< "\nTests failed: " << failed
		<< "\nFails passed: " << mustfail_passed << std::endl;
	
	return 0;
}

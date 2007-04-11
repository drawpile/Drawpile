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

#ifndef TESTING_H_INCLUDED
#define TESTING_H_INCLUDED

#include <iostream>

static size_t passed=0, failed=0, mustfail=0, mustfail_passed=0, total=0, xpectfail=0;

inline
void testing(char* name)
{
	std::cout << name << std::flush;
}

template <class T>
bool testoutput(T o, T x, size_t s)
{
	return (o == x);
}

template <class T> inline
bool testoutput(T* o, T* x, size_t s)
{
	return (memcmp(o, x, s) == 0);
}

template <class T, class X>
void show_vars(T input, X output, X expect)
{
	std::cout
		<< "input:    " << input
		<< "\noutput:   " << output
		<< "\nexpected: " << expect << std::endl;
}

template <class T, class X>
void show_vars(T input, X* output, X* expect)
{
	std::cout
		<< "input:    " << input
		<< "\noutput:   " << *output
		<< "\nexpected: " << *expect << std::endl;
}

template <class T, class X>
void show_vars(T* input, X output, X expect)
{
	std::cout
		<< "input:    " << *input
		<< "\noutput:   " << output
		<< "\nexpected: " << expect << std::endl;
}

template <class T, class X>
void show_vars(T* input, X* output, X* expect)
{
	std::cout
		<< "input:    " << *input
		<< "\noutput:   " << *output
		<< "\nexpected: " << *expect << std::endl;
}

/* informal */
template <class T, class X>
void testvars(T input, X output, X expect, bool fail=false, size_t array_size=1)
{
	if (fail)
		xpectfail++;
	total++;
	
	if (testoutput(output, expect, array_size) == false)
	{
		if (fail)
		{
			std::cout << " passed (f)" << std::endl;
			
			mustfail++;
		}
		else
		{
			std::cout << " failed!" << std::endl;
			show_vars(input, output, expect);
			
			failed++;
		}
	}
	else
	{
		if (fail)
		{
			std::cout << " failed (f)" << std::endl;
			show_vars(input, output, expect);
			
			mustfail_passed++;
		}
		else
		{
			std::cout << " passed" << std::endl;
			
			passed++;
		}
	}
}

/* informal */
inline
void testname(char* testname)
{
	std::cout << "\nTesting: " << testname << std::endl;
}

#endif

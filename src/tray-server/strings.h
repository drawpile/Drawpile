/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

******************************************************************************/

#ifndef Strings_INCLUDED
#define Strings_INCLUDED

class QString;

//! Qt String conversion
namespace convert {

typedef unsigned int uint;

//! Converts QString to UTF-8 char* array
char* toUTF8(const QString& string, uint& bytes) throw();

//! Converts QString to UTF-16 char* array
char* toUTF16(const QString& string, uint& bytes) throw();

} // namespace:convert

#endif // Strings_INCLUDED

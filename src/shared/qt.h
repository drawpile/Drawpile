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

#ifndef QtShared_INCLUDED
#define QtShared_INCLUDED

class QHostAddress;

namespace Network {

//! Get external host address
/**
 * @return First or only QHostAddress that seems to be external
 */
QHostAddress getExternalAddress();

} // Network

class QString;

//! Qt String conversion
namespace convert {

typedef unsigned int uint;

QString fromUTF(const char *string, uint length, bool Utf16=false);

//! Convenience function
char* toUTF(const QString& string, uint& bytes, bool Utf16=false);

//! Converts QString to UTF-8 char* array
char* toUTF8(const QString& string, uint& bytes);

//! Converts QString to UTF-16 char* array
char* toUTF16(const QString& string, uint& bytes);

} // namespace:convert

#endif // QtShared_INCLUDED

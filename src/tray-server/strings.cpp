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

#include "strings.h"

#include <QByteArray>
#include <QString>

namespace convert {

char* toUTF8(const QString& string, uint& bytes) throw()
{
	QByteArray array = string.toUtf8();
	bytes = array.count();
	char *str = new char[bytes];
	memcpy(str, array.constData(), bytes);
	return str;
}

char* toUTF16(const QString& string, uint& bytes) throw()
{
	//const ushort * array = string.utf16();
	bytes = 0;
	return 0;
}

} // namespace:convert

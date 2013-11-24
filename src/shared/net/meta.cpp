/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
*/

#include <cstring>
#include "meta.h"

namespace protocol {

UserJoin *UserJoin::deserialize(const uchar *data, uint len)
{
	if(len<2)
		return 0;
	return new UserJoin(*data, QByteArray((const char*)data+1, len-1));
}

int UserJoin::serializePayload(uchar *data) const
{
	*data = _id;
	memcpy(data+1, _name.constData(), _name.length());
	return 1 + _name.length();
}

int UserJoin::payloadLength() const
{
	return 1 + _name.length();
}

UserLeave *UserLeave::deserialize(const uchar *data, uint len)
{
	if(len!=1)
		return 0;
	return new UserLeave(*data);
}

int UserLeave::serializePayload(uchar *data) const
{
	*data = _id;
	return 1;
}

int UserLeave::payloadLength() const
{
	return 1;
}

UserAttr *UserAttr::deserialize(const uchar *data, uint len)
{
	if(len!=2)
		return 0;
	return new UserAttr(data[0], data[1]);
}

int UserAttr::serializePayload(uchar *data) const
{
	*data = _id;
	*(data+1) = _attrs;
	return 2;
}

int UserAttr::payloadLength() const
{
	return 2;
}

Chat *Chat::deserialize(const uchar *data, uint len)
{
	if(len<2)
		return 0;
	return new Chat(*data, QByteArray((const char*)data+1, len-1));
}

int Chat::serializePayload(uchar *data) const
{
	*data = _user;
	memcpy(data+1, _msg.constData(), _msg.length());
	return 1 + _msg.length();
}

int Chat::payloadLength() const
{
	return 1 + _msg.length();
}


}

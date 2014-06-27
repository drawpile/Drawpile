/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <QtEndian>
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
	uchar *ptr = data;
	*(ptr++) = contextId();
	memcpy(ptr, _name.constData(), _name.length());
	ptr += _name.length();
	return ptr - data;
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
	*data = contextId();
	return 1;
}

int UserLeave::payloadLength() const
{
	return 1;
}

UserAttr *UserAttr::deserialize(const uchar *data, uint len)
{
	if(len!=3)
		return 0;

	return new UserAttr(
		*(data+0),
		qFromBigEndian<quint16>(data+1));
}

int UserAttr::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_attrs, ptr); ptr += 2;
	return ptr-data;
}

int UserAttr::payloadLength() const
{
	return 1 + 2;
}

SessionTitle *SessionTitle::deserialize(const uchar *data, uint len)
{
	if(len<1)
		return 0;

	return new SessionTitle(data[0], QByteArray((const char*)data+1, len-1));
}

int SessionTitle::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	memcpy(ptr, _title.constData(), _title.length());
	ptr += _title.length();
	return ptr - data;
}

int SessionTitle::payloadLength() const
{
	return 1 + _title.length();
}

SessionConf *SessionConf::deserialize(const uchar *data, uint len)
{
	if(len!=3)
		return 0;

	return new SessionConf(
		*(data+0),
		qFromBigEndian<quint16>(data+1));
}

int SessionConf::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = _maxusers;
	qToBigEndian(_attrs, ptr); ptr += 2;
	return ptr-data;
}

int SessionConf::payloadLength() const
{
	return 3;
}

Chat *Chat::deserialize(const uchar *data, uint len)
{
	if(len<3)
		return 0;
	return new Chat(*data, *(data+1), QByteArray((const char*)data+2, len-2));
}

int Chat::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	*(ptr++) = flags();
	memcpy(ptr, _msg.constData(), _msg.length());
	ptr += _msg.length();
	return ptr - data;
}

int Chat::payloadLength() const
{
	return 2 + _msg.length();
}

MovePointer *MovePointer::deserialize(const uchar *data, uint len)
{
	if(len!=10)
		return 0;
	return new MovePointer(
		*(data+0),
		qFromBigEndian<quint32>(data+1),
		qFromBigEndian<quint32>(data+5),
		*(data+9)
	);
}

int MovePointer::payloadLength() const
{
	return 1 + 2*4 + 1;
}

int MovePointer::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	*(ptr++) = _persistence;

	return ptr-data;
}

}

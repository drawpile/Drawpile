/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

UserJoin *UserJoin::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<1)
		return 0;
	return new UserJoin(ctx, QByteArray((const char*)data, len));
}

int UserJoin::serializePayload(uchar *data) const
{
	memcpy(data, _name.constData(), _name.length());
	return _name.length();
}

int UserJoin::payloadLength() const
{
	return _name.length();
}

UserAttr *UserAttr::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=2)
		return 0;

	return new UserAttr(
		ctx,
		qFromBigEndian<quint16>(data+0));
}

int UserAttr::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_attrs, ptr); ptr += 2;
	return ptr-data;
}

int UserAttr::payloadLength() const
{
	return 2;
}

SessionTitle *SessionTitle::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	return new SessionTitle(ctx, QByteArray((const char*)data, len));
}

int SessionTitle::serializePayload(uchar *data) const
{
	memcpy(data, _title.constData(), _title.length());
	return _title.length();
}

int SessionTitle::payloadLength() const
{
	return _title.length();
}

SessionConf *SessionConf::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=3)
		return 0;

	return new SessionConf(
		ctx,
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

Chat *Chat::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<2)
		return nullptr;
	return new Chat(ctx, *(data+0), QByteArray((const char*)data+1, len-1));
}

int Chat::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = flags();
	memcpy(ptr, _msg.constData(), _msg.length());
	ptr += _msg.length();
	return ptr - data;
}

int Chat::payloadLength() const
{
	return 1 + _msg.length();
}

MovePointer *MovePointer::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=9)
		return nullptr;
	return new MovePointer(
		ctx,
		qFromBigEndian<quint32>(data+0),
		qFromBigEndian<quint32>(data+4),
		*(data+8)
	);
}

int MovePointer::payloadLength() const
{
	return 2*4 + 1;
}

int MovePointer::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	*(ptr++) = _persistence;

	return ptr-data;
}

}

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

#include "meta2.h"

#include <cstring>
#include <QtEndian>

namespace protocol {

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
	memcpy(ptr, m_msg.constData(), m_msg.length());
	ptr += m_msg.length();
	return ptr - data;
}

int Chat::payloadLength() const
{
	return 1 + m_msg.length();
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

UserACL *UserACL::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len>255)
		return nullptr;

	QList<uint8_t> ids;
	ids.reserve(len);
	for(uint i=0;i<len;++i)
		ids.append(data[i]);

	return new UserACL(ctx, ids);
}

int UserACL::serializePayload(uchar *data) const
{
	for(int i=0;i<m_ids.size();++i)
		data[i] = m_ids[i];
	return m_ids.size();
}

int UserACL::payloadLength() const
{
	return m_ids.size();
}


LayerACL *LayerACL::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 3 || len > 3+255)
		return nullptr;
	uint16_t id = qFromBigEndian<quint16>(data+0);
	uint8_t lock = data[2];
	QList<uint8_t> exclusive;
	for(uint i=3;i<len;++i)
		exclusive.append(data[i]);

	return new LayerACL(ctx, id, lock, exclusive);
}

int LayerACL::payloadLength() const
{
	return 3 + _exclusive.count();
}

int LayerACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	*(ptr++) = _locked;
	foreach(uint8_t e, _exclusive)
		*(ptr++) = e;
	return ptr-data;
}

SessionACL *SessionACL::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 2)
		return nullptr;
	uint16_t flags = qFromBigEndian<quint16>(data+0);

	return new SessionACL(ctx, flags);
}

int SessionACL::payloadLength() const
{
	return 2;
}

int SessionACL::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_flags, ptr); ptr += 2;
	return ptr-data;
}

}

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
	if(len<2)
		return 0;
	return new UserJoin(ctx, *(data+0), QByteArray((const char*)data+1, len-1));
}

int UserJoin::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = m_flags;
	memcpy(ptr, m_name.constData(), m_name.length());
	ptr += m_name.length();
	return ptr - data;
}

int UserJoin::payloadLength() const
{
	return 1 + m_name.length();
}

SessionOwner *SessionOwner::deserialize(uint8_t ctx, const uchar *data, int len)
{
	if(len>255)
		return nullptr;

	QList<uint8_t> ids;
	ids.reserve(len);
	for(int i=0;i<len;++i)
		ids.append(data[i]);

	return new SessionOwner(ctx, ids);
}

int SessionOwner::serializePayload(uchar *data) const
{
	for(int i=0;i<m_ids.size();++i)
		data[i] = m_ids[i];
	return m_ids.size();
}

int SessionOwner::payloadLength() const
{
	return m_ids.size();
}

}

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include <QtEndian>

#include "annotation.h"

namespace protocol {

AnnotationCreate *AnnotationCreate::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationCreate(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

int AnnotationCreate::payloadLength() const
{
	return 2 + 4*2 + 2*2;
}

int AnnotationCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

int AnnotationReshape::payloadLength() const
{
	return 2 + 4*2 + 2*2;
}

AnnotationReshape *AnnotationReshape::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=14)
		return 0;
	return new AnnotationReshape(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint16>(data+10),
		qFromBigEndian<quint16>(data+12)
	);
}

int AnnotationReshape::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

AnnotationEdit *AnnotationEdit::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 8)
		return 0;

	return new AnnotationEdit(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<quint32>(data+2),
		*(data+6),
		*(data+7),
		QByteArray((const char*)data+8, len-8)
	);
}

int AnnotationEdit::payloadLength() const
{
	return 2 + 4 + 2 + m_text.length();
}

int AnnotationEdit::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	qToBigEndian(m_bg, ptr); ptr += 4;
	*(ptr++) = m_flags;
	*(ptr++) = m_border;
	memcpy(ptr, m_text.constData(), m_text.length());
	ptr += m_text.length();
	return ptr - data;
}

AnnotationDelete *AnnotationDelete::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 2)
		return 0;
	return new AnnotationDelete(ctx, qFromBigEndian<quint16>(data));
}

int AnnotationDelete::payloadLength() const
{
	return 2;
}

int AnnotationDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(_id, ptr); ptr += 2;
	return ptr-data;
}

}

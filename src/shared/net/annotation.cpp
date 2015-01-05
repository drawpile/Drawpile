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

AnnotationCreate *AnnotationCreate::deserialize(const uchar *data, uint len)
{
	if(len!=15)
		return 0;
	return new AnnotationCreate(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<qint32>(data+3),
		qFromBigEndian<qint32>(data+7),
		qFromBigEndian<quint16>(data+11),
		qFromBigEndian<quint16>(data+13)
	);
}

int AnnotationCreate::payloadLength() const
{
	return 1 + 2 + 4*2 + 2*2;
}

int AnnotationCreate::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

int AnnotationReshape::payloadLength() const
{
	return 1 + 2 + 4*2 + 2*2;
}

AnnotationReshape *AnnotationReshape::deserialize(const uchar *data, uint len)
{
	if(len!=15)
		return 0;
	return new AnnotationReshape(
		*(data+0),
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<qint32>(data+3),
		qFromBigEndian<qint32>(data+7),
		qFromBigEndian<quint16>(data+11),
		qFromBigEndian<quint16>(data+13)
	);
}

int AnnotationReshape::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_x, ptr); ptr += 4;
	qToBigEndian(_y, ptr); ptr += 4;
	qToBigEndian(_w, ptr); ptr += 2;
	qToBigEndian(_h, ptr); ptr += 2;
	return ptr - data;
}

AnnotationEdit *AnnotationEdit::deserialize(const uchar *data, uint len)
{
	if(len < 6)
		return 0;

	return new AnnotationEdit(
		*data,
		qFromBigEndian<quint16>(data+1),
		qFromBigEndian<quint32>(data+3),
		QByteArray((const char*)data+7, len-7)
	);
}

int AnnotationEdit::payloadLength() const
{
	return 1 + 2 + 4 + _text.length();
}

int AnnotationEdit::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	qToBigEndian(_bg, ptr); ptr += 4;
	memcpy(ptr, _text.constData(), _text.length());
	ptr += _text.length();
	return ptr - data;
}

AnnotationDelete *AnnotationDelete::deserialize(const uchar *data, uint len)
{
	if(len != 3)
		return 0;
	return new AnnotationDelete(data[0], qFromBigEndian<quint16>(data+1));
}

int AnnotationDelete::payloadLength() const
{
	return 3;
}

int AnnotationDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	*(ptr++) = contextId();
	qToBigEndian(_id, ptr); ptr += 2;
	return ptr-data;
}

}

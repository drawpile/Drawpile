/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "brushes.h"
#include "textmode.h"

#include <QtEndian>

namespace protocol {

DrawDabsClassic *DrawDabsClassic::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 15)
		return nullptr;

	const int dabCount = (len-15) / ClassicBrushDab::LENGTH;
	if(uint(dabCount * ClassicBrushDab::LENGTH + 15) != len)
		return nullptr;

	DrawDabsClassic *d = new DrawDabsClassic(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<qint32>(data+2),
		qFromBigEndian<qint32>(data+6),
		qFromBigEndian<quint32>(data+10),
		*(data+14)
	);
	d->m_dabs.reserve(dabCount);

	data += 15;

	for(int i=0;i<dabCount;++i) {
		d->m_dabs << ClassicBrushDab {
			int8_t(*(data+0)),
			int8_t(*(data+1)),
			qFromBigEndian<quint16>(data+2),
			*(data+4),
			*(data+5)
		};
		data += ClassicBrushDab::LENGTH;
	}

	return d;
}

int DrawDabsClassic::payloadLength() const
{
	return 2 + 4*3 + 1 + m_dabs.size() * ClassicBrushDab::LENGTH;
}

int DrawDabsClassic::serializePayload(uchar *data) const
{
	Q_ASSERT(m_dabs.size() <= MAX_DABS);

	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_color, ptr); ptr += 4;
	*(ptr++) = m_mode;

	for(const ClassicBrushDab &d : m_dabs) {
		*(ptr++) = d.x;
		*(ptr++) = d.y;
		qToBigEndian(d.radius, ptr); ptr += 2;
		*(ptr++) = d.hardness;
		*(ptr++) = d.opacity;
	}

	return ptr-data;
}

QString ClassicBrushDab::toString() const
{
	return QStringLiteral("%1 %1 %3 %4 %5")
		.arg(x / 4.0, 0, 'f', 1)
		.arg(y / 4.0, 0, 'f', 1)
		.arg(radius / 256.0, 0, 'f', 1)
		.arg(int(hardness))
		.arg(int(opacity))
		;
}

QString DrawDabsClassic::toString() const
{
	QString s = QString::number(contextId()) + " dabs ";
	if(m_dabs.size()==1) {
		s += m_dabs.at(0).toString();

	} else {
		s += "{\n\t";
		for(const ClassicBrushDab &p : m_dabs) {
			s += p.toString();
			s += "\n\t";
		}
		s += "}";
	}
	return s;
}

}


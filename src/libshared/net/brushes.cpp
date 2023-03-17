/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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

#include "libshared/net/brushes.h"
#include "libshared/net/textmode.h"

#include <QtEndian>
#include <QRect>

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
		qToBigEndian(d.size, ptr); ptr += 2;
		*(ptr++) = d.hardness;
		*(ptr++) = d.opacity;
	}

	return ptr-data;
}

bool DrawDabsClassic::payloadEquals(const Message &m) const
{
	const auto &o = static_cast<const DrawDabsClassic&>(m);
	if(m_dabs.size() != o.m_dabs.size())
		return false;

	if(
			m_x != o.m_x ||
			m_y != o.m_y ||
			m_color != o.m_color ||
			m_layer != o.m_layer ||
			m_mode != o.m_mode
			)
		return false;

	for(int i=0;i<m_dabs.size();++i)
		if(m_dabs.at(i) != o.m_dabs.at(i))
			return false;
	return true;
}


QString ClassicBrushDab::toString() const
{
	return QStringLiteral("%1 %2 %3 %4 %5")
		.arg(x / 4.0, 0, 'f', 1)
		.arg(y / 4.0, 0, 'f', 1)
		.arg(size)
		.arg(int(hardness))
		.arg(int(opacity))
		;
}

QString DrawDabsClassic::toString() const
{
	QString s = QStringLiteral("%1 %2 layer=%3 x=%4 y=%5 color=%6 mode=%7 {\n\t")
		.arg(contextId())
		.arg(messageName())
		.arg(text::idString(m_layer))
		.arg(m_x / 4.0, 0, 'f', 1)
		.arg(m_y / 4.0, 0, 'f', 1)
		.arg(text::argbString(m_color))
		.arg(int(m_mode))
		;

	for(const ClassicBrushDab &p : m_dabs) {
		s += p.toString();
		s += "\n\t";
	}
	s += "}";
	return s;
}

DrawDabsClassic *DrawDabsClassic::fromText(uint8_t ctx, const Kwargs &kwargs, const QStringList &dabs)
{
	if(dabs.size() % 5 != 0)
		return nullptr;

	ClassicBrushDabVector dabvector;
	dabvector.reserve(dabs.size() / 5);
	for(int i=0;i<dabs.size();i+=5) {
		dabvector << ClassicBrushDab {
			int8_t(dabs.at(i+0).toFloat() * 4),
			int8_t(dabs.at(i+1).toFloat() * 4),
			uint16_t(dabs.at(i+2).toInt()),
			uint8_t(dabs.at(i+3).toInt()),
			uint8_t(dabs.at(i+4).toInt())
		};
	}

	return new DrawDabsClassic(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs.value("x").toFloat() * 4,
		kwargs.value("y").toFloat() * 4,
		text::parseColor(kwargs["color"]),
		kwargs.value("mode", "1").toInt(),
		dabvector
	);
}

QPoint DrawDabsClassic::lastPoint() const
{
	int x = m_x;
	int y = m_y;
	for(const auto dab : m_dabs) {
		x += dab.x;
		y += dab.y;
	}
	return QPoint(x/4, y/4);
}

QRect DrawDabsClassic::bounds() const
{
	int x = m_x, y = m_y;
	int minX = x, maxX = x;
	int minY = y, maxY = y;
	for(const auto dab : m_dabs) {
		const int r = dab.size/(256*2)*4+1;
		x += dab.x;
		y += dab.y;

		minX = qMin(minX, x - r);
		minY = qMin(minY, y - r);
		maxX = qMax(maxX, x + r);
		maxY = qMax(maxY, y + r);
	}
	return QRect(minX/4, minY/4, (maxX-minX)/4, (maxY-minY)/4);
}

bool DrawDabsClassic::extend(const DrawDabs &dabs)
{
	if(dabs.type() != type())
		return false;
	const auto &ddc = static_cast<const DrawDabsClassic&>(dabs);

	if(m_color != ddc.m_color ||
		m_layer != ddc.m_layer ||
		m_mode != ddc.m_mode)
		return false;

	const int newLength = ddc.dabs().length() + m_dabs.length();
	if(newLength > MAX_DABS)
		return false;

	int lastX = m_x;
	int lastY = m_y;
	for(const auto dab : m_dabs) {
		lastX += dab.x;
		lastY += dab.y;
	}

	auto dab = ddc.dabs().first();

	const int offsetX = ddc.originX() - lastX + dab.x;
	const int offsetY = ddc.originY() - lastY + dab.y;

	if(qAbs(offsetX) > ClassicBrushDab::MAX_XY_DELTA ||
		qAbs(offsetY) > ClassicBrushDab::MAX_XY_DELTA)
		return false;

	m_dabs.reserve(newLength);

	dab.x = offsetX;
	dab.y = offsetY;
	m_dabs << dab;

	for(int i=1;i<ddc.dabs().size();++i)
		m_dabs << ddc.dabs().at(i);

	return true;
}

DrawDabsPixel *DrawDabsPixel::deserialize(DabShape shape, uint8_t ctx, const uchar *data, uint len)
{
	if(len < 15)
		return nullptr;

	const int dabCount = (len-15) / PixelBrushDab::LENGTH;
	if(uint(dabCount * PixelBrushDab::LENGTH + 15) != len)
		return nullptr;

	DrawDabsPixel *d = new DrawDabsPixel(
		shape,
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
		d->m_dabs << PixelBrushDab {
			int8_t(*(data+0)),
			int8_t(*(data+1)),
			*(data+2),
			*(data+3)
		};
		data += PixelBrushDab::LENGTH;
	}

	return d;
}

int DrawDabsPixel::payloadLength() const
{
	return 2 + 4*3 + 1 + m_dabs.size() * PixelBrushDab::LENGTH;
}

int DrawDabsPixel::serializePayload(uchar *data) const
{
	Q_ASSERT(m_dabs.size() <= MAX_DABS);

	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_color, ptr); ptr += 4;
	*(ptr++) = m_mode;

	for(const PixelBrushDab &d : m_dabs) {
		*(ptr++) = d.x;
		*(ptr++) = d.y;
		*(ptr++) = d.size;
		*(ptr++) = d.opacity;
	}

	return ptr-data;
}

bool DrawDabsPixel::payloadEquals(const Message &m) const
{
	const auto &o = static_cast<const DrawDabsPixel&>(m);
	if(m_dabs.size() != o.m_dabs.size())
		return false;

	if(
			m_x != o.m_x ||
			m_y != o.m_y ||
			m_color != o.m_color ||
			m_layer != o.m_layer ||
			m_mode != o.m_mode
			)
		return false;

	for(int i=0;i<m_dabs.size();++i)
		if(m_dabs.at(i) != o.m_dabs.at(i))
			return false;
	return true;
}

QString PixelBrushDab::toString() const
{
	return QStringLiteral("%1 %2 %3 %4")
		.arg(int(x))
		.arg(int(y))
		.arg(int(size))
		.arg(int(opacity))
		;
}

QString DrawDabsPixel::toString() const
{
	QString s = QStringLiteral("%1 %2 layer=%3 x=%4 y=%5 color=%6 mode=%7 {\n\t")
		.arg(contextId())
		.arg(messageName())
		.arg(text::idString(m_layer))
		.arg(m_x)
		.arg(m_y)
		.arg(text::argbString(m_color))
		.arg(int(m_mode))
		;

	for(const PixelBrushDab &p : m_dabs) {
		s += p.toString();
		s += "\n\t";
	}
	s += "}";
	return s;
}

DrawDabsPixel *DrawDabsPixel::fromText(DabShape shape, uint8_t ctx, const Kwargs &kwargs, const QStringList &dabs)
{
	if(dabs.size() % 4 != 0)
		return nullptr;

	PixelBrushDabVector dabvector;
	dabvector.reserve(dabs.size() / 4);
	for(int i=0;i<dabs.size();i+=4) {
		dabvector << PixelBrushDab {
			int8_t(dabs.at(i+0).toInt()),
			int8_t(dabs.at(i+1).toInt()),
			uint8_t(dabs.at(i+2).toInt()),
			uint8_t(dabs.at(i+3).toInt())
		};
	}

	return new DrawDabsPixel(
		shape,
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs.value("x").toInt(),
		kwargs.value("y").toInt(),
		text::parseColor(kwargs["color"]),
		kwargs.value("mode", "1").toInt(),
		dabvector
	);
}

QPoint DrawDabsPixel::lastPoint() const
{
	int x = m_x;
	int y = m_y;
	for(const auto dab : m_dabs) {
		x += dab.x;
		y += dab.y;
	}
	return QPoint(x, y);
}

QRect DrawDabsPixel::bounds() const
{
	int x = m_x, y = m_y;
	int minX = x, maxX = x;
	int minY = y, maxY = y;
	for(const auto dab : m_dabs) {
		const int r = dab.size/2+1;
		x += dab.x;
		y += dab.y;

		minX = qMin(minX, x - r);
		minY = qMin(minY, y - r);
		maxX = qMax(maxX, x + r);
		maxY = qMax(maxY, y + r);
	}
	return QRect(minX, minY, maxX-minX, maxY-minY);
}

bool DrawDabsPixel::extend(const DrawDabs &dabs)
{
	if(dabs.type() != type())
		return false;
	const auto &ddp = static_cast<const DrawDabsPixel&>(dabs);

	if(m_color != ddp.m_color ||
		m_layer != ddp.m_layer ||
		m_mode != ddp.m_mode)
		return false;

	const int newLength = ddp.dabs().length() + m_dabs.length();
	if(newLength > MAX_DABS)
		return false;

	int lastX = m_x;
	int lastY = m_y;
	for(const auto dab : m_dabs) {
		lastX += dab.x;
		lastY += dab.y;
	}

	auto dab = ddp.dabs().first();

	const int offsetX = ddp.originX() - lastX + dab.x;
	const int offsetY = ddp.originY() - lastY + dab.y;

	if(qAbs(offsetX) > ClassicBrushDab::MAX_XY_DELTA ||
		qAbs(offsetY) > ClassicBrushDab::MAX_XY_DELTA)
		return false;

	m_dabs.reserve(newLength);

	dab.x = offsetX;
	dab.y = offsetY;
	m_dabs << dab;

	for(int i=1;i<ddp.dabs().size();++i)
		m_dabs << ddp.dabs().at(i);

	return true;
}

}


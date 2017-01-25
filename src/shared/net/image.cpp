/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

#include "image.h"
#include "textmode.h"

#include <QtEndian>

namespace protocol {

PutImage *PutImage::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 19)
		return 0;

	return new PutImage(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		qFromBigEndian<quint32>(data+3),
		qFromBigEndian<quint32>(data+7),
		qFromBigEndian<quint32>(data+11),
		qFromBigEndian<quint32>(data+15),
		QByteArray((const char*)data+19, len-19)
	);
}

int PutImage::payloadLength() const
{
	return 3 + 4*4 + m_image.size();
}

int PutImage::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	*(ptr++) = m_mode;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_w, ptr); ptr += 4;
	qToBigEndian(m_h, ptr); ptr += 4;
	memcpy(ptr, m_image.constData(), m_image.length());
	ptr += m_image.length();
	return ptr-data;
}

bool PutImage::payloadEquals(const Message &m) const
{
	const PutImage &p = static_cast<const PutImage&>(m);
	return
		layer() == p.layer() &&
		blendmode() == p.blendmode() &&
		x() == p.x() &&
		y() == p.y() &&
		width() == p.width() &&
		height() == p.height() &&
		image() == p.image();
}

Kwargs PutImage::kwargs() const
{
	Kwargs kw;
	kw["layer"] = text::idString(m_layer);
	kw["mode"] = QString::number(m_mode);
	kw["x"] = QString::number(m_x);
	kw["y"] = QString::number(m_y);
	kw["w"] = QString::number(m_w);
	kw["h"] = QString::number(m_h);

	// Split the base64 encoded image data into multiple lines
	// so the text file is nicer to look at
	const int cols = 70;
	QByteArray img = m_image.toBase64();
	QString imgstr;
	imgstr.reserve(img.length() + img.length()/cols);
	imgstr += QString::fromUtf8(img.left(cols));
	for(int i=cols;i<img.length();i+=cols) {
		imgstr += '\n';
		imgstr += QString::fromUtf8(img.mid(i, cols));
	}
	kw["img"] = imgstr;

	return kw;
}

PutImage *PutImage::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new PutImage(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs["mode"].toInt(),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt(),
		QByteArray::fromBase64(kwargs["img"].toUtf8())
		);
}

FillRect *FillRect::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 23)
		return 0;

	return new FillRect(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		qFromBigEndian<quint32>(data+3),
		qFromBigEndian<quint32>(data+7),
		qFromBigEndian<quint32>(data+11),
		qFromBigEndian<quint32>(data+15),
		qFromBigEndian<quint32>(data+19)
	);
}

int FillRect::payloadLength() const
{
	return 3 + 4*4 + 4;
}

int FillRect::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	*(ptr++) = m_blend;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_w, ptr); ptr += 4;
	qToBigEndian(m_h, ptr); ptr += 4;
	qToBigEndian(m_color, ptr); ptr += 4;

	return ptr-data;
}

Kwargs FillRect::kwargs() const
{
	Kwargs kw;
	kw["layer"] = text::idString(m_layer);
	kw["blend"] = QString::number(m_blend);
	kw["color"] = text::argbString(m_color);
	kw["x"] = QString::number(m_x);
	kw["y"] = QString::number(m_y);
	kw["w"] = QString::number(m_w);
	kw["h"] = QString::number(m_h);
	return kw;
}

FillRect *FillRect::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new FillRect(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs["blend"].toInt(),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt(),
		text::parseColor(kwargs["color"])
		);
}


}

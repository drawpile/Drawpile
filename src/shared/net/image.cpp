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

// Split the base64 encoded image data into multiple lines
// so the text file is nicer to look at
static QString splitToColumns(const QByteArray text, int cols)
{
	QString out;
	out.reserve(text.length() + text.length()/cols);
	out += QString::fromUtf8(text.left(cols));
	for(int i=cols;i<text.length();i+=cols) {
		out += '\n';
		out += QString::fromUtf8(text.mid(i, cols));
	}
	return out;
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
	kw["img"] = splitToColumns(m_image.toBase64(), 70);

	return kw;
}

PutImage *PutImage::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QByteArray img = QByteArray::fromBase64(kwargs["img"].toUtf8());
	if(img.isEmpty() || img.length()>MAX_LEN)
		return nullptr;

	return new PutImage(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs.value("mode", "1").toInt(),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt(),
		img
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
		kwargs.value("blend", "1").toInt(),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt(),
		text::parseColor(kwargs["color"])
		);
}

MoveRegion *MoveRegion::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < (2 + 4*4 + 8*4))
		return nullptr;

	return new MoveRegion(
		ctx,
		qFromBigEndian<quint16>(data+0), // layer ID
		qFromBigEndian<quint32>(data+2), // source bounding rect
		qFromBigEndian<quint32>(data+6),
		qFromBigEndian<quint32>(data+10),
		qFromBigEndian<quint32>(data+14),
		qFromBigEndian<quint32>(data+18), // target 1
		qFromBigEndian<quint32>(data+22),
		qFromBigEndian<quint32>(data+26), // target 2
		qFromBigEndian<quint32>(data+30),
		qFromBigEndian<quint32>(data+34), // target 3
		qFromBigEndian<quint32>(data+38),
		qFromBigEndian<quint32>(data+42), // target 4
		qFromBigEndian<quint32>(data+46),
		QByteArray((const char*)data+50, len-50) // source mask
	);
}

int MoveRegion::payloadLength() const
{
	return 2 + 4*4 + 8*4 + m_mask.size();
}

int MoveRegion::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	qToBigEndian(m_bx, ptr); ptr += 4;
	qToBigEndian(m_by, ptr); ptr += 4;
	qToBigEndian(m_bw, ptr); ptr += 4;
	qToBigEndian(m_bh, ptr); ptr += 4;

	qToBigEndian(m_x1, ptr); ptr += 4;
	qToBigEndian(m_y1, ptr); ptr += 4;
	qToBigEndian(m_x2, ptr); ptr += 4;
	qToBigEndian(m_y2, ptr); ptr += 4;
	qToBigEndian(m_x3, ptr); ptr += 4;
	qToBigEndian(m_y3, ptr); ptr += 4;
	qToBigEndian(m_x4, ptr); ptr += 4;
	qToBigEndian(m_y4, ptr); ptr += 4;

	memcpy(ptr, m_mask.constData(), m_mask.length());
	ptr += m_mask.length();
	return ptr-data;
}

Kwargs MoveRegion::kwargs() const
{
	Kwargs kw;
	kw["layer"] = text::idString(m_layer);
	kw["bx"] = QString::number(m_bx);
	kw["by"] = QString::number(m_by);
	kw["bw"] = QString::number(m_bw);
	kw["bh"] = QString::number(m_bh);

	kw["x1"] = QString::number(m_x1);
	kw["y1"] = QString::number(m_y1);
	kw["x2"] = QString::number(m_x2);
	kw["y2"] = QString::number(m_y2);
	kw["x3"] = QString::number(m_x3);
	kw["y3"] = QString::number(m_y3);
	kw["x4"] = QString::number(m_x4);
	kw["y4"] = QString::number(m_y4);

	if(!m_mask.isEmpty())
		kw["mask"] = splitToColumns(m_mask.toBase64(), 70);

	return kw;
}

MoveRegion *MoveRegion::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QByteArray mask = QByteArray::fromBase64(kwargs["mask"].toUtf8());
	if(mask.length()>MAX_LEN)
		return nullptr;

	return new MoveRegion(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs["bx"].toInt(),
		kwargs["by"].toInt(),
		kwargs["bw"].toInt(),
		kwargs["bh"].toInt(),
		kwargs["x1"].toInt(),
		kwargs["y1"].toInt(),
		kwargs["x2"].toInt(),
		kwargs["y2"].toInt(),
		kwargs["x3"].toInt(),
		kwargs["y3"].toInt(),
		kwargs["x4"].toInt(),
		kwargs["y4"].toInt(),
		mask
		);
}

}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/net/image.h"
#include "libshared/net/textmode.h"

#include <QtEndian>

namespace protocol {

PutImage *PutImage::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 19)
		return nullptr;

	return new PutImage(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		qFromBigEndian<quint32>(data+3),
		qFromBigEndian<quint32>(data+7),
		qFromBigEndian<quint32>(data+11),
		qFromBigEndian<quint32>(data+15),
		QByteArray(reinterpret_cast<const char*>(data)+19, len-19)
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

static QByteArray colorByteArray(quint32 c)
{
	QByteArray ba(4, 0);
	qToBigEndian(c, ba.data());
	return ba;
}

PutTile::PutTile(uint8_t ctx, uint16_t layer, uint8_t sublayer, uint16_t col, uint16_t row, uint16_t repeat, uint32_t color)
	: PutTile(ctx, layer, sublayer, col, row, repeat, colorByteArray(color))
{
}

PutTile *PutTile::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 13)
		return nullptr;

	return new PutTile(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		qFromBigEndian<quint16>(data+3),
		qFromBigEndian<quint16>(data+5),
		qFromBigEndian<quint16>(data+7),
		QByteArray(reinterpret_cast<const char*>(data)+9, len-9)
	);
}

int PutTile::payloadLength() const
{
	return 9 + m_image.length();
}

int PutTile::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	*(ptr++) = m_sublayer;
	qToBigEndian(m_col, ptr); ptr += 2;
	qToBigEndian(m_row, ptr); ptr += 2;
	qToBigEndian(m_repeat, ptr); ptr += 2;

	memcpy(ptr, m_image.constData(), m_image.length());
	ptr += m_image.length();

	return ptr-data;
}

uint32_t PutTile::color() const
{
	Q_ASSERT(m_image.length()==4);
	return qFromBigEndian<quint32>(m_image.constData());
}

bool PutTile::payloadEquals(const Message &m) const
{
	const PutTile &p = static_cast<const PutTile&>(m);
	return
		layer() == p.layer() &&
		sublayer() == p.sublayer() &&
		column() == p.column() &&
		row() == p.row() &&
		repeat() == p.repeat() &&
		image() == p.image();
}

Kwargs PutTile::kwargs() const
{
	Kwargs kw;
	kw["layer"] = text::idString(m_layer);
	if(m_sublayer>0)
		kw["sublayer"] = QString::number(m_sublayer);
	kw["row"] = QString::number(m_row);
	kw["col"] = QString::number(m_col);
	if(m_repeat>0)
		kw["repeat"] = QString::number(m_repeat);
	if(isSolidColor())
		kw["color"] = text::argbString(color());
	else
		kw["img"] = splitToColumns(m_image.toBase64(), 70);

	return kw;
}

PutTile *PutTile::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QByteArray img;
	if(kwargs.contains("color")) {
		img = colorByteArray(text::parseColor(kwargs["color"]));

	} else {
		img = QByteArray::fromBase64(kwargs["img"].toUtf8());
		if(img.length()<=4)
			return nullptr;
	}

	return new PutTile(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs["sublayer"].toInt(),
		kwargs["col"].toInt(),
		kwargs["row"].toInt(),
		kwargs["repeat"].toInt(),
		img
		);
}

CanvasBackground::CanvasBackground(uint8_t ctx, uint32_t color)
	: CanvasBackground(ctx, colorByteArray(color))
{
}

CanvasBackground *CanvasBackground::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 4)
		return nullptr;

	return new CanvasBackground(
		ctx,
		QByteArray(reinterpret_cast<const char*>(data), len)
	);
}

int CanvasBackground::payloadLength() const
{
	return m_image.length();
}

int CanvasBackground::serializePayload(uchar *data) const
{
	memcpy(data, m_image.constData(), m_image.length());
	return m_image.length();
}

uint32_t CanvasBackground::color() const
{
	Q_ASSERT(m_image.length()==4);
	return qFromBigEndian<quint32>(m_image.constData());
}

bool CanvasBackground::payloadEquals(const Message &m) const
{
	const CanvasBackground &p = static_cast<const CanvasBackground&>(m);
	return m_image == p.m_image;
}

Kwargs CanvasBackground::kwargs() const
{
	Kwargs kw;
	if(isSolidColor())
		kw["color"] = text::argbString(color());
	else
		kw["img"] = splitToColumns(m_image.toBase64(), 70);

	return kw;
}

CanvasBackground *CanvasBackground::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QByteArray img;
	if(kwargs.contains("color")) {
		img = colorByteArray(text::parseColor(kwargs["color"]));

	} else {
		img = QByteArray::fromBase64(kwargs["img"].toUtf8());
		if(img.length()<=4)
			return nullptr;
	}

	return new CanvasBackground(ctx, img);
}

FillRect *FillRect::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 23)
		return nullptr;

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
		QByteArray(reinterpret_cast<const char*>(data)+50, len-50) // source mask
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

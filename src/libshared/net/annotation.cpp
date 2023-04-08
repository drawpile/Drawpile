// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/net/annotation.h"
#include "libshared/net/textmode.h"

#include <QtEndian>

namespace protocol {

AnnotationCreate *AnnotationCreate::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=14)
		return nullptr;
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
	qToBigEndian(m_id, ptr); ptr += 2;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_w, ptr); ptr += 2;
	qToBigEndian(m_h, ptr); ptr += 2;
	return ptr - data;
}

Kwargs AnnotationCreate::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	kw["x"] = QString::number(m_x);
	kw["y"] = QString::number(m_y);
	kw["w"] = QString::number(m_w);
	kw["h"] = QString::number(m_h);
	return kw;
}

AnnotationCreate *AnnotationCreate::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new AnnotationCreate(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt()
		);
}

int AnnotationReshape::payloadLength() const
{
	return 2 + 4*2 + 2*2;
}

AnnotationReshape *AnnotationReshape::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len!=14)
		return nullptr;
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
	qToBigEndian(m_id, ptr); ptr += 2;
	qToBigEndian(m_x, ptr); ptr += 4;
	qToBigEndian(m_y, ptr); ptr += 4;
	qToBigEndian(m_w, ptr); ptr += 2;
	qToBigEndian(m_h, ptr); ptr += 2;
	return ptr - data;
}

Kwargs AnnotationReshape::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	kw["x"] = QString::number(m_x);
	kw["y"] = QString::number(m_y);
	kw["w"] = QString::number(m_w);
	kw["h"] = QString::number(m_h);
	return kw;
}

AnnotationReshape *AnnotationReshape::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new AnnotationReshape(
		ctx,
		text::parseIdString16(kwargs["id"]),
		kwargs["x"].toInt(),
		kwargs["y"].toInt(),
		kwargs["w"].toInt(),
		kwargs["h"].toInt()
		);
}

AnnotationEdit *AnnotationEdit::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len < 8)
		return nullptr;

	return new AnnotationEdit(
		ctx,
		qFromBigEndian<quint16>(data+0),
		qFromBigEndian<quint32>(data+2),
		*(data+6),
		*(data+7),
		QByteArray(reinterpret_cast<const char*>(data)+8, len-8)
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

Kwargs AnnotationEdit::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	if(m_bg>0)
		kw["bg"] = text::argbString(m_bg);

	if((m_flags&FLAG_PROTECT))
		kw["flags"] = "protect";
	if((m_flags&FLAG_VALIGN_BOTTOM)==FLAG_VALIGN_BOTTOM)
		kw["valign"] = "bottom";
	else if((m_flags&FLAG_VALIGN_CENTER))
		kw["valign"] = "center";

	if(m_border>0)
		kw["border"] = QString::number(m_border);

	kw["text"] = text();
	return kw;
}

AnnotationEdit *AnnotationEdit::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	QStringList flags = kwargs["flags"].split(',');
	return new AnnotationEdit(
		ctx,
		text::parseIdString16(kwargs["id"]),
		text::parseColor(kwargs["bg"]),
		(flags.contains("protect") ? FLAG_PROTECT : 0) |
		(kwargs["valign"]=="bottom" ? FLAG_VALIGN_BOTTOM : 0) |
		(kwargs["valign"]=="center" ? FLAG_VALIGN_CENTER : 0),
		kwargs["border"].toInt(),
		kwargs["text"]
		);
}

AnnotationDelete *AnnotationDelete::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 2)
		return nullptr;
	return new AnnotationDelete(ctx, qFromBigEndian<quint16>(data));
}

int AnnotationDelete::payloadLength() const
{
	return 2;
}

int AnnotationDelete::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_id, ptr); ptr += 2;
	return ptr-data;
}

Kwargs AnnotationDelete::kwargs() const
{
	Kwargs kw;
	kw["id"] = text::idString(m_id);
	return kw;
}

AnnotationDelete *AnnotationDelete::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	return new AnnotationDelete(
		ctx,
		text::parseIdString16(kwargs["id"])
		);
}

}

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

#include "pen.h"
#include "textmode.h"

#include <QtEndian>

namespace protocol {

ToolChange *ToolChange::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len != 18)
		return 0;

	return new ToolChange(
		ctx,
		qFromBigEndian<quint16>(data+0),
		*(data+2),
		*(data+3),
		*(data+4),
		qFromBigEndian<quint32>(data+5),
		*(data+9),
		*(data+10),
		*(data+11),
		*(data+12),
		*(data+13),
		*(data+14),
		*(data+15),
		*(data+16),
		*(data+17)
	);
}

int ToolChange::payloadLength() const
{
	return 18;
}

int ToolChange::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	qToBigEndian(m_layer, ptr); ptr += 2;
	*(ptr++) = m_blend;
	*(ptr++) = m_mode;
	*(ptr++) = m_spacing;
	qToBigEndian(m_color, ptr); ptr += 4;
	*(ptr++) = m_hard_h;
	*(ptr++) = m_hard_l;
	*(ptr++) = m_size_h;
	*(ptr++) = m_size_l;
	*(ptr++) = m_opacity_h;
	*(ptr++) = m_opacity_l;
	*(ptr++) = m_smudge_h;
	*(ptr++) = m_smudge_l;
	*(ptr++) = m_resmudge;
	return ptr-data;
}

Kwargs ToolChange::kwargs() const
{
	Kwargs kw;
	QStringList mode;
	if((m_mode & TOOL_MODE_SUBPIXEL)) mode << "soft";
	if((m_mode & TOOL_MODE_INCREMENTAL)) mode << "inc";
	kw["layer"] = text::idString(m_layer);
	kw["blend"] = QString::number(m_blend);
	kw["mode"] = mode.join(',');
	kw["spacing"] = QString::number(m_spacing);
	kw["color"] = text::rgbString(m_color);
	if(m_hard_h != m_hard_l) {
		kw["hardh"] = text::decimal(m_hard_h);
		kw["hardl"] = text::decimal(m_hard_l);
	} else {
		kw["hard"] = text::decimal(m_hard_h);
	}
	if(m_size_h != m_size_l) {
		kw["sizeh"] = QString::number(m_size_h);
		kw["sizel"] = QString::number(m_size_l);
	} else {
		kw["size"] = QString::number(m_size_h);
	}
	if(m_opacity_h != m_opacity_l) {
		kw["opacityh"] = text::decimal(m_opacity_h);
		kw["opacityl"] = text::decimal(m_opacity_l);
	} else {
		kw["opacity"] = text::decimal(m_opacity_h);
	}
	if(m_smudge_h != m_smudge_l) {
		kw["smudgeh"] = text::decimal(m_smudge_h);
		kw["smudgel"] = text::decimal(m_smudge_l);
	} else if(m_smudge_h>0) {
		kw["smudge"] = text::decimal(m_smudge_h);
	}
	if(m_resmudge>0)
		kw["resmudge"] = QString::number(m_resmudge);
	return kw;
}

ToolChange *ToolChange::fromText(uint8_t ctx, const Kwargs &kwargs)
{
	uint8_t hardh, hardl, sizeh, sizel, opacityh, opacityl, smudgeh, smudgel;
	if(kwargs.contains("hard")) {
		hardh = hardl = text::parseDecimal8(kwargs["hard"]);
	} else {
		hardh = text::parseDecimal8(kwargs["hardh"]);
		hardl = text::parseDecimal8(kwargs["hardl"]);
	}

	if(kwargs.contains("size")) {
		sizeh = sizel = text::parseDecimal8(kwargs["size"]);
	} else {
		sizeh = kwargs.value("sizeh", "1").toInt();
		sizel = kwargs.value("sizel", "1").toInt();
	}

	if(kwargs.contains("opacity")) {
		opacityh = opacityl = text::parseDecimal8(kwargs["opacity"]);
	} else {
		opacityh = text::parseDecimal8(kwargs.value("opacityh", "100"));
		opacityl = text::parseDecimal8(kwargs.value("opacityl", "100"));
	}

	if(kwargs.contains("smudge")) {
		smudgeh = smudgel = text::parseDecimal8(kwargs["smudge"]);
	} else {
		smudgeh = text::parseDecimal8(kwargs["smudgeh"]);
		smudgel = text::parseDecimal8(kwargs["smudgel"]);
	}

	QStringList mode = kwargs["mode"].split(',');

	return new ToolChange(
		ctx,
		text::parseIdString16(kwargs["layer"]),
		kwargs.value("blend", "1").toInt(),
		(mode.contains("inc") ? TOOL_MODE_INCREMENTAL : 0) |
		(mode.contains("soft") ? TOOL_MODE_SUBPIXEL : 0),
		kwargs["spacing"].toInt(),
		text::parseColor(kwargs["color"]),
		hardh, hardl,
		sizeh, sizel,
		opacityh, opacityl,
		smudgeh, smudgel,
		kwargs["resmudge"].toInt()
		);
}

PenMove *PenMove::deserialize(uint8_t ctx, const uchar *data, uint len)
{
	if(len<10 || len%10)
		return nullptr;
	PenPointVector pp;

	int points = len/10;
	pp.reserve(points);
	while(points--) {
		pp.append(PenPoint(
			qFromBigEndian<qint32>(data),
			qFromBigEndian<qint32>(data+4),
			qFromBigEndian<quint16>(data+8)
		));
		data += 10;
	}
	return new PenMove(ctx, pp);
}

int PenMove::payloadLength() const
{
	return 10 * m_points.size();
}

int PenMove::serializePayload(uchar *data) const
{
	uchar *ptr = data;
	for(const PenPoint &p : m_points) {
		qToBigEndian(p.x, ptr); ptr += 4;
		qToBigEndian(p.y, ptr); ptr += 4;
		qToBigEndian(p.p, ptr); ptr += 2;
	}
	return ptr - data;
}

bool PenMove::payloadEquals(const Message &m) const
{
	const PenMove &pm = static_cast<const PenMove&>(m);

	if(points().size() != pm.points().size())
		return false;

	for(int i=0;i<points().size();++i) {
		if(points().at(i) != pm.points().at(i))
			return false;
	}

	return true;
}

static QString penPointToString(const PenPoint &pp)
{
	if(pp.p < 0xffff)
		return QStringLiteral("%1 %2 %3")
			.arg(pp.x / 4.0, 0, 'f', 1)
			.arg(pp.y / 4.0, 0, 'f', 1)
			.arg(pp.p * 100.0 / double(0xffff), 0, 'f', 3);
	else
		return QStringLiteral("%1 %2")
			.arg(pp.x / 4.0, 0, 'f', 1)
			.arg(pp.y / 4.0, 0, 'f', 1);
}

QString PenMove::toString() const
{
	QString s = QString::number(contextId()) + " penmove ";
	if(m_points.size()==1) {
		s += penPointToString(m_points[0]);

	} else {
		s += "{\n\t";
		for(const PenPoint &p : m_points) {
			s += penPointToString(p);
			s += "\n\t";
		}
		s += "}";
	}
	return s;
}

}


/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "brush.h"

#include <QJsonObject>
#include <QDataStream>

namespace brushes {

QJsonObject ClassicBrush::toJson() const
{
	QJsonObject o;
	switch(m_shape) {
	case ROUND_PIXEL: o["shape"] = "round-pixel"; break;
	case SQUARE_PIXEL: o["shape"] = "square-pixel"; break;
	case ROUND_SOFT: o["shape"] = "round-soft"; break;
	}

	o["size"] = m_size1;
	if(m_size2>1) o["size2"] = m_size2;

	o["opacity"] = m_opacity1;
	if(m_opacity2>0) o["opacity2"] = m_opacity2;

	o["hard"] = m_hardness1;
	if(m_hardness2>0) o["hard2"] = m_hardness2;

	if(m_smudge1>0) o["smudge"] = m_smudge1;
	if(m_smudge2>0) o["smudge2"] = m_smudge2;

	o["spacing"] = m_spacing;
	if(m_resmudge>0) o["resmudge"] = m_resmudge;

	if(m_incremental) o["inc"] = true;
	if(m_colorpick) o["colorpick"] = true;
	if(m_sizePressure) o["sizep"] = true;
	if(m_hardnessPressure) o["hardp"] = true;
	if(m_opacityPressure) o["opacityp"] = true;
	if(m_smudgePressure) o["smudgep"] = true;

	o["blend"] = paintcore::findBlendMode(m_blend).svgname;

	// Note: color is intentionally omitted

	return QJsonObject {
		{"type", "dp-classic"},
		{"version", 1},
		{"settings", o}
	};
}

ClassicBrush ClassicBrush::fromJson(const QJsonObject &json)
{
	ClassicBrush b;
	if(json["type"] != "dp-classic") {
		qWarning("ClassicBrush::fromJson: type is not dp-classic!");
		return b;
	}

	const QJsonObject o = json["settings"].toObject();

	if(o["shape"] == "round-pixel")
		b.m_shape = ROUND_PIXEL;
	else if(o["shape"] == "square-pixel")
		b.m_shape = SQUARE_PIXEL;
	else
		b.m_shape = ROUND_SOFT;

	b.setSize(o["size"].toInt());
	b.setSize2(o["size2"].toInt());

	b.setOpacity(o["opacity"].toDouble());
	b.setOpacity2(o["opacity2"].toDouble());

	b.setHardness(o["hard"].toDouble());
	b.setHardness2(o["hard2"].toDouble());

	b.setSmudge(o["smudge"].toDouble());
	b.setSmudge2(o["smudge2"].toDouble());
	b.setResmudge(o["resmudge"].toInt());

	b.setSpacing(o["spacing"].toDouble());

	b.setIncremental(o["inc"].toBool());
	b.setColorPickMode(o["colorpick"].toBool());
	b.setSizePressure(o["sizep"].toBool());
	b.setHardnessPressure(o["hardp"].toBool());
	b.setOpacityPressure(o["opacityp"].toBool());
	b.setSmudgePressure(o["smudgep"].toBool());

	b.setBlendingMode(paintcore::findBlendModeByName(o["blend"].toString(), nullptr).id);

	return b;
}

QDataStream &operator<<(QDataStream &out, const ClassicBrush &b)
{
	return out
		<< int(b.m_shape)
		<< int(b.m_blend)
		<< b.m_size1 << b.m_size2
		<< b.m_resmudge
		<< b.m_hardness1 << b.m_hardness2
		<< b.m_opacity1 << b.m_opacity2
		<< b.m_smudge1 << b.m_smudge2
		<< b.m_spacing
		<< b.m_color
		<< b.m_incremental << b.m_colorpick
		<< b.m_sizePressure << b.m_hardnessPressure << b.m_opacityPressure << b.m_smudgePressure
		;
}

QDataStream &operator>>(QDataStream &in, ClassicBrush &b)
{
	int shape, blend;
	in
		>> shape
		>> blend
		>> b.m_size1 >> b.m_size2
		>> b.m_resmudge
		>> b.m_hardness1 >> b.m_hardness2
		>> b.m_opacity1 >> b.m_opacity2
		>> b.m_smudge1 >> b.m_smudge2
		>> b.m_spacing
		>> b.m_color
		>> b.m_incremental >> b.m_colorpick
		>> b.m_sizePressure >> b.m_hardnessPressure >> b.m_opacityPressure >> b.m_smudgePressure
		;
	b.m_shape = ClassicBrush::Shape(shape);
	b.m_blend = paintcore::BlendMode::Mode(blend);
	return in;
}

}


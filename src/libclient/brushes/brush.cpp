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

ClassicBrush::ClassicBrush()
	: m_brush(rustpile::ClassicBrush {
		{1.0, 10.0},
		{0, 1},
		{0, 1},
		{0, 0},
		0.1,
		0,
		rustpile::Color_BLACK,
		rustpile::ClassicBrushShape::RoundPixel,
		rustpile::Blendmode::Normal,
		true,
		false,
		false,
		false,
		false,
		false
	})
{
}

QJsonObject ClassicBrush::toJson() const
{
	QJsonObject o;
	switch(shape()) {
	case rustpile::ClassicBrushShape::RoundPixel: o["shape"] = "round-pixel"; break;
	case rustpile::ClassicBrushShape::SquarePixel: o["shape"] = "square-pixel"; break;
	case rustpile::ClassicBrushShape::RoundSoft: o["shape"] = "round-soft"; break;
	}

	o["size"] = size1();
	if(size2()>1) o["size2"] = size2();

	o["opacity"] = opacity1();
	if(opacity2()>0) o["opacity2"] = opacity2();

	o["hard"] = hardness1();
	if(opacity2()>0) o["hard2"] = hardness2();

	if(smudge1()>0) o["smudge"] = smudge1();
	if(smudge2()>0) o["smudge2"] = smudge2();

	o["spacing"] = spacing();
	if(resmudge()>0) o["resmudge"] = resmudge();

	if(incremental()) o["inc"] = true;
	if(isColorPickMode()) o["colorpick"] = true;
	if(useSizePressure()) o["sizep"] = true;
	if(useHardnessPressure()) o["hardp"] = true;
	if(useOpacityPressure()) o["opacityp"] = true;
	if(useSmudgePressure()) o["smudgep"] = true;

	o["blend"] = paintcore::findBlendMode(blendingMode()).svgname;

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
		b.setShape(rustpile::ClassicBrushShape::RoundPixel);
	else if(o["shape"] == "square-pixel")
		b.setShape(rustpile::ClassicBrushShape::SquarePixel);
	else
		b.setShape(rustpile::ClassicBrushShape::RoundSoft);

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

#if 0
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
#endif

}


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
#include "canvas/blendmodes.h"

#include <QJsonObject>

namespace brushes {

ClassicBrush::ClassicBrush()
	: rustpile::ClassicBrush{
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
	}
{
}


QJsonObject ClassicBrush::toJson() const
{
	QJsonObject o;
	switch(shape) {
	case rustpile::ClassicBrushShape::RoundPixel: o["shape"] = "round-pixel"; break;
	case rustpile::ClassicBrushShape::SquarePixel: o["shape"] = "square-pixel"; break;
	case rustpile::ClassicBrushShape::RoundSoft: o["shape"] = "round-soft"; break;
	}

	o["size"] = size.max;
	if(size.min>1) o["size2"] = size.min;

	o["opacity"] = opacity.max;
	if(opacity.min>0) o["opacity2"] = opacity.min;

	o["hard"] = hardness.max;
	if(hardness.min>0) o["hard2"] = hardness.min;

	if(smudge.max > 0) o["smudge"] = smudge.max;
	if(smudge.min > 0) o["smudge2"] = smudge.min;

	o["spacing"] = spacing;
	if(resmudge>0) o["resmudge"] = resmudge;

	if(incremental) o["inc"] = true;
	if(colorpick) o["colorpick"] = true;
	if(size_pressure) o["sizep"] = true;
	if(hardness_pressure) o["hardp"] = true;
	if(opacity_pressure) o["opacityp"] = true;
	if(smudge_pressure) o["smudgep"] = true;

	o["blend"] = canvas::blendmode::svgName(mode);

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
		b.shape = rustpile::ClassicBrushShape::RoundPixel;
	else if(o["shape"] == "square-pixel")
		b.shape = rustpile::ClassicBrushShape::SquarePixel;
	else
		b.shape = rustpile::ClassicBrushShape::RoundSoft;

	b.size.max = o["size"].toDouble();
	b.size.min = o["size2"].toDouble();

	b.opacity.max = o["opacity"].toDouble();
	b.opacity.min= o["opacity2"].toDouble();

	b.hardness.max = o["hard"].toDouble();
	b.hardness.min = o["hard2"].toDouble();

	b.smudge.max = o["smudge"].toDouble();
	b.smudge.min = o["smudge2"].toDouble();
	b.resmudge = o["resmudge"].toInt();

	b.spacing = o["spacing"].toDouble();

	b.incremental = o["inc"].toBool();
	b.colorpick = o["colorpick"].toBool();
	b.size_pressure = o["sizep"].toBool();
	b.hardness_pressure = o["hardp"].toBool();
	b.opacity_pressure = o["opacityp"].toBool();
	b.smudge_pressure = o["smudgep"].toBool();

	b.mode = canvas::blendmode::fromSvgName(o["blend"].toString());

	return b;
}

}


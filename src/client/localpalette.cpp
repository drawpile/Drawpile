/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QApplication>
#include "localpalette.h"

LocalPalette::LocalPalette(const QString& name, const QList<QVariant>& list)
	: name_(name)
{
	foreach(QVariant v, list)
		colors_.append(v.value<QColor>());
}

/**
 * Generates a palette with some predefined colors.
 * @return a new palette
 */
LocalPalette *LocalPalette::makeDefaultPalette()
{
	LocalPalette *pal = new LocalPalette(QApplication::tr("Default"));

	for(int value=25;value<255;value+=25) {
		for(int hue=0;hue<345;hue+=35)
			pal->insertColor(0, QColor::fromHsv(hue,255,value));
	}
	return pal;
}

int LocalPalette::count() const
{
	return colors_.count();
}

QColor LocalPalette::color(int index) const
{
	return colors_.at(index);
}

void LocalPalette::setColor(int index, const QColor& color)
{
	colors_[index] = color;
}

void LocalPalette::insertColor(int index, const QColor& color)
{
	colors_.insert(index, color);
}

void LocalPalette::removeColor(int index)
{
	colors_.removeAt(index);
}

QList<QVariant> LocalPalette::toVariantList() const
{
	QList<QVariant> list;
	foreach(QColor col, colors_)
		list.append(QVariant(col));
	return list;
}



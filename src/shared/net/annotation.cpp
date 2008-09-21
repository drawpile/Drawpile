/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#include "annotation.h"

namespace protocol {

/**
 * Check isValid after constructing the annotation.
 */
Annotation::Annotation(const QStringList& tokens)
	: valid_(false)
{
	if(tokens.size() != 16) return;
	id = tokens[1].toInt();
	user = tokens[2].toInt();
	int x, y, w, h;
	x = tokens[3].toInt();
	y = tokens[4].toInt();
	w = tokens[5].toInt();
	h = tokens[6].toInt();
	rect = QRect(x, y, w, h);
	textcolor = tokens[7];
	textalpha = tokens[8].toInt();
	backgroundcolor = tokens[9];
	bgalpha = tokens[10].toInt();
	if(tokens[11]=="L") justify=LEFT;
	else if(tokens[11]=="R") justify=RIGHT;
	else if(tokens[11]=="C") justify=CENTER;
	else if(tokens[11]=="F") justify=FILL;
	bold = tokens[12].contains('B');
	italic = tokens[12].contains('I');
	font = tokens[13];
	size = tokens[14].toInt();
	text = tokens[15];
	valid_ = true;
}

QStringList Annotation::tokens() const {
	QStringList sl;
	sl << "ANNOTATE" << QString::number(id) <<
		QString::number(user) <<
		QString::number(rect.x()) <<
		QString::number(rect.y()) <<
		QString::number(rect.width()) <<
		QString::number(rect.height()) <<
		textcolor << QString::number(textalpha) <<
		backgroundcolor << QString::number(bgalpha);
	switch(justify) {
		case LEFT: sl << "L"; break;
		case RIGHT: sl << "R"; break;
		case CENTER: sl << "C"; break;
		case FILL: sl << "F"; break;
	}
	QString mod;
	if(bold) mod += "B";
	if(italic) mod += "I";
	sl << mod;
	sl << font;
	sl << QString::number(size);
	sl << text;
	return sl;
}

}


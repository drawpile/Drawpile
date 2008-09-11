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

#include <QIODevice>

#include "stroke.h"
#include "utils.h"

namespace protocol {

StrokePoint *StrokePoint::deserialize(QIODevice& data, int len) {
	char user = Utils::read8(data);
	quint16 x = Utils::read16(data);
	quint16 y = Utils::read16(data);
	quint8 z = Utils::read8(data);
	StrokePoint *sp = new StrokePoint(user, x, y ,z);

	// A strokepoint packet may contain multiple points
	for(int i=6;i<len;i+=5) {
		quint16 x = Utils::read16(data);
		quint16 y = Utils::read16(data);
		quint8 z = Utils::read8(data);
		sp->addPoint(x, y, z);
	}

	return sp;
}

void StrokePoint::serializeBody(QIODevice& data) const {
	data.putChar(_user);
	for(int i=0;i<_points.size();++i) {
		Utils::write16(data, _points[i].x);
		Utils::write16(data, _points[i].y);
		data.putChar(_points[i].z);
	}
}

StrokeEnd *StrokeEnd::deserialize(QIODevice& data, int len) {
	return new StrokeEnd(Utils::read8(data));
}

void StrokeEnd::serializeBody(QIODevice& data) const {
	data.putChar(_user);
}

}

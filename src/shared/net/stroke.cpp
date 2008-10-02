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
	char user = utils::read8(data);
	quint16 x = utils::read16(data);
	quint16 y = utils::read16(data);
	quint8 z = utils::read8(data);
	qreal xf = (x & 3) / 4.0;
	qreal yf = (y & 3) / 4.0;
	StrokePoint *sp = new StrokePoint(user, x>>2, y>>2, xf, yf ,z);

	// A strokepoint packet may contain multiple points
	for(int i=6;i<len;i+=5) {
		quint16 x = utils::read16(data);
		quint16 y = utils::read16(data);
		quint8 z = utils::read8(data);
		qreal xf = (x & 3) / 4.0;
		qreal yf = (y & 3) / 4.0;
		sp->addPoint(x>>2, y>>2, xf, yf, z);
	}

	return sp;
}

void StrokePoint::serializeBody(QIODevice& data) const {
	data.putChar(_user);
	for(int i=0;i<_points.size();++i) {
		quint16 x = _points[i].x << 2;
		quint16 xf = quint16(_points[i].xf * 4.0) & 3;
		quint16 y = _points[i].y << 2;
		quint16 yf = quint16(_points[i].yf * 4.0) & 3;
		utils::write16(data, x | xf);
		utils::write16(data, y | yf);
		data.putChar(_points[i].z);
	}
}

StrokeEnd *StrokeEnd::deserialize(QIODevice& data, int len) {
	return new StrokeEnd(utils::read8(data));
}

void StrokeEnd::serializeBody(QIODevice& data) const {
	data.putChar(_user);
}

}

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef CANVAS_PRESSURE_H
#define CANVAS_PRESSURE_H

#include "libclient/utils/kis_cubic_curve.h"

struct PressureMapping {
	enum Mode {
		STYLUS, // real tablet pressure info, no parameter
		DISTANCE, // param=max distance
		VELOCITY // param=max velocity
	};

	Mode mode;
	KisCubicCurve curve;
	qreal param;
};

Q_DECLARE_METATYPE(PressureMapping)

#endif


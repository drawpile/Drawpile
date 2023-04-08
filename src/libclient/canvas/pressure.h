// SPDX-License-Identifier: GPL-3.0-or-later

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


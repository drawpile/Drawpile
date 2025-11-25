// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/view/zoom.h"
#include "libclient/view/enums.h"
#include <cmath>

namespace view {

static bool zoomLevelsHardware;
static QVector<qreal> zoomLevels;

qreal getZoomMin()
{
	if(zoomLevelsHardware) {
		return 0.0078125;
	} else {
		return 0.05;
	}
}

qreal getZoomMax()
{
	return 64.0;
}

qreal getZoomSoftMin()
{
	return 0.125;
}

qreal getZoomSoftMax()
{
	if(zoomLevelsHardware) {
		return 32.0;
	} else {
		return 8.0;
	}
}


const QVector<qreal> &getZoomLevels()
{
	if(zoomLevels.isEmpty()) {
		if(zoomLevelsHardware) {
			// This set of zoom levels gives slightly nicer interpolation
			// results for mipmaps when scroll zooming. It computes the same
			// zoom levels as Paint Tool SAIv2.

			// Divisions per integer power; Also best as a power of 2.
			const int substep = 4;
			const int minlevel = std::round(std::log2(getZoomMin()) * substep);
			const int maxlevel = std::round(std::log2(getZoomMax()) * substep);
			// Set to 1 to have the same step count as SAI. Higher number makes
			// scroll zoom faster.
			const int step = 2;
			for(int i = minlevel; i <= maxlevel; i += step) {
				zoomLevels.append(
					std::pow(2, static_cast<double>(i) / substep));
			}
		} else {
			// Zoom levels close to what Krita does, but nudged to cause less
			// jitter in the software renderers.
			zoomLevels = {
				0.0625, 0.08, 0.125, 0.2, 0.25, 0.375, 0.5,	 0.75, 1.0,	 1.5,
				2.0,	3.0,  4.0,	 6.0, 8.0,	12.0,  16.0, 24.0, 32.0, 48.0,
			};
		}
	}
	return zoomLevels;
}

void setZoomLevelsCanvasImplementation(int canvasImplementation)
{
	bool hardware =
		canvasImplementation == int(view::CanvasImplementation::OpenGl);
	if(zoomLevelsHardware != hardware) {
		zoomLevelsHardware = hardware;
		zoomLevels.clear();
	}
}

}

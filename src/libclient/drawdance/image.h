// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_IMAGE_H
#define DRAWDANCE_IMAGE_H
#include <QImage>

typedef struct DP_Image DP_Image;
typedef union DP_Pixel8 DP_Pixel8;

namespace drawdance {

QImage wrapImage(DP_Image *img);

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels);

QColor sampleColorAt(
	const QImage &img, uint16_t *stampBuffer, int x, int y, int diameter,
	bool opaque, int &lastDiameter);

QImage transformImage(
	const QImage &source, const QPolygon &dstQuad, int interpolation,
	QPoint *outOffset = nullptr);

}

#endif

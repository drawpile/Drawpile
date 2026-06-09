// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_IMAGE_H
#define DRAWDANCE_IMAGE_H
#include <QByteArray>
#include <QImage>
#include <QString>

typedef struct DP_Image DP_Image;
typedef union DP_Pixel8 DP_Pixel8;

namespace drawdance {

QImage wrapImage(DP_Image *img);

// Decodes an image file at `path` using the Drawdance impex loader (which
// supports WebP and QOI in addition to PNG/JPEG), falling back to Qt's
// QImage loader when Drawdance can't decode it. Returns a null QImage on
// failure; when `outError` is non-null it receives an error description.
QImage loadImage(const QString &path, QString *outError = nullptr);

// Decodes in-memory image bytes the same way. Used for clipboard/drop data.
QImage loadImage(const QByteArray &bytes, QString *outError = nullptr);

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels);

QColor sampleColorAt(
	const QImage &img, uint16_t *stampBuffer, int x, int y, int diameter,
	bool opaque, int &lastDiameter);

QImage transformImage(
	const QImage &source, const QPolygon &dstQuad, int interpolation,
	bool checkBounds, QPoint *outOffset = nullptr);

QSize thumbnailDimensions(const QSize &size, const QSize &maxSize);

}

#endif

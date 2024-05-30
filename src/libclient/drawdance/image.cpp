// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/image.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/image.h"

static void cleanupImage(void *user)
{
	DP_image_free(static_cast<DP_Image *>(user));
}

static void cleanupPixels8(void *user)
{
	DP_free(static_cast<DP_Pixel8 *>(user));
}

static DP_Pixel8 getPixel(void *user, int x, int y)
{
	const QImage *img = static_cast<const QImage *>(user);
	DP_Pixel8 pixel;
	pixel.color = img->pixel(x, y);
	return pixel;
}

namespace drawdance {

QImage wrapImage(DP_Image *img)
{
	if(img) {
		return QImage{
			reinterpret_cast<const uchar *>(DP_image_pixels(img)),
			DP_image_width(img),
			DP_image_height(img),
			QImage::Format_ARGB32_Premultiplied,
			cleanupImage,
			img};
	} else {
		return QImage{};
	}
}

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels)
{
	if(width > 0 && height > 0 && pixels) {
		return QImage{
			reinterpret_cast<const uchar *>(pixels), width,			 height,
			QImage::Format_ARGB32_Premultiplied,	 cleanupPixels8, pixels};
	} else {
		return QImage{};
	}
}

QColor sampleColorAt(
	const QImage &img, uint16_t *stampBuffer, int x, int y, int diameter,
	bool opaque, int &lastDiameter)
{
	DP_UPixelFloat pixel = DP_image_sample_color_at_with(
		img.width(), img.height(), getPixel, const_cast<QImage *>(&img),
		stampBuffer, x, y, diameter, opaque, &lastDiameter);
	QColor color;
	color.setRgbF(pixel.r, pixel.g, pixel.b, pixel.a);
	return color;
}

QImage transformImage(
	const QImage &source, const QPolygon &dstQuad, int interpolation,
	QPoint *outOffset)
{
	if(source.isNull()) {
		DP_error_set("Source image is null");
	} else {
		DrawContext drawContext = DrawContextPool::acquire();
		DP_Quad quad = DP_quad_make(
			dstQuad.point(0).x(), dstQuad.point(0).y(), dstQuad.point(1).x(),
			dstQuad.point(1).y(), dstQuad.point(2).x(), dstQuad.point(2).y(),
			dstQuad.point(3).x(), dstQuad.point(3).y());
		int offsetX, offsetY;
		DP_Image *img = DP_image_transform_pixels(
			source.width(), source.height(),
			reinterpret_cast<const DP_Pixel8 *>(source.constBits()),
			drawContext.get(), &quad, interpolation, &offsetX, &offsetY);
		if(img && outOffset) {
			outOffset->setX(offsetX);
			outOffset->setY(offsetY);
			return wrapImage(img);
		}
	}
	return QImage();
}

}

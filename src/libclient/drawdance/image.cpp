// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpcommon/input.h>
#include <dpengine/image.h>
#include <dpimpex/image_impex.h>
}
#include "libclient/drawdance/global.h"
#include "libclient/drawdance/image.h"
#include <QImageReader>

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

static QImage decodeWithDrawdance(DP_Input *input)
{
	// input is consumed/freed here.
	DP_Image *dpImg = input
						  ? DP_image_new_from_file(
								input, DP_IMAGE_FILE_TYPE_GUESS, nullptr)
						  : nullptr;
	if(input) {
		DP_input_free(input);
	}
	return wrapImage(dpImg);
}

QImage wrapImage(DP_Image *img)
{
	if(img) {
		return QImage{
			reinterpret_cast<uchar *>(DP_image_pixels(img)),
			DP_image_width(img),
			DP_image_height(img),
			QImage::Format_ARGB32_Premultiplied,
			cleanupImage,
			img};
	} else {
		return QImage{};
	}
}

QImage loadImage(const QString &path, QString *outError)
{
	QByteArray pathBytes = path.toUtf8();
	DP_Input *input = DP_file_input_new_from_path(pathBytes.constData());
	QImage img = decodeWithDrawdance(input);
	if(!img.isNull()) {
		return img;
	}

	QImageReader reader(path);
	QImage qtImg;
	if(reader.read(&qtImg)) {
		return qtImg;
	}
	if(outError) {
		*outError = reader.errorString();
	}
	return QImage();
}

QImage loadImage(const QByteArray &bytes, QString *outError)
{
	DP_Input *input = bytes.isEmpty()
						  ? nullptr
						  : DP_mem_input_new_keep_on_close(
								bytes.constData(),
								static_cast<size_t>(bytes.size()));
	QImage img = decodeWithDrawdance(input);
	if(!img.isNull()) {
		return img;
	}

	QImage qtImg;
	if(qtImg.loadFromData(bytes)) {
		return qtImg;
	}
	if(outError) {
		*outError = QStringLiteral("Unsupported or corrupt image data");
	}
	return QImage();
}

QImage wrapPixels8(int width, int height, DP_Pixel8 *pixels)
{
	if(width > 0 && height > 0 && pixels) {
		return QImage{
			reinterpret_cast<uchar *>(pixels),	 width,			 height,
			QImage::Format_ARGB32_Premultiplied, cleanupPixels8, pixels};
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
	bool checkBounds, QPoint *outOffset)
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
			drawContext.get(), &quad, interpolation, checkBounds, &offsetX,
			&offsetY);
		if(img && outOffset) {
			outOffset->setX(offsetX);
			outOffset->setY(offsetY);
			return wrapImage(img);
		}
	}
	return QImage();
}

QSize thumbnailDimensions(const QSize &size, const QSize &maxSize)
{
	if(size.isEmpty() || maxSize.isEmpty()) {
		return QSize();
	} else {
		int width, height;
		DP_image_thumbnail_dimensions(
			size.width(), size.height(), maxSize.width(), maxSize.height(),
			&width, &height);
		return QSize(width, height);
	}
}

}

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "images.h"

#include <QSize>
#include <QImageWriter>
#include <QImage>
#include <QColor>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size)
{
	// The protocol limits width and height to 2^29 (PenMove, 2^32 for others)
	// However, QPixmap can be at most 32767 pixels wide/tall
	static const int MAX_SIZE = 32767;

	// Rather than limiting the image dimensions to a safe square, allow large rectangular images
	// by limiting size by required memory
	static const quint64 MAX_MEM = 1024 * 1024 * 1024;
	static const quint64 MAX_PIXELS = MAX_MEM / 4;

	return
		size.width() <= MAX_SIZE &&
		size.height() <= MAX_SIZE &&
		quint64(size.width())*quint64(size.height()) <= MAX_PIXELS;
}

bool isWritableFormat(const QString &filename)
{
	const int dot = filename.lastIndexOf('.');
	if(dot<0)
		return false;
	const QByteArray suffix = filename.mid(dot+1).toUtf8();

	for(const QPair<QString,QByteArray> &fmt : writableImageFormats()) {
		if(fmt.second == suffix)
			return true;
	}

	return false;
}

QList<QPair<QString,QByteArray>> writableImageFormats()
{
	QList<QPair<QString,QByteArray>> formats;

	// We support ORA ourselves
	formats.append(QPair<QString,QByteArray>("OpenRaster", "ora"));

	// Get list of available formats
	for(const QByteArray &fmt : QImageWriter::supportedImageFormats())
	{
		// only offer a reasonable subset
		if(fmt == "png" || fmt=="jpeg" || fmt=="bmp" || fmt=="gif" || fmt=="tiff")
			formats.append(QPair<QString,QByteArray>(QString(fmt).toUpper(), fmt));
		else if(fmt=="jp2")
			formats.append(QPair<QString,QByteArray>("JPEG2000", fmt));
	}
	return formats;
}

QColor isSolidColorImage(const QImage &image)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32);
	if(image.format() != QImage::Format_ARGB32) {
		qWarning("isSolidColorImage: not an ARGB32 image!");
		return QColor();
	}

	const quint32 c = *reinterpret_cast<const quint32*>(image.bits());
	const quint32 *p = reinterpret_cast<const quint32*>(image.bits());

	int len = image.width() * image.height();
	while(--len) {
		if(*(++p) != c)
			return QColor();
	}
	return QColor::fromRgba(c);
}

}

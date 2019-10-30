/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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
#include <QImageReader>
#include <QImageWriter>
#include <QImage>
#include <QColor>
#include <QGuiApplication>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size)
{
	// The protocol limits width and height to 2^29 (PenMove, 2^32 for others)
	// However, QPixmap can be at most 32767 pixels wide/tall
	static const int MAX_SIZE = 32767;

	return
		size.width() <= MAX_SIZE &&
		size.height() <= MAX_SIZE;
}

bool isWritableFormat(const QString &filename)
{
	const int dot = filename.lastIndexOf('.');
	if(dot<0)
		return false;
	const QByteArray suffix = filename.mid(dot+1).toLower().toLatin1();

	// Formats we support
	if(suffix == "ora")
		return true;

	// All formats supported by Qt
	for(const QByteArray &fmt : QImageWriter::supportedImageFormats()) {
		if(suffix == fmt)
			return true;
	}

	return false;
}

QVector<QPair<QString,QByteArray>> writableImageFormats()
{
	QVector<QPair<QString,QByteArray>> formats;

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

QString fileFormatFilter(FileFormatOptions formats)
{
	QStringList filter;
	QString readImages, recordings;

	if(formats.testFlag(FileFormatOption::Images)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			// List all image formats for saving
			for(const auto &format : utils::writableImageFormats()) {
				filter << QStringLiteral("%1 (*.%2)").arg(format.first, QString::fromLatin1(format.second));
			}

		} else {
			// A single Images filter for loading
			if(!formats.testFlag(FileFormatOption::QtImagesOnly))
				readImages = "*.ora ";

			for(QByteArray format : QImageReader::supportedImageFormats()) {
				readImages += "*." + format + " ";
			}

			filter << QGuiApplication::tr("Images (%1)").arg(readImages);
		}
	}

	if(formats.testFlag(FileFormatOption::Recordings)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			// Recording formats individually for saving
			filter
				<< QGuiApplication::tr("Binary Recordings (%1)").arg("*.dprec")
				<< QGuiApplication::tr("Text Recordings (%1)").arg("*.dptxt")
				<< QGuiApplication::tr("Compressed Binary Recordings (%1)").arg("*.dprecz")
				<< QGuiApplication::tr("Compressed Text Recordings (%1)").arg("*.dptxtz")
				;

		} else {
			// A single Recordings filter for loading
			recordings = "*.dprec *.dptxt *.dprecz *.dptxtz *.dprec.gz *.dptxt.gz";
			filter
				<< QGuiApplication::tr("Recordings (%1)").arg(recordings)
				;
		}
	}

	if(!readImages.isEmpty() && !recordings.isEmpty()) {
		filter.prepend(
			QGuiApplication::tr("All Supported Files (%1)").arg(readImages + recordings)
		);
	}

	// An all files filter when requested
	if(formats.testFlag(FileFormatOption::AllFiles)) {
		filter << QGuiApplication::tr("All Files (*)");
	}

	return filter.join(";;");
}

QColor isSolidColorImage(const QImage &image)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);
	if(image.format() != QImage::Format_ARGB32_Premultiplied) {
		qWarning("isSolidColorImage: not a premultiplied ARGB32 image!");
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

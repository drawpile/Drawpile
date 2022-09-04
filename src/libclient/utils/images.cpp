/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

using WritableImageFormat = QPair<QString,QVector<QByteArray>>;

static const QVector<WritableImageFormat> &writableImageFormats()
{
	static QVector<WritableImageFormat> formats;

	if(formats.isEmpty()) {
		// List of formats supported by Rustpile
		// (See the image crate's features in dpimpex/Cargo.toml)
		formats
			<< WritableImageFormat("OpenRaster", {"ora"})
			<< WritableImageFormat("JPEG", {"jpeg", "jpg"})
			<< WritableImageFormat("PNG", {"png"})
			<< WritableImageFormat("GIF", {"gif"})
			;
	}

	return formats;
}

bool isWritableFormat(const QString &filename)
{
	const int dot = filename.lastIndexOf('.');
	if(dot<0)
		return false;
	const QByteArray suffix = filename.mid(dot+1).toLower().toLatin1();

	for(const WritableImageFormat &pair : writableImageFormats()) {
		if(pair.second.contains(suffix))
			return true;
	}

	return false;
}

QString fileFormatFilter(FileFormatOptions formats)
{
	QStringList filter;
	QString readImages, recordings;

	if(formats.testFlag(FileFormatOption::Images)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			if(formats.testFlag(FileFormatOption::QtImagesOnly)) {
				for(const QByteArray &fmt : QImageWriter::supportedImageFormats()) {
					filter << QStringLiteral("%1 (*.%2)").arg(QString::fromLatin1(fmt.toUpper()), QString::fromLatin1(fmt));
				}

			} else {
				for(const WritableImageFormat &format : writableImageFormats()) {
					QStringList extensions;
					for(const QByteArray &extension : format.second) {
						extensions << QStringLiteral("*.%1").arg(QString::fromUtf8(extension));
					}
					filter << QStringLiteral("%1 (%2)").arg(format.first, extensions.join(" "));
				}
			}

		} else {
			// A single Images filter for loading
			if(formats.testFlag(FileFormatOption::QtImagesOnly)) {
				for(QByteArray format : QImageReader::supportedImageFormats()) {
					readImages += "*." + format + " ";
				}
			} else {
				// Formats supported by Rustpile
				readImages = "*.ora *.png *.jpg *.jpeg *.gif";
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
				;

		} else {
			// A single Recordings filter for loading
			recordings = "*.dprec *.dptxt";
			filter
				<< QGuiApplication::tr("Recordings (%1)").arg(recordings)
				;
		}
	}

	if(!readImages.isEmpty() && !recordings.isEmpty()) {
		filter.prepend(
			QGuiApplication::tr("All Supported Files (%1)").arg(readImages + ' ' + recordings)
		);
	}

	// An all files filter when requested
	if(formats.testFlag(FileFormatOption::AllFiles)) {
		filter << QGuiApplication::tr("All Files (*)");
	}

	return filter.join(";;");
}

}

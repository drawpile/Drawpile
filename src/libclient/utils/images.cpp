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

#include "config.h"
#include "libclient/utils/images.h"

extern "C" {
#include <dpengine/load.h>
#include <dpengine/save.h>
}

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

using ImageFormat = QPair<QString,QVector<QString>>;

static void appendFormats(QVector<ImageFormat> &formats, const char *title, const char **extensions)
{
	ImageFormat format{QString::fromUtf8(title), {}};
	for(const char **ext = extensions; *ext; ++ext) {
		format.second.append(QString::fromUtf8(*ext));
	}
	formats.append(format);
}

static const QVector<ImageFormat> &writableImageFormats()
{
	static QVector<ImageFormat> formats;
	if(formats.isEmpty()) {
		for(const DP_SaveFormat *sf = DP_save_supported_formats(); sf->title; ++sf) {
			appendFormats(formats, sf->title, sf->extensions);
		}
	}
	return formats;
}

bool isWritableFormat(const QString &filename)
{
	const int dot = filename.lastIndexOf('.');
	if(dot<0)
		return false;
	const QString suffix = filename.mid(dot+1).toLower();

	for(const ImageFormat &pair : writableImageFormats()) {
		if(pair.second.contains(suffix))
			return true;
	}

	return false;
}

QStringList fileFormatFilterList(FileFormatOptions formats)
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
				for(const ImageFormat &format : writableImageFormats()) {
					QStringList extensions;
					for(const QString &extension : format.second) {
						extensions << QStringLiteral("*.%1").arg(extension);
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
				readImages = DRAWPILE_FILE_GROUP_IMAGE;
			}

			filter << QGuiApplication::tr("Images (%1)").arg(readImages);
		}
	}

	if(formats.testFlag(FileFormatOption::Gif)) {
		filter << QGuiApplication::tr("GIF (%1)").arg("*.gif");
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
			recordings = DRAWPILE_FILE_GROUP_RECORDING;
			filter
				<< QGuiApplication::tr("Recordings (%1)").arg(recordings)
				;
		}
	}

	if(formats.testFlag(FileFormatOption::Profile)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			filter << QGuiApplication::tr("Performance Profile (%1)").arg("*.dpperf");
		} else {
			// Can't read performance profiles.
		}
	}

	if(formats.testFlag(FileFormatOption::DebugDumps)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			// Can't write debug dumps.
		} else {
			filter << QGuiApplication::tr("Debug Dumps (%1)").arg("*.drawdancedump");
		}
	}

	if(formats.testFlag(FileFormatOption::EventLog)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			filter << QGuiApplication::tr("Tablet Event Log (%1)").arg("*.dplog");
		} else {
			// Can't read event logs.
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

	return filter;
}

QString fileFormatFilter(FileFormatOptions formats)
{
	return fileFormatFilterList(formats).join(";;");
}

}

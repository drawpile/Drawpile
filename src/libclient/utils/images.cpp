// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/images.h"
#include "cmake-config/config.h"

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
				readImages = cmake_config::file_group::image();
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
			recordings = cmake_config::file_group::recording();
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

	if(formats.testFlag(FileFormatOption::Mp4)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			filter << QGuiApplication::tr("MP4 Video (%1)").arg("*.mp4");
		} else {
			// Can't read MP4 videos.
		}
	}

	if(formats.testFlag(FileFormatOption::Webm)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			filter << QGuiApplication::tr("WebM Video (%1)").arg("*.webm");
		} else {
			// Can't read WebM videos.
		}
	}

	if(formats.testFlag(FileFormatOption::Text)) {
		if(formats.testFlag(FileFormatOption::Save)) {
			filter << QGuiApplication::tr("Text File (%1)").arg("*.txt");
		} else {
			// Can't read text files.
		}
	}

	if(formats.testFlag(FileFormatOption::BrushPack)) {
		filter << QGuiApplication::tr("Brush Pack (%1)").arg("*.zip");
	}

	if(formats.testFlag(FileFormatOption::SessionBans)) {
		filter << QGuiApplication::tr("Session Bans (%1)").arg("*.dpbans");
	}

	if(formats.testFlag(FileFormatOption::AuthList)) {
		filter << QGuiApplication::tr("Roles (%1)").arg("*.dproles");
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

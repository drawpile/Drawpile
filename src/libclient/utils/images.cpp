// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/images.h"
#include "cmake-config/config.h"
#include "libclient/drawdance/image.h"

extern "C" {
#include <dpcommon/input.h>
#include <dpimpex/image_impex.h>
#include <dpimpex/load.h>
#include <dpimpex/save.h>
}

#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QImageReader>
#include <QImageWriter>
#include <QGuiApplication>

namespace utils {

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

static QString normalizedSuffix(QString suffix)
{
	if(suffix.startsWith('.')) {
		suffix.remove(0, 1);
	}
	return suffix.toCaseFolded();
}

static bool suffixMatchesPatternList(const QString &suffix, const char *patterns)
{
	const QString normalized = normalizedSuffix(suffix);
	for(const QString &pattern :
		QString::fromUtf8(patterns).split(' ', Qt::SkipEmptyParts)) {
		QString extension = pattern;
		if(extension.startsWith(QStringLiteral("*."))) {
			extension.remove(0, 2);
		}
		if(normalized == extension.toCaseFolded()) {
			return true;
		}
	}
	return false;
}

static void appendUniqueImagePattern(QStringList &patterns, const QString &pattern)
{
	if(!patterns.contains(pattern, Qt::CaseInsensitive)) {
		patterns.append(pattern);
	}
}

static QString qtAndNativeFlatImageFilterPatterns()
{
	QStringList patterns;
	for(QByteArray format : QImageReader::supportedImageFormats()) {
		appendUniqueImagePattern(
			patterns,
			QStringLiteral("*.%1").arg(QString::fromLatin1(format)));
	}
	for(const QString &pattern : QString::fromUtf8(
			cmake_config::file_group::flatImage())
								  .split(' ', Qt::SkipEmptyParts)) {
		appendUniqueImagePattern(patterns, pattern);
	}
	return patterns.join(' ');
}

static QImage loadImageWithQt(const QString &path, QString &outError)
{
	QImage img;
	QImageReader reader(path);
	if(reader.read(&img) && !img.isNull()) {
		outError.clear();
		return img;
	} else {
		outError = reader.errorString();
		return QImage{};
	}
}

static QImage loadImageWithDrawdance(const QString &path, QString &outError)
{
	QByteArray pathBytes = path.toUtf8();
	DP_Input *input = DP_file_input_new_from_path(pathBytes.constData());
	if(!input) {
		outError = QString::fromUtf8(DP_error());
		return QImage{};
	}

	DP_Image *img =
		DP_image_new_from_file(input, DP_IMAGE_FILE_TYPE_GUESS, nullptr);
	DP_input_free(input);
	if(img) {
		outError.clear();
		return drawdance::wrapImage(img);
	} else {
		outError = QString::fromUtf8(DP_error());
		return QImage{};
	}
}

bool isLoadableImageFileSuffix(const QString &suffix)
{
	return QImageReader::supportedImageFormats().contains(
			   normalizedSuffix(suffix).toUtf8()) ||
		   suffixMatchesPatternList(suffix, cmake_config::file_group::flatImage());
}

QImage loadImageFromFile(const QString &path, QString *outError)
{
	const QString suffix = QFileInfo(path).suffix();
	const bool preferDrawdance =
		suffix.compare(QStringLiteral("webp"), Qt::CaseInsensitive) == 0 ||
		suffix.compare(QStringLiteral("qoi"), Qt::CaseInsensitive) == 0;

	QString primaryError;
	QString fallbackError;
	QImage img = preferDrawdance ? loadImageWithDrawdance(path, primaryError)
								 : loadImageWithQt(path, primaryError);
	if(!img.isNull()) {
		if(outError) {
			outError->clear();
		}
		return img;
	}

	img = preferDrawdance ? loadImageWithQt(path, fallbackError)
						  : loadImageWithDrawdance(path, fallbackError);
	if(!img.isNull()) {
		if(outError) {
			outError->clear();
		}
		return img;
	}

	if(outError) {
		*outError = fallbackError.isEmpty() ? primaryError : fallbackError;
	}
	return QImage{};
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
				readImages = qtAndNativeFlatImageFilterPatterns();
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

}

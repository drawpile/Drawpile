// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMAGESIZECHECK_H
#define IMAGESIZECHECK_H

class QSize;
class QImage;
class QColor;

#include <QVector>
#include <QPair>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size);

/**
 * @brief Check if we support writing an image file with this format
 *
 * The the format is identified from the filename suffix.
 * Note: only formats supported by the Rustpile library are included
 * in the list.
 */
bool isWritableFormat(const QString &filename);

enum FileFormatOption {
	Images = 0x01,
	Recordings = 0x02,
	AllFiles = 0x04,
	Save = 0x08,
	QtImagesOnly = 0x10,  // return images supported by Qt, rather than Rustpile
	Profile = 0x20,
	DebugDumps = 0x40,
	EventLog = 0x80,
	Gif = 0x100,

#ifdef Q_OS_ANDROID
	SaveAllFiles = 0x0,
#else
	SaveAllFiles = AllFiles,
#endif

	OpenImages = Images | AllFiles,
	OpenEverything = Images | Recordings | AllFiles,
	OpenDebugDumps = DebugDumps,
	SaveImages = Images | SaveAllFiles | Save,
	SaveRecordings = Recordings | SaveAllFiles | Save,
	SaveProfile = Profile | Save,
	SaveEventLog = EventLog | Save,
	SaveGif = Gif | Save,
};
Q_DECLARE_FLAGS(FileFormatOptions, FileFormatOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormatOptions)

QStringList fileFormatFilterList(FileFormatOptions formats);

//! Get a filter string to use in an Open or Save dialog
QString fileFormatFilter(FileFormatOptions formats);

}

#endif


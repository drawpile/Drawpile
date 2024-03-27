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
	Mp4 = 0x200,
	Webm = 0x400,
	Text = 0x800,
	BrushPack = 0x1000,
	SessionBans = 0x2000,
	AuthList = 0x4000,

#if defined(Q_OS_ANDROID) | defined(__EMSCRIPTEN__)
	SaveAllFiles = 0x0,
#else
	SaveAllFiles = AllFiles,
#endif

	OpenImages = Images | AllFiles,
	OpenEverything = Images | Recordings | AllFiles,
	OpenDebugDumps = DebugDumps,
	OpenBrushPack = BrushPack,
	OpenSessionBans = SessionBans,
	SaveImages = Images | SaveAllFiles | Save,
	SaveRecordings = Recordings | SaveAllFiles | Save,
	SaveProfile = Profile | Save,
	SaveEventLog = EventLog | Save,
	SaveText = Text | Save,
	SaveGif = Gif | Save,
	SaveMp4 = Mp4 | Save,
	SaveWebm = Webm | Save,
	SaveBrushPack = BrushPack | Save,
	SaveSessionBans = SessionBans | Save,
	SaveAuthList = AuthList | Save,
};
Q_DECLARE_FLAGS(FileFormatOptions, FileFormatOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormatOptions)

QStringList fileFormatFilterList(FileFormatOptions formats);

}

#endif


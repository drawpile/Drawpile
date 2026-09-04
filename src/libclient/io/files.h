// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_IO_FILES_H
#define LIBCLIENT_IO_FILES_H
#include <QByteArray>
#include <QString>

class QFileDevice;

namespace io {

bool looksLikeCanvasReplacingSuffix(const QString &suffix);

bool slurp(const QString &path, QByteArray &outBytes, QString &outError);

bool copyFile(
	const QString &sourcePath, const QString &targetPath, QString &outError);

bool copySaveFile(
	const QString &sourcePath, const QString &targetPath, QString &outError);

bool copyFileContents(
	QFileDevice &sourceFile, QFileDevice &targetFile, QString &outError);

}

#endif

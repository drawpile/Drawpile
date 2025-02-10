// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATHS_H
#define PATHS_H

#include <QStandardPaths>

namespace utils {
namespace paths {

//! Override the default read-only data paths
void setDataPath(const QString &datapath);

/**
 * @brief Override the writable data path
 *
 * If you're overriding both read-only and writable data paths,
 * setDataPath must be called first.
 */
void setWritablePath(const QString &datapath);

/**
 * @brief Get a list of paths in which datafiles may reside
 */
QStringList dataPaths();

/**
 * @brief Locate a file in dataPaths
 *
 * Returns the first file found or an empty string if not.
 */
QString locateDataFile(const QString &filename);

/**
 * @brief Get the full path to a writable directory or file.
 *
 * If both dirOrFilename and filename are given, the dirOrFilename should contain a directory component.
 * That directory is created first, if it doesn't exist already.
 */
QString writablePath(QStandardPaths::StandardLocation location, const QString &dirOrFileName, const QString &filename=QString());

inline QString writablePath(const QString &dirOrFileName, const QString &filename=QString())
{
	return writablePath(QStandardPaths::AppDataLocation, dirOrFileName, filename);
}

/**
 * @brief Gets the name of the file without any directories.
 *
 * On normal platforms, this is just the basename of the file. On Android, the
 * basename is extracted from the content URI that it uses. The returned path
 * could conceivably be empty, consider checking for it.
 */
QString extractBasename(QString filename);

bool looksLikeCanvasReplacingSuffix(const QString &suffix);

bool slurp(const QString &path, QByteArray &outBytes, QString &outError);

}
}

#endif

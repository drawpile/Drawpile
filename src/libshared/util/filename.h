// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UTIL_FILENAME_H
#define UTIL_FILENAME_H

class QString;
class QDir;

namespace utils {

/**
 * @brief Generate a filename that is unique
 *
 * If the file name+extension exist in the directory, a name in
 * format "<name> (n).<extension>" will be generated.
 *
 * @param dir the directory for the file
 * @param name file base name
 * @param extension file extension
 * @param fullpath return the full path
 * @return full path to a file that does not exist yet
 */
QString uniqueFilename(const QDir &dir, const QString &name, const QString &extension, bool fullpath=true);

/**
 * @brief Convert an existing filename into a unique one (if it isn't already)
 * @param path
 * @param defaultExtension (note: should start with a .)
 * @return
 */
QString makeFilenameUnique(const QString &path, const QString &defaultExtension);
}

#endif

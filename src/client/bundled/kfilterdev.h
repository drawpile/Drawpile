/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef __kfilterdev_h
#define __kfilterdev_h

#include "kcompressiondevice.h"
#include <QtCore/QString>

class QFile;
class KFilterBase;

/**
 * A class for reading and writing compressed data onto a device
 * (e.g. file, but other usages are possible, like a buffer or a socket).
 *
 * To simply read/write compressed files, see deviceForFile.
 *
 * KFilterDev adds MIME type support to KCompressionDevice, and also
 * provides compatibility methods for KDE 4 code.
 *
 * @author David Faure <faure@kde.org>
 */
class KFilterDev : public KCompressionDevice
{
public:
    /**
     * @since 5.0
     * Constructs a KFilterDev for a given FileName.
     * @param fileName the name of the file to filter.
     */
    KFilterDev(const QString &fileName);

    /**
     * Returns the compression type for the given mimetype, if possible. Otherwise returns None.
     * This handles simple cases like application/x-gzip, but also application/x-compressed-tar, and inheritance.
     */
    static CompressionType compressionTypeForMimeType(const QString &mimetype);
};

#endif

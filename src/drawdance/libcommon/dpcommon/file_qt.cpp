// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "file_qt.h"
#include "common.h"
}
#include <QFileInfo>
#include <QString>


extern "C" bool DP_qfile_exists(const char *path)
{
    return QFileInfo::exists(QString::fromUtf8(path));
}

extern "C" bool DP_qfile_remove(const char *path)
{
    return QFile::remove(QString::fromUtf8(path));
}

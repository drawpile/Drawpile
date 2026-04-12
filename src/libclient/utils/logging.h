// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILE_LOGGING_H
#define DRAWPILE_LOGGING_H
#include <QFileInfoList>
#include <QString>

namespace utils {

QString logFilePath();

void enableLogFile(bool enable);

QFileInfoList allLogFilesExceptCurrent();

}

#endif

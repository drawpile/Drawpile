// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWPILE_LOGGING_H
#define DRAWPILE_LOGGING_H

#include <cstdint>

class QByteArray;

namespace utils {

QByteArray logFilePath();

void enableLogFile(bool enable);

void logMessage(int level, const char *file, uint32_t line, const char *msg);

}

#endif

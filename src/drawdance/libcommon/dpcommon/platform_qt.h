// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPCOMMON_PLATFORM_H
#define DPCOMMON_PLATFORM_H
#include <QIODevice>

// On Android, files don't get truncated when writing without the truncate flag.
static constexpr QIODevice::OpenMode DP_QT_WRITE_FLAGS =
#ifdef Q_OS_ANDROID
    QIODevice::WriteOnly | QIODevice::Truncate;
#else
    QIODevice::WriteOnly;
#endif

#endif

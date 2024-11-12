// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPCOMMON_PLATFORM_H
#define DPCOMMON_PLATFORM_H
#include <QIODevice>

// On Unix systems, Qt messes up the locale on startup.
#ifdef Q_OS_UNIX
#    include <locale.h>
#    define DP_QT_LOCALE_RESET()    \
        do {                        \
            setlocale(LC_ALL, "C"); \
        } while (0)
#else
#    define DP_QT_LOCALE_RESET() \
        do {                     \
            /* nothing */        \
        } while (0)
#endif

// On Android, files don't get truncated when writing without the truncate flag.
static constexpr QIODevice::OpenMode DP_QT_WRITE_FLAGS =
#ifdef Q_OS_ANDROID
    QIODevice::WriteOnly | QIODevice::Truncate;
#else
    QIODevice::WriteOnly;
#endif

#endif

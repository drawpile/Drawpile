// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_STRINGS_H
#define LIBCLIENT_UTILS_STRINGS_H
#include <QString>

namespace strings {

QString percent();
QString px();

// Gives a reasonable format for a project file, either in MiB or GiB (presented
// without the overly correct i), avoiding smaller units because at best it's
// useless to know just how tiny a tiny file is. Worse, users may not know the
// more unusual smaller units or misread them, thinking that they're huge.
QString formatFileSize(qint64 sizeInBytes);

}

#endif

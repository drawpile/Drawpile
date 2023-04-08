// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IDENTICON_H
#define IDENTICON_H

#include <QImage>

/**
 * Generate a (repeatable) pseudorandom avatar icon from the given string
 */
QImage make_identicon(const QString &name, const QSize &size=QSize(32,32));

#endif



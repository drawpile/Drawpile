// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_PIXELS_H
#define LIBCLIENT_DRAWDANCE_PIXELS_H
#include <QColor>

struct DP_UPixelFloat;

namespace drawdance {

bool toUPixelFloat(DP_UPixelFloat &r, const QColor &q);
QColor fromUPixelFloat(const DP_UPixelFloat &color);

}

#endif

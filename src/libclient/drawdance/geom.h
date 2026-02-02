// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_GEOM_H
#define LIBCLIENT_DRAWDANCE_GEOM_H

class QRect;
struct DP_Rect;

namespace drawdance {

bool rectDpToQt(const DP_Rect *rectOrNull, QRect &outRect);

bool rectQtToDp(const QRect &rect, DP_Rect &outRect);

}

#endif

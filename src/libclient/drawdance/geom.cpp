// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
}
#include "libclient/drawdance/geom.h"
#include <QRect>

namespace drawdance {

bool rectDpToQt(const DP_Rect *rectOrNull, QRect &outRect)
{
	if(rectOrNull) {
		DP_Rect rect = *rectOrNull;
		if(DP_rect_valid(rect)) {
			outRect = QRect(QPoint(rect.x1, rect.y1), QPoint(rect.x2, rect.y2));
			return true;
		}
	}
	outRect = QRect();
	return false;
}

bool rectQtToDp(const QRect &rect, DP_Rect &outRect)
{
	if(rect.isValid()) {
		outRect.x1 = rect.left();
		outRect.y1 = rect.top();
		outRect.x2 = rect.right();
		outRect.y2 = rect.bottom();
		return true;
	}
	outRect = DP_rect_make_invalid();
	return false;
}

}

// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/pixels.h>
}
#include "libclient/drawdance/pixels.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

bool toUPixelFloat(DP_UPixelFloat &r, const QColor &q)
{
	if(q.isValid()) {
		r = {
			compat::cast<float>(q.blueF()),
			compat::cast<float>(q.greenF()),
			compat::cast<float>(q.redF()),
			compat::cast<float>(q.alphaF()),
		};
		return true;
	} else {
		return false;
	}
}

QColor fromUPixelFloat(const DP_UPixelFloat &color)
{
	return QColor::fromRgbF(color.r, color.g, color.b, color.a);
}

}

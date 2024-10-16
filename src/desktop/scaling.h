// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCALING_H
#define DESKTOP_SCALING_H
#include <Qt>

class QScreen;
class QWidget;

namespace scaling {

void initScaling(qreal initialOverrideFactor);

qreal getScreenScale(const QScreen *screen);

qreal getOverrideFactor(); // Returns 0.0 if no override factor is set.
void setOverrideFactor(qreal newOverrideFactor);

qreal getWidgetScale(QWidget *widget);

}

#endif

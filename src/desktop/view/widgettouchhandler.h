// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_TOUCHHANDLER_H
#define DESKTOP_UTILS_TOUCHHANDLER_H
#include "libclient/view/touchhandler.h"

class QGestureEvent;

namespace view {

// QGestureEvent is only in QtWidgets, hence this extension.
class WidgetTouchHandler final : public TouchHandler {
	Q_OBJECT
public:
	WidgetTouchHandler(QObject *parent);

	void handleGesture(QGestureEvent *event, qreal zoom, qreal rotation);

private:
	QPointF m_gestureStartPos;
	qreal m_gestureStartZoom = 0.0;
	qreal m_gestureStartRotation = 0.0;
};

}

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_TOUCHHANDLER_H
#define DESKTOP_UTILS_TOUCHHANDLER_H
#include <QObject>
#include <QPointF>

class QGestureEvent;
class QTouchEvent;

class TouchHandler : public QObject {
	Q_OBJECT
public:
	TouchHandler(QObject *parent);

	void onTabletEventReceived() { m_anyTabletEventsReceived = true; }

	bool isTouchDrawEnabled() const;
	bool isTouchPanEnabled() const;
	bool isTouchDrawOrPanEnabled() const;
	bool isTouchPinchEnabled() const { return m_enableTouchPinch; }
	bool isTouchTwistEnabled() const { return m_enableTouchTwist; }

	bool isTouching() const { return m_touching; }

	void handleTouchBegin(QTouchEvent *event);
	void handleTouchUpdate(
		QTouchEvent *event, qreal zoom, qreal rotation, qreal dpr);
	void handleTouchEnd(QTouchEvent *event, bool cancel);

	void handleGesture(QGestureEvent *event, qreal zoom, qreal rotation);

signals:
	void touchPressed(QEvent *event, long long timeMsec, const QPointF &posf);
	void touchMoved(long long timeMsec, const QPointF &posf);
	void touchReleased(long long timeMsec, const QPointF &posf);
	void touchScrolledBy(qreal dx, qreal dy);
	void touchZoomedRotated(qreal zoom, qreal rotation);

private:
	static constexpr qreal TOUCH_DRAW_DISTANCE = 10.0;
	static constexpr int TOUCH_DRAW_BUFFER_COUNT = 20;

	enum class TouchMode { Unknown, Drawing, Moving };

	static qreal squareDist(const QPointF &p)
	{
		return p.x() * p.x() + p.y() * p.y();
	}

	void setOneFingerTouchAction(int oneFingerTouchAction);
	void setEnableTouchPinch(bool enableTouchPinch);
	void setEnableTouchTwist(bool enableTouchTwist);

	void flushTouchDrawBuffer();

	bool m_enableTouchPinch = true;
	bool m_enableTouchTwist = true;
	bool m_touching = false;
	bool m_touchRotating = false;
	bool m_anyTabletEventsReceived = false;
	int m_oneFingerTouchAction;
	TouchMode m_touchMode = TouchMode::Unknown;
	QVector<QPair<long long, QPointF>> m_touchDrawBuffer;
	QPointF m_touchStartPos;
	qreal m_touchStartZoom = 0.0;
	qreal m_touchStartRotate = 0.0;
	QPointF m_gestureStartPos;
	qreal m_gestureStartZoom = 0.0;
	qreal m_gestureStartRotation = 0.0;
};

#endif

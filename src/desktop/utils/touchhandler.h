// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_TOUCHHANDLER_H
#define DESKTOP_UTILS_TOUCHHANDLER_H
#include <QDeadlineTimer>
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
	bool isTouchPinchEnabled() const;
	bool isTouchTwistEnabled() const;
	bool isTouchPinchOrTwistEnabled() const;

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
	void touchTapActionActivated(int action);

private:
	static constexpr qreal TAP_SLOP_SQUARED = 16.0 * 16.0;
	static constexpr int TAP_MAX_DELAY_MS = 1000;
	static constexpr int DRAW_BUFFER_COUNT = 20;

	enum class TouchMode { Unknown, Drawing, Moving };

	static qreal squareDist(const QPointF &p)
	{
		return p.x() * p.x() + p.y() * p.y();
	}

	void setOneFingerTouchAction(int oneFingerTouchAction);
	void setTwoFingerPinchAction(int twoFingerPinchAction);
	void setTwoFingerTwistAction(int twoFingerTwistAction);
	void setOneFingerTapAction(int oneFingerTapAction);
	void setTwoFingerTapAction(int twoFingerTapAction);
	void setThreeFingerTapAction(int threeFingerTapAction);
	void setFourFingerTapAction(int fourFingerTapAction);

	qreal adjustTwistRotation(qreal degrees) const;
	void flushTouchDrawBuffer();
	void emitTapAction(int action);

	bool m_touching = false;
	bool m_touchDragging = false;
	bool m_touchRotating = false;
	bool m_anyTabletEventsReceived = false;
	int m_oneFingerTouchAction;
	int m_twoFingerPinchAction;
	int m_twoFingerTwistAction;
	int m_oneFingerTapAction;
	int m_twoFingerTapAction;
	int m_threeFingerTapAction;
	int m_fourFingerTapAction;
	int m_maxTouchPoints = 0;
	TouchMode m_touchMode = TouchMode::Unknown;
	QVector<QPair<long long, QPointF>> m_touchDrawBuffer;
	QPointF m_touchStartPos;
	qreal m_touchStartZoom = 0.0;
	qreal m_touchStartRotate = 0.0;
	QPointF m_gestureStartPos;
	qreal m_gestureStartZoom = 0.0;
	qreal m_gestureStartRotation = 0.0;
	QDeadlineTimer m_tapTimer;
};

#endif

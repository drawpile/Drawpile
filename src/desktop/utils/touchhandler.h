// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_TOUCHHANDLER_H
#define DESKTOP_UTILS_TOUCHHANDLER_H
#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QPointF>

class QGestureEvent;
class QTimer;
class QTouchEvent;

class TouchHandler : public QObject {
	Q_OBJECT
public:
	TouchHandler(QObject *parent);

	void onTabletEventReceived() { m_anyTabletEventsReceived = true; }

	void setAllowColorPick(bool allowColorPick)
	{
		m_allowColorPick = allowColorPick;
	}

	bool isTouchDrawEnabled() const;
	bool isTouchDrawPressureEnabled() const;
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
	void touchPressed(
		QEvent *event, long long timeMsec, const QPointF &posf, qreal pressure);
	void touchMoved(long long timeMsec, const QPointF &posf, qreal pressure);
	void touchReleased(long long timeMsec, const QPointF &posf);
	void touchScrolledBy(qreal dx, qreal dy);
	void touchZoomedRotated(qreal zoom, qreal rotation);
	void touchTapActionActivated(int action);
	void touchColorPicked(const QPointF &posf);
	void touchColorPickFinished();

private:
	static constexpr qreal TAP_SLOP_SQUARED = 16.0 * 16.0;
	static constexpr int TAP_MAX_DELAY_MS = 1000;
	static constexpr int TAP_AND_HOLD_DELAY_MS = 400;
	static constexpr int DRAW_BUFFER_COUNT = 20;
	// On Android with UI scaling set to something other than 100%, the start
	// values jitter by very large amounts. It goes up to around 1.1 at most
	// from testing no matter the scaling, so 1.5 should hopefully be a safe
	// value. Since the point of this is to detect a user lifting their finger
	// and placing them in a different spot, this delta shouldn't cause issues.
	static constexpr qreal TOUCH_START_SLOP = 1.5;

	enum class TouchMode { Unknown, Drawing, Moving };

	struct TouchSpot {
		enum { Initial, Active, Gone } state = Initial;
		QPointF current;
		QPointF previous;
		QPointF anchor;
		QPointF first;
	};

	class TouchState {
	public:
		void clear();
		void begin(QTouchEvent *event);
		void update(QTouchEvent *event);

		bool isEmpty() const { return m_spotsById.isEmpty(); }
		bool isSingleTouch() const { return m_spotsById.size() == 1; }
		bool isMultiTouch() const { return m_spotsById.size() > 1; }
		bool isSpotsChanged() const { return m_spotsChanged; }
		int maxTouchPoints() const { return m_maxTouchPoints; }
		const QList<TouchSpot> &spots() { return m_spots; }
		const QPointF &currentCenter() const { return m_currentCenter; }
		const QPointF &previousCenter();
		const QPointF &anchorCenter();

	private:
		QHash<int, TouchSpot> m_spotsById;
		QList<TouchSpot> m_spots;
		QPointF m_currentCenter;
		QPointF m_previousCenter;
		QPointF m_anchorCenter;
		int m_maxTouchPoints = 0;
		bool m_previousCenterValid = true;
		bool m_anchorCenterValid = true;
		bool m_spotsChanged = true;
	};

	struct TouchDrawPoint {
		long long timeMsec;
		QPointF posf;
		qreal pressure;
	};

	qreal touchPressure(const QTouchEvent *event) const;

	static qreal squareDist(const QPointF &p)
	{
		return p.x() * p.x() + p.y() * p.y();
	}

	void setTouchDrawPressureEnabled(bool touchDrawPressureEnabled);
	void setOneFingerTouchAction(int oneFingerTouchAction);
	void setTwoFingerPinchAction(int twoFingerPinchAction);
	void setTwoFingerTwistAction(int twoFingerTwistAction);
	void setOneFingerTapAction(int oneFingerTapAction);
	void setTwoFingerTapAction(int twoFingerTapAction);
	void setThreeFingerTapAction(int threeFingerTapAction);
	void setFourFingerTapAction(int fourFingerTapAction);
	void setOneFingerTapAndHoldAction(int oneFingerTapAndHoldAction);
	void setSmoothing(int smoothing);
	void scrollBy(qreal dx, qreal dy);
	void startZoomRotate(qreal zoom, qreal rotation);
	void zoomRotate(qreal zoom, qreal rotation);
	void updateSmoothedMotion();

	void triggerTapAndHold();

	qreal adjustTwistRotation(qreal degrees) const;
	void flushTouchDrawBuffer();
	void emitTapAction(int action);

	bool m_touching = false;
	bool m_touchDragging = false;
	bool m_touchStartsValid = false;
	bool m_touchRotating = false;
	bool m_touchHeld = false;
	bool m_anyTabletEventsReceived = false;
	bool m_allowColorPick = true;
	bool m_touchDrawPressureEnabled = false;
	int m_oneFingerTouchAction;
	int m_twoFingerPinchAction;
	int m_twoFingerTwistAction;
	int m_oneFingerTapAction;
	int m_twoFingerTapAction;
	int m_threeFingerTapAction;
	int m_fourFingerTapAction;
	int m_oneFingerTapAndHoldAction;
	TouchMode m_touchMode = TouchMode::Unknown;
	QVector<TouchDrawPoint> m_touchDrawBuffer;
	TouchState m_touchState;
	qreal m_touchStartZoom = 0.0;
	qreal m_touchStartRotate = 0.0;
	QPointF m_gestureStartPos;
	qreal m_gestureStartZoom = 0.0;
	qreal m_gestureStartRotation = 0.0;
	qreal m_smoothDx = 0.0;
	qreal m_smoothDy = 0.0;
	qreal m_smoothZoomCurrent = 0.0;
	qreal m_smoothZoomTarget = 0.0;
	qreal m_smoothRotationCurrent = 0.0;
	qreal m_smoothRotationTarget = 0.0;
	qreal m_smoothMultiplier = 0.5;
	QDeadlineTimer m_tapTimer;
	QTimer *m_tapAndHoldTimer;
	QTimer *m_smoothTimer = nullptr;
};

#endif

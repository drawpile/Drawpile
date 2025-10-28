// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_LONGPRESSEVENTFILTER_H
#define DESKTOP_UTILS_LONGPRESSEVENTFILTER_H
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QWidget>

class QMouseEvent;
class QTimer;

class LongPressEventFilter : public QObject {
	Q_OBJECT
public:
	static constexpr char ENABLED_PROPERTY[] = "DRAWPILE_LONG_PRESS";

	explicit LongPressEventFilter(QObject *parent = nullptr);

	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	static constexpr int MINIMUM_DELAY = 100;
	static constexpr int MINIMUM_DISTANCE = 0;

	bool handleMousePress(QWidget *target, const QMouseEvent *me);
	bool handleMouseMove(const QMouseEvent *me);
	void flush();
	void cancel();

	bool isWithinDistance(const QPoint &globalPos) const;

	void triggerLongPress();

	void setKineticScrollGesture(int kineticScrollGesture);
	int getKineticScrollDelay(QWidget *target) const;

	static bool isContextMenuTarget(QWidget *target);
	static bool isLongPressableWidget(QWidget *target);

	QTimer *m_timer;
	long long m_distanceSquared = 0LL;
	QPoint m_pressLocalPos;
	QPoint m_pressGlobalPos;
	QPointer<QWidget> m_target;
	int m_kineticScrollGesture;
#ifdef Q_OS_ANDROID
	int m_longPressTimeout;
#endif
	bool m_handlingEvent = false;
};

#endif

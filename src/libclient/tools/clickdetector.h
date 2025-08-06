// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_CLICKDETECTOR_H
#define LIBCLIENT_TOOLS_CLICKDETECTOR_H
#include "libclient/tools/enums.h"
#include <QDeadlineTimer>
#include <QGuiApplication>
#include <QLineF>
#include <QPointF>
#include <QStyleHints>

namespace tools {

class DragDetector;

class ClickDetector final {
	friend class DragDetector;

public:
	void begin(const QPointF &viewPos, DeviceType deviceType)
	{
		m_clickDistance = getClickDistance(deviceType);
		if(m_timer.hasExpired() ||
		   QLineF(m_endViewPos, viewPos).length() > m_clickDistance) {
			m_clicks = 0;
		}
		m_timer.setRemainingTime(CLICK_TIME);
		m_startViewPos = viewPos;
		m_endViewPos = viewPos;
	}

	void motion(const QPointF &viewPos) { m_endViewPos = viewPos; }

	void end()
	{
		if(!m_timer.hasExpired() &&
		   QLineF(m_startViewPos, m_endViewPos).length() <= m_clickDistance) {
			++m_clicks;
			m_timer.setRemainingTime(getDoubleClickTime());
		} else {
			clear();
		}
	}

	void clear()
	{
		m_timer.setRemainingTime(0);
		m_clicks = 0;
	}

	bool isClick() const { return m_clicks != 0; }
	int clicks() const { return m_clicks; }

private:
	static constexpr int CLICK_TIME = 200;
	static constexpr int CLICK_DISTANCE = 100;

	static int getDoubleClickTime()
	{
		return qMax(
			CLICK_TIME,
			QGuiApplication::styleHints()->mouseDoubleClickInterval());
	}

	static qreal getClickDistance(DeviceType deviceType)
	{
		int distance;
		switch(deviceType) {
		case DeviceType::Mouse:
			// Mice are pretty stable, so they don't need as much fudge.
			distance = CLICK_DISTANCE / 2;
			break;
		default:
			distance = CLICK_DISTANCE;
			break;
		}
		return qreal(qMax(
			distance,
			QGuiApplication::styleHints()->mouseDoubleClickDistance()));
	}

	QDeadlineTimer m_timer;
	qreal m_clickDistance = 0.0;
	QPointF m_startViewPos;
	QPointF m_endViewPos;
	unsigned int m_clicks = 0;
};

class DragDetector final {
public:
	void begin(const QPointF &viewPos, DeviceType deviceType)
	{
		m_clickDistance = ClickDetector::getClickDistance(deviceType);
		m_timer.setRemainingTime(ClickDetector::CLICK_TIME);
		m_startViewPos = viewPos;
		m_dragThresholdHit = false;
	}

	void motion(const QPointF &viewPos)
	{
		if(!m_dragThresholdHit && !m_timer.hasExpired()) {
			if(QLineF(m_startViewPos, viewPos).length() > m_clickDistance) {
				m_dragThresholdHit = true;
			}
		}
	}

	bool isDrag() const { return m_dragThresholdHit || m_timer.hasExpired(); }

private:
	QDeadlineTimer m_timer;
	qreal m_clickDistance = 0.0;
	QPointF m_startViewPos;
	bool m_dragThresholdHit = false;
};

}

#endif

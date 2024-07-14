// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_CLICKDETECTOR_H
#define LIBCLIENT_TOOLS_CLICKDETECTOR_H
#include "libclient/tools/devicetype.h"
#include <QDeadlineTimer>
#include <QGuiApplication>
#include <QLineF>
#include <QPointF>
#include <QStyleHints>

namespace tools {

class ClickDetector final {
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

	bool isClick() { return m_clicks != 0; }
	int clicks() { return m_clicks; }

private:
	static constexpr int CLICK_TIME = 150;

	static int getDoubleClickTime()
	{
		return qMax(
			CLICK_TIME,
			QGuiApplication::styleHints()->mouseDoubleClickInterval());
	}

	qreal getClickDistance(DeviceType deviceType)
	{
		int distance;
		switch(deviceType) {
		case DeviceType::Mouse:
			distance = 2;
			break;
		case DeviceType::Tablet:
		case DeviceType::Touch:
			distance = 20;
			break;
		default:
			qWarning("Unknown device type %d", int(deviceType));
			distance = 0;
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

}

#endif

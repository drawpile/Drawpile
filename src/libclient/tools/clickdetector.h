// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_CLICKDETECTOR_H
#define LIBCLIENT_TOOLS_CLICKDETECTOR_H
#include "libclient/tools/devicetype.h"
#include <QDeadlineTimer>
#include <QLineF>
#include <QPointF>

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
			m_timer.setRemainingTime(CLICK_TIME);
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

	qreal getClickDistance(DeviceType deviceType)
	{
		switch(deviceType) {
		case DeviceType::Mouse:
			return 2.0;
		case DeviceType::Tablet:
		case DeviceType::Touch:
			return 20.0;
		}
		qWarning("Unknown device type %d", int(deviceType));
		return 0.0;
	}

	QDeadlineTimer m_timer;
	qreal m_clickDistance = 0.0;
	QPointF m_startViewPos;
	QPointF m_endViewPos;
	unsigned int m_clicks = 0;
};

}

#endif

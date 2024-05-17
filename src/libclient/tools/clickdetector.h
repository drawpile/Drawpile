// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_CLICKDETECTOR_H
#define LIBCLIENT_TOOLS_CLICKDETECTOR_H
#include <QDeadlineTimer>
#include <QLineF>
#include <QPointF>

namespace tools {

class ClickDetector final {
public:
	static constexpr int CLICK_TIME = 150;
	static constexpr qreal CLICK_DISTANCE = 50;

	void begin(const QPointF &viewPos)
	{
		if(m_timer.hasExpired() ||
		   QLineF(m_endViewPos, viewPos).length() > CLICK_DISTANCE) {
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
		   QLineF(m_startViewPos, m_endViewPos).length() <= CLICK_DISTANCE) {
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
	QDeadlineTimer m_timer;
	QPointF m_startViewPos;
	QPointF m_endViewPos;
	unsigned int m_clicks = 0;
};

}

#endif

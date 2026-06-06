#ifndef DESKTOP_UTILS_TABLETFILTER_H
#define DESKTOP_UTILS_TABLETFILTER_H
#include <QTabletEvent>
#ifdef Q_OS_WIN
#	include "desktop/tabletinput.h"
#endif

class TabletFilter {
public:
	void setIgnoreBlotches(bool ignoreBlotches)
	{
		if(ignoreBlotches != m_ignoreBlotches) {
			m_ignoreBlotches = ignoreBlotches;
			m_pressure = 0.0;
		}
	}

	void reset()
	{
		m_pressure = 0.0;
#ifdef Q_OS_WIN
		m_ignoreSpontaneous = false;
#endif
	}

	bool shouldIgnore(const QTabletEvent *event)
	{
#ifdef Q_OS_WIN
		bool spontaneous = event->spontaneous();
		if(m_ignoreSpontaneous) {
			return spontaneous;
		} else {
			if(!spontaneous && tabletinput::ignoreSpontaneous()) {
				m_ignoreSpontaneous = true;
			}
			return false;
		}
#else
		if(m_ignoreBlotches) {
			if(event->type() == QEvent::TabletRelease) {
				m_pressure = 0.0;
			} else {
				qreal lastPressure = m_pressure;
				m_pressure = event->pressure();
				// This hack is relevant on tablets that have only a really weak
				// pressure response, where a person physically can't press hard
				// enough to reach over 50% anyway. So this should be safe.
				if(m_pressure == 1.0 && lastPressure < 0.5) {
					return true;
				}
			}
		}
		return false;
#endif
	}

private:
	qreal m_pressure = 0.0;
	bool m_ignoreBlotches = false;
#ifdef Q_OS_WIN
	bool m_ignoreSpontaneous = false;
#endif
};

#endif

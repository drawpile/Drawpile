#ifndef DESKTOP_UTILS_TABLETFILTER_H
#define DESKTOP_UTILS_TABLETFILTER_H
#include "desktop/tabletinput.h"
#include <QTabletEvent>

class TabletFilter {
public:
	TABLETINPUT_CONSTEXPR_OR_INLINE void reset()
	{
#ifdef Q_OS_WIN
		m_ignoreSpontaneous = false;
#endif
	}

	TABLETINPUT_CONSTEXPR_OR_INLINE bool shouldIgnore(const QTabletEvent *event)
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
		Q_UNUSED(event);
		return false;
#endif
	}

#ifdef Q_OS_WIN
private:
	bool m_ignoreSpontaneous = false;
#endif
};

#endif

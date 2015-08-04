#include "eventfixfilter.h"
#include "tabletstate.h"

#include <QEvent>
#include <QQuickView>
#include <QWidget>
#include <QApplication>
#include <QDebug>

EventFixFilter::EventFixFilter(TabletState *ts, QWidget *container)
	: QObject(container), m_tabletstate(ts), m_container(container)
{
	Q_ASSERT(ts);
}

bool EventFixFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	// Work around QtQuick's lack of tablet events.
	// This workaround also fixes the jittery position data real QTabletEvents
	// sometimes deliver.
	case QEvent::TabletPress:
	case QEvent::TabletMove:
			m_tabletstate->setLastPressure(static_cast<QTabletEvent*>(event)->pressure());
			m_tabletstate->setStylusDown(true);
			break;
	case QEvent::TabletRelease:
			m_tabletstate->setStylusDown(false);
			break;


	default: break;
	}

	return false;
}

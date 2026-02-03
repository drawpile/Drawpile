// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/clickeventfilter.h"
#include <QMouseEvent>

ClickEventFilter::ClickEventFilter(QObject *parent)
	: QObject(parent)
{
}

bool ClickEventFilter::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type()) {
	case QEvent::Enter:
	case QEvent::Leave:
		m_pressed = false;
		break;
	case QEvent::MouseButtonPress:
		if(static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			m_pressed = true;
		}
		return true;
	case QEvent::MouseButtonRelease:
		if(m_pressed &&
		   static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			m_pressed = false;
			Q_EMIT clicked();
		}
		return true;
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

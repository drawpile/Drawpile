// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/viewstatusbar.h"
#include <QScopedValueRollback>

namespace widgets {

ViewStatusBar::ViewStatusBar(QWidget *parent)
	: QStatusBar{parent}
{
	connect(
		this, &QStatusBar::messageChanged, this, &ViewStatusBar::updateMessage);
	showCoordinatesMessage();
}

void ViewStatusBar::setSessionHistorySize(int sessionHistorySize)
{
	m_sessionHistorySize = sessionHistorySize;
	scheduleCoordinatesMessage();
}

void ViewStatusBar::setLatency(qint64 latency)
{
	m_latency = latency;
	scheduleCoordinatesMessage();
}

void ViewStatusBar::setCoordinates(const QPointF &coordinates)
{
	m_coordinates = coordinates;
	scheduleCoordinatesMessage();
}

void ViewStatusBar::timerEvent(QTimerEvent *event)
{
	Q_UNUSED(event);
	if(m_timerId != 0) {
		showCoordinatesMessage();
		if(m_shouldStopTimer) {
			resetMessageTimer();
		} else {
			m_shouldStopTimer = true;
		}
	}
}

void ViewStatusBar::updateMessage(const QString &message)
{
	resetMessageTimer();
	m_showStatusMessage = m_settingStatusMessage || message.isEmpty();
	showCoordinatesMessage();
}

void ViewStatusBar::scheduleCoordinatesMessage()
{
	if(m_timerId == 0) {
		m_shouldStopTimer = true;
		m_timerId = startTimer(66, Qt::CoarseTimer);
	} else {
		m_shouldStopTimer = false;
	}
}

void ViewStatusBar::showCoordinatesMessage()
{
	if(m_showStatusMessage && !m_settingStatusMessage) {
		QScopedValueRollback<bool> guard{m_settingStatusMessage, true};
		int x = int(m_coordinates.x());
		int y = int(m_coordinates.y());
		if(m_sessionHistorySize < 0) {
			if(m_latency < 0) {
				showMessage(QStringLiteral("(%1, %2)").arg(x).arg(y));
			} else {
				showMessage(QStringLiteral("%1 ms | (%2, %3)")
								.arg(m_latency)
								.arg(x)
								.arg(y));
			}
		} else if(m_latency < 0) {
			showMessage(
				QStringLiteral("%1 MB | (%2, %3)")
					.arg(m_sessionHistorySize / double(1024 * 1024), 0, 'f', 2)
					.arg(x)
					.arg(y));
		} else {
			showMessage(
				QStringLiteral("%1 MB | %2 ms | (%3, %4)")
					.arg(m_sessionHistorySize / double(1024 * 1024), 0, 'f', 2)
					.arg(m_latency)
					.arg(x)
					.arg(y));
		}
	}
}

void ViewStatusBar::resetMessageTimer()
{
	if(m_timerId != 0) {
		killTimer(m_timerId);
		m_timerId = 0;
	}
}

}

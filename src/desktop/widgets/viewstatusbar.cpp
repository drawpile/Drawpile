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
}

void ViewStatusBar::setCoordinates(const QPointF &coordinates)
{
	m_coordinates = coordinates;
	showCoordinatesMessage();
}

void ViewStatusBar::updateMessage(const QString &message)
{
	m_showStatusMessage = m_settingStatusMessage || message.isEmpty();
	showCoordinatesMessage();
}

void ViewStatusBar::showCoordinatesMessage()
{
	if(m_showStatusMessage && !m_settingStatusMessage) {
		QScopedValueRollback<bool> guard{m_settingStatusMessage, true};
		int x = int(m_coordinates.x());
		int y = int(m_coordinates.y());
		if(m_sessionHistorySize < 0) {
			showMessage(QStringLiteral("(%1, %2)").arg(x).arg(y));
		} else {
			showMessage(
				QStringLiteral("%1 MB | (%2, %3)")
					.arg(m_sessionHistorySize / double(1024 * 1024), 0, 'f', 2)
					.arg(x)
					.arg(y));
		}
	}
}

}

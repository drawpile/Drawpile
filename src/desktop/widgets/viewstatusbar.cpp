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

void ViewStatusBar::setCoordinates(const QPointF &coordinates)
{
	m_coordinates = coordinates;
	showCoordinatesMessage();
}

void ViewStatusBar::updateMessage(const QString &message)
{
	m_showCoordinates = m_settingCoordinates || message.isEmpty();
	showCoordinatesMessage();
}

void ViewStatusBar::showCoordinatesMessage()
{
	if(m_showCoordinates && !m_settingCoordinates) {
		QScopedValueRollback<bool> guard{m_settingCoordinates, true};
		showMessage(QStringLiteral("(%1, %2)")
						.arg(int(m_coordinates.x()))
						.arg(int(m_coordinates.y())));
	}
}

}

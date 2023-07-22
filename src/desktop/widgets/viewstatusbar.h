// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_WIDGETS_VIEWSTATUSBAR_H
#define DESKTOP_WIDGETS_VIEWSTATUSBAR_H

#include <QPointF>
#include <QStatusBar>

namespace widgets {

class ViewStatusBar final : public QStatusBar {
	Q_OBJECT
public:
	ViewStatusBar(QWidget *parent = nullptr);

	void setSessionHistorySize(int sessionHistorySize);
	void setCoordinates(const QPointF &coordinates);

private slots:
	void updateMessage(const QString &message);

private:
	void showCoordinatesMessage();

	bool m_showStatusMessage = true;
	bool m_settingStatusMessage = false;
	int m_sessionHistorySize = -1;
	QPointF m_coordinates = {0, 0};
};

}

#endif

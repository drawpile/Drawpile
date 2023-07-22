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

	void setCoordinates(const QPointF &coordinates);

private slots:
	void updateMessage(const QString &message);

private:
	void showCoordinatesMessage();

	bool m_showCoordinates = true;
	bool m_settingCoordinates = false;
	QPointF m_coordinates = {0, 0};
};

}

#endif

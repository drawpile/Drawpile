/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef TITLEWIDGET_H
#define TITLEWIDGET_H

#include <QDockWidget>

class QBoxLayout;

namespace docks {

class TitleWidget : public QWidget
{
	Q_OBJECT
public:
	explicit TitleWidget(QDockWidget *parent = nullptr);

	void addCustomWidget(QWidget *widget, bool stretch=false);
	void addSpace(int space);
	void addStretch(int stretch=0);

	/// Add a spacer to the left side to center the custom widgets
	void addCenteringSpacer();

private slots:
	void toggleFloating();
	void toggleDockable();
	void showContextMenu(const QPoint &pos);
	void updateContextMenuActions();
	void onDockLocationChanged(Qt::DockWidgetArea area);
	void onFeaturesChanged(QDockWidget::DockWidgetFeatures features);

private:
	class Button;

	QBoxLayout *m_layout;
	Button *m_dockButton;
	Button *m_closeButton;
	QMenu *m_contextMenu;
	QAction *m_dockAction;
	QAction *m_dockableAction;
};

}

#endif // TITLEWIDGET_H

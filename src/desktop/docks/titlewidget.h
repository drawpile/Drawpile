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
class QMenu;
class QToolButton;

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

	void addGlobalDockActions(const QList<QAction *> &actions);

private slots:
	void toggleFloating();
	void toggleDockable();
	void updateContextMenuActions();
	void onDockLocationChanged(Qt::DockWidgetArea area);

private:
	QBoxLayout *m_layout;
	QToolButton *m_button;
	QMenu *m_menu;
	QAction *m_dockAction;
	QAction *m_dockableAction;
	QAction *m_closeAction;
};

}

#endif // TITLEWIDGET_H

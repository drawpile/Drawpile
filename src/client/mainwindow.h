/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "tools.h"

class QActionGroup;
namespace widgets {
	class NetStatus;
	class HostLabel;
	class EditorView;
	class DualColorButton;
	class ToolSettings;
	class ColorDialog;
}
namespace drawingboard {
	class Board;
}
class Controller;

class MainWindow : public QMainWindow {
	Q_OBJECT
	public:
		MainWindow();

	public slots:
		void save();
		void saveas();
		void zoomin();
		void zoomout();
		void zoomone();
		void selectTool(QAction *tool);

	signals:
		void toolChanged(tools::Type);

	protected:
		void closeEvent(QCloseEvent *event);

	private:
		void readSettings();
		void writeSettings();

		void initActions();
		void createMenus();
		void createToolbars();
		void createDocks();
		void createToolSettings(QMenu *menu);

		widgets::ToolSettings *toolsettings_;
		widgets::DualColorButton *fgbgcolor_;
		widgets::NetStatus *netstatus_;
		widgets::HostLabel *hostaddress_;
		widgets::EditorView *view_;
		widgets::ColorDialog *fgdialog_,*bgdialog_;
		drawingboard::Board *board_;
		Controller *controller_;

		QString filename_;
		QString lastpath_;

		QAction *save_;
		QAction *saveas_;
		QAction *quit_;

		QAction *host_;
		QAction *join_;
		QAction *logout_;

		QActionGroup *adminTools_;
		QAction *lockboard_;
		QAction *lockuser_;
		QAction *kickuser_;

		QActionGroup *drawingtools_;
		QAction *brushtool_;
		QAction *erasertool_;
		QAction *pickertool_;
		QAction *zoomin_;
		QAction *zoomout_;
		QAction *zoomorig_;

		QAction *toggleoutline_;
		QAction *togglecrosshair_;

		QAction *toolbartoggles_;
		QAction *docktoggles_;

		QAction *help_;
		QAction *about_;
};

#endif


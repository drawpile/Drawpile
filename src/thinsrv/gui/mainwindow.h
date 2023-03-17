/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QModelIndex;
class QScrollArea;

namespace server {
namespace gui {

class SidebarModel;
class Server;

class MainWindow final : public QMainWindow
{
	Q_OBJECT
public:
	explicit MainWindow(Server *serverConnection, QWidget *parent=nullptr);

	//! Set the server connection to use for the default mainwindow instance
	static void setDefaultInstanceServer(Server *serverConnection);

	static Server *defaultInstanceServer();

	//! Show the default MainWindow
	static void showDefaultInstance();

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	void onPageSelect(const QModelIndex &index);

private:
	Server *m_server;
	SidebarModel *m_model;
	QScrollArea *m_pageArea;
	QString m_currentPage;
};

}
}

#endif

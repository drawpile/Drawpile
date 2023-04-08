// SPDX-License-Identifier: GPL-3.0-or-later

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

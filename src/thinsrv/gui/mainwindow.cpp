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

#include "thinsrv/gui/mainwindow.h"
#include "thinsrv/gui/sidebarmodel.h"
#include "thinsrv/gui/sidebaritemdelegate.h"
#include "thinsrv/gui/localserver.h"
#include "thinsrv/gui/trayicon.h"

#include <QDebug>
#include <QAction>
#include <QToolBar>
#include <QSplitter>
#include <QTreeView>
#include <QScrollArea>
#include <QPointer>
#include <QTimer>
#include <QCloseEvent>

namespace server {
namespace gui {

static Server *DEFAULT_SERVER;
static QPointer<MainWindow> DEFAULT_INSTANCE;

void MainWindow::setDefaultInstanceServer(Server *serverConnection)
{
	Q_ASSERT(serverConnection);
	Q_ASSERT(DEFAULT_INSTANCE.isNull());
	DEFAULT_SERVER = serverConnection;
}

Server *MainWindow::defaultInstanceServer()
{
	return DEFAULT_SERVER;
}

void MainWindow::showDefaultInstance()
{
	Q_ASSERT(DEFAULT_SERVER);
	if(DEFAULT_INSTANCE.isNull())
		DEFAULT_INSTANCE = new MainWindow(DEFAULT_SERVER);
	DEFAULT_INSTANCE->show();
	DEFAULT_INSTANCE->raise();
}

MainWindow::MainWindow(Server *serverConnection, QWidget *parent)
	: QMainWindow(parent), m_server(serverConnection)
{
	setAttribute(Qt::WA_DeleteOnClose);

	setUnifiedTitleAndToolBarOnMac(true);
	QString windowTitle = tr("Drawpile Server");
	if(!m_server->isLocal())
		windowTitle = m_server->address() + " - " + windowTitle;
	setWindowTitle(windowTitle);

	// Create internal model
	m_model = new SidebarModel(this);
	connect(m_server, &Server::sessionListRefreshed, m_model, &SidebarModel::setSessionList);

	// Splitter to divide the window into a sidebar and the main work area
	QSplitter *splitter = new QSplitter(Qt::Horizontal);
	setCentralWidget(splitter);

	// Sidebar
	QTreeView *sidebar = new QTreeView;
	sidebar->setHeaderHidden(true);
	sidebar->setModel(m_model);
	sidebar->setFocusPolicy(Qt::NoFocus);
	sidebar->setFrameShape(QFrame::NoFrame);
	sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sidebar->setItemDelegate(new SidebarItemDelegate);
	sidebar->setExpandsOnDoubleClick(false);
	QPalette sidebarPalette = sidebar->palette();
	sidebarPalette.setColor(QPalette::Base, sidebarPalette.color(QPalette::Window));
	sidebar->setPalette(sidebarPalette);

	sidebar->expandAll();

	connect(sidebar->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::onPageSelect);

	splitter->addWidget(sidebar);
	splitter->setStretchFactor(0, 1);
	splitter->setCollapsible(0, false);

	// Main content area
	m_pageArea = new QScrollArea(this);
	m_pageArea->setWidgetResizable(true);
	m_pageArea->setFrameStyle(QFrame::NoFrame);
	m_pageArea->setBackgroundRole(QPalette::Base);

	splitter->addWidget(m_pageArea);
	splitter->setStretchFactor(1, 10);
	splitter->setCollapsible(1, false);

	// Activate first page
	QModelIndex firstPage = m_model->index(0, 0, m_model->index(0, 0));
	sidebar->setCurrentIndex(firstPage);
	onPageSelect(firstPage);

	// Periodically refresh session list
	QTimer *timer = new QTimer(this);
	timer->setSingleShot(false);
	timer->setInterval(5000);
	connect(timer, &QTimer::timeout, m_server, &Server::refreshSessionList);
	timer->start();

	resize(800, 600);
}

void MainWindow::onPageSelect(const QModelIndex &index)
{
	PageFactory *pf = (PageFactory*)index.data(SidebarModel::PageFactoryRole).value<void*>();
	if(!pf) {
		qDebug() << "Switch to a blank page";
		delete m_pageArea->takeWidget();
		m_currentPage = QString();
		return;
	}

	if(pf->pageId() == m_currentPage)
		return;

	qDebug() << "Switching from page" << m_currentPage << "to" << pf->pageId();
	m_currentPage = pf->pageId();

	delete m_pageArea->takeWidget();
	QWidget *page = pf->makePage(m_server);
	m_pageArea->setWidget(page);
	page->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if(!TrayIcon::isTrayIconVisible() && m_server->isLocal()) {
		LocalServer *srv = static_cast<LocalServer*>(m_server);
		if(srv->isRunning()) {
			event->ignore();
			srv->confirmQuit();
		}
	}
}

}
}

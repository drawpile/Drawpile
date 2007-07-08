/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

******************************************************************************/

#include <QDebug>
#include <QtGui>

#include "version.h"
#include "traymenu.h"

#include "statusdialog.h"
#include "configdialog.h"

#include "../server/server.h"
#include "srvthread.h"

TrayMenu::TrayMenu()
	: srvthread(0),
	srv(new Server),
	config(0),
	status(0)
{
	srvthread = new ServerThread(srv, this);
	connect(srvthread, SIGNAL(started()), this, SLOT(serverStarted()));
	connect(srvthread, SIGNAL(finished()), this, SLOT(serverStopped()));
	
	createMenu();
	
	connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
	
	setIconIndex(0);
	
	setToolTip(
		QString("%1 v%2")
		.arg(QCoreApplication::applicationName())
		.arg(trayserver::version)
	);
	
	show();
	showMsg(QCoreApplication::applicationName(), tr("Server ready for action!"), QSystemTrayIcon::Information);
}

void TrayMenu::setIconIndex(int index)
{
	assert(index >= 0 or index <= 1);
	
	static const QIcon icon[] = {
		QIcon(":/icons/offline.png"),
		QIcon(":/icons/online.png")
	};
	
	setIcon(icon[index]);
	
	//setWindowIcon(icon);
}

void TrayMenu::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::DoubleClick)
	{
		if (status->isHidden())
			status->show();
		else
			status->hide();
	}
}

void TrayMenu::showMsg(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon, uint delay)
{
	#ifdef HAVE_QT43
	showMessage(title, message, icon, delay);
	#endif
}

void TrayMenu::createMenu()
{
	QMenu *menu = new QMenu;
	
	startAction = new QAction(tr("Start"), this);
	connect(startAction, SIGNAL(triggered()), this, SLOT(start()));
	connect(startAction, SIGNAL(triggered()), srvthread, SLOT(start()));
	menu->addAction(startAction);
	
	stopAction = new QAction(tr("Stop"), this);
	connect(stopAction, SIGNAL(triggered()), srvthread, SLOT(stop()));
	connect(stopAction, SIGNAL(triggered()), this, SLOT(stop()));
	stopAction->setDisabled(true);
	menu->addAction(stopAction);
	
	QAction *configAction = new QAction(tr("Configure"), this);
	connect(configAction, SIGNAL(triggered()), this, SLOT(configOpen()));
	menu->addAction(configAction);
	
	QAction *statusAction = new QAction(tr("Status"), this);
	connect(statusAction, SIGNAL(triggered()), this, SLOT(statusOpen()));
	menu->addAction(statusAction);
	
	QAction *separator = new QAction(this);
	separator->setSeparator(true);
	menu->addAction(separator);
	
	QAction *quitAction = new QAction(tr("Quit"), this);
	connect(quitAction, SIGNAL(triggered()), srvthread, SLOT(stop()));
	connect(quitAction, SIGNAL(triggered()), this, SLOT(quitAct()));
	menu->addAction(quitAction);
	
	menu->setDefaultAction(statusAction);
	
	setContextMenu(menu);
}

void TrayMenu::start()
{
	startAction->setDisabled(true);
	
	showMsg(QCoreApplication::applicationName(), tr("Starting server..."));
}

void TrayMenu::stop()
{
	stopAction->setDisabled(true);
	
	showMsg(QCoreApplication::applicationName(), tr("Stopping server..."));
}

void TrayMenu::quitAct()
{
	if (srvthread->isRunning())
		connect(srvthread, SIGNAL(finished()), qApp, SLOT(quit()));
	else
		qApp->quit();
}

void TrayMenu::serverStarted()
{
	stopAction->setEnabled(true);
	setIconIndex(1);
	
	showMsg(QCoreApplication::applicationName(), tr("Server started!"));
}

void TrayMenu::serverStopped()
{
	startAction->setEnabled(true);
	setIconIndex(0);
	
	showMsg(QCoreApplication::applicationName(), tr("Server stopped!"));
}

void TrayMenu::statusOpen()
{
	if (!status)
	{
		status = new StatusDialog(srv, 0);
		//connect(status, SIGNAL(message(const QString&, const QString&)), this, SLOT(showMsg(const QString&, const QString&)));
		connect(status, SIGNAL(destroyed(QObject*)), this, SLOT(statusClosed()));
		connect(srvthread, SIGNAL(started()), status, SLOT(serverStarted()));
		connect(srvthread, SIGNAL(finished()), status, SLOT(serverStopped()));
	}
	
	if (status->isHidden())
		status->show();
	else
		status->activateWindow();
}

void TrayMenu::statusClosed()
{
	status = 0;
}

void TrayMenu::configOpen()
{
	if (!config)
	{
		config = new ConfigDialog(srv, 0);
		connect(config, SIGNAL(message(const QString&, const QString&, QSystemTrayIcon::MessageIcon)), this, SLOT(showMsg(const QString&, const QString&, QSystemTrayIcon::MessageIcon)));
		connect(config, SIGNAL(destroyed(QObject*)), this, SLOT(configClosed()));
		connect(srvthread, SIGNAL(started()), config, SLOT(serverStarted()));
		connect(srvthread, SIGNAL(finished()), config, SLOT(serverStopped()));
	}
	
	if (config->isHidden())
		config->show();
	else
		config->activateWindow();
}

void TrayMenu::configClosed()
{
	config = 0;
}

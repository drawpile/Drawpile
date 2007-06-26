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

TrayMenu::TrayMenu()
	: srvthread(0),
	title(tr("DrawPile Server")),
	srv(new Server)
{
	trayIcon = new QSystemTrayIcon(this);
	
	config = new ConfigDialog(srv, this);
	status = new StatusDialog(srv, this);
	//connect(status, SIGNAL(message(const QString&, const QString&)), this, SLOT(showMessage(const QString&, const QString&)));
	
	srvthread = new ServerThread(srv, this);
	
	createMenu();
	
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
	
	setIcon(0);
	
	QString tooltip_str(title);
	tooltip_str.append(" v").append(trayserver::version);
	
	trayIcon->setToolTip(tooltip_str);
	trayIcon->show();
	
	connect(srvthread, SIGNAL(started()), this, SLOT(serverStarted()));
	connect(srvthread, SIGNAL(started()), config, SLOT(serverStarted()));
	connect(srvthread, SIGNAL(started()), status, SLOT(serverStarted()));
	
	connect(srvthread, SIGNAL(finished()), this, SLOT(serverStopped()));
	connect(srvthread, SIGNAL(finished()), config, SLOT(serverStopped()));
	connect(srvthread, SIGNAL(finished()), status, SLOT(serverStopped()));
	
	showMessage(title, tr("Server ready for action!"), QSystemTrayIcon::Information);
}

void TrayMenu::setIcon(int index)
{
	assert(index >= 0 or index <= 1);
	
	static const QIcon icon[] = {
		QIcon(":/icons/offline.png"),
		QIcon(":/icons/online.png")
	};
	
	trayIcon->setIcon(icon[index]);
	
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

void TrayMenu::showMessage(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon, uint delay)
{
	#ifdef HAVE_QT43
	trayIcon->showMessage(title, message, icon, delay);
	#endif
}

void TrayMenu::createMenu()
{
	menu = new QMenu(this);
	
	startAction = new QAction(tr("Start"), this);
	connect(startAction, SIGNAL(triggered()), this, SLOT(start()));
	connect(startAction, SIGNAL(triggered()), srvthread, SLOT(start()));
	menu->addAction(startAction);
	
	stopAction = new QAction(tr("Stop"), this);
	connect(stopAction, SIGNAL(triggered()), srvthread, SLOT(stop()));
	connect(stopAction, SIGNAL(triggered()), this, SLOT(stop()));
	stopAction->setDisabled(true);
	menu->addAction(stopAction);
	
	configureAction = new QAction(tr("Configure"), this);
	connect(configureAction, SIGNAL(triggered()), this, SLOT(configSlot()));
	menu->addAction(configureAction);
	
	statusAction = new QAction(tr("Status"), this);
	connect(statusAction, SIGNAL(triggered()), this, SLOT(statusSlot()));
	menu->addAction(statusAction);
	
	QAction *separator = new QAction(this);
	separator->setSeparator(true);
	menu->addAction(separator);
	
	quitAction = new QAction(tr("Quit"), this);
	connect(quitAction, SIGNAL(triggered()), srvthread, SLOT(stop()));
	connect(quitAction, SIGNAL(triggered()), this, SLOT(quitSlot()));
	menu->addAction(quitAction);
	
	menu->setDefaultAction(statusAction);
	
	trayIcon->setContextMenu(menu);
}

void TrayMenu::start()
{
	startAction->setDisabled(true);
	
	showMessage(title, tr("Starting server..."));
}

void TrayMenu::stop()
{
	stopAction->setDisabled(true);
	
	showMessage(title, tr("Stopping server..."));
}

void TrayMenu::quitSlot()
{
	delete menu;
	delete config;
	delete status;
	
	if (srvthread->isRunning())
		connect(srvthread, SIGNAL(finished()), qApp, SLOT(quit()));
	else
		qApp->quit();
}

void TrayMenu::serverStarted()
{
	stopAction->setEnabled(true);
	setIcon(1);
	
	showMessage(title, tr("Server started!"));
}

void TrayMenu::serverStopped()
{
	startAction->setEnabled(true);
	setIcon(0);
	
	showMessage(title, tr("Server stopped!"));
}

void TrayMenu::statusSlot()
{
	if (status->isHidden())
		status->show();
	else
		status->activateWindow();
}

void TrayMenu::configSlot()
{
	if (config->isHidden())
		config->show();
	else
		config->activateWindow();
}

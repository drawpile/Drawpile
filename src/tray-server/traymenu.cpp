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

#include "traymenu.h"

#include "statusdialog.h"
#include "configdialog.h"

TrayMenu::TrayMenu()
	: title(tr("DrawPile Server")),
	srv(new Server),
	config(new ConfigDialog()),
	status(new StatusDialog())
{
	createActions();
	createTrayIcon();
	
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
	
	setIcon(0);
	
	trayIcon->setToolTip(title);
	trayIcon->show();
	
	showMessage(title, tr("Server ready for action!"), QSystemTrayIcon::Information);
}

void TrayMenu::setIcon(int index)
{
	assert(index == 0 or index == 1);
	
	QIcon icon;
	if (index == 1)
		icon = QIcon(":/icons/online.png");
	else
		icon = QIcon(":/icons/offline.png");
	trayIcon->setIcon(icon);
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

void TrayMenu::createActions()
{
	startAction = new QAction(tr("&Start"), this);
	connect(startAction, SIGNAL(triggered()), this, SLOT(startSlot()));
	
	stopAction = new QAction(tr("St&op"), this);
	connect(stopAction, SIGNAL(triggered()), this, SLOT(stopSlot()));
	stopAction->setDisabled(true);
	
	configureAction = new QAction(tr("&Configure"), this);
	connect(configureAction, SIGNAL(triggered()), this, SLOT(configSlot()));
	
	statusAction = new QAction(tr("S&tatus"), this);
	connect(statusAction, SIGNAL(triggered()), this, SLOT(statusSlot()));
	
	separator = new QAction(this);
	separator->setSeparator(true);
	
	quitAction = new QAction(tr("&Quit"), this);
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
}

void TrayMenu::createTrayIcon()
{
	menu = new QMenu(this);
	
	menu->addAction(startAction);
	menu->addAction(stopAction);
	menu->addAction(configureAction);
	menu->addAction(statusAction);
	menu->addAction(separator);
	menu->addAction(quitAction);
	
	menu->setDefaultAction(statusAction);
	
	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setContextMenu(menu);
}

void TrayMenu::toggleStartStop(bool started)
{
	startAction->setDisabled(started);
	stopAction->setDisabled(!started);
}

void TrayMenu::startSlot()
{
	srvthread = new ServerThread(srv, this);
	toggleStartStop(true);
	setIcon(1);
	
	// todo: use signal instead?
	showMessage(title, "Server started!");
}

void TrayMenu::stopSlot()
{
	delete srvthread;
	toggleStartStop(false);
	setIcon(0);
	
	// todo: use signal instead?
	showMessage(title, "Server stopped!");
}

void TrayMenu::statusSlot()
{
	/*
	if (!status)
		status = new StatusDialog(this);
	else
		status->activateWindow();
	*/
	if (status->isHidden())
		status->show();
	else
		status->activateWindow();
}

void TrayMenu::configSlot()
{
	/*
	if (!config)
		config = new ConfigDialog(this);
	else
		config->activateWindow();
	*/
	if (config->isHidden())
		config->show();
	else
		config->activateWindow();
}

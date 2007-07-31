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

#ifndef TrayMenu_INCLUDED
#define TrayMenu_INCLUDED

#include <QSystemTrayIcon>

class ServerThread;
class Server;
class StatusDialog;
class ConfigDialog;

class QAction;

//! Tray Server's main class
class TrayMenu
	: public QSystemTrayIcon
{
	Q_OBJECT
	
public:
	//! Constructor
	TrayMenu();
	
public slots:
	void serverStarted();
	void serverStopped();
	
	void configClosed() __attribute__ ((nothrow));
	void statusClosed() __attribute__ ((nothrow));
	
	//! Show balloon message
	void showMsg(const QString& title, const QString& message, QSystemTrayIcon::MessageIcon icon=QSystemTrayIcon::NoIcon, uint delay=5000);
	
private slots:
	//! Set tray icon
	void setIconIndex(int index);
	
	//! Detect double-clicking of tray icon and toggle status window with it
	void iconActivated(QSystemTrayIcon::ActivationReason reason);
	
	//! Menu 'start' action
	void start();
	
	//! Menu 'stop' action
	void stop();
	
	//! Menu 'config' action
	void configOpen();
	
	//! Menu 'status' action
	void statusOpen();
	
	//! Menu 'quit' action
	void quitAct();
	
private:
	//! Create tray context menu
	void createMenu();
	
	//! Server thread
	ServerThread *srvthread;
	
	//! Pointer to server class
	Server *srv;
	
	//! Config dialog
	ConfigDialog *config;
	//! Status dialog
	StatusDialog *status;
	
	//! Start server action
	QAction *startAction;
	//! Stop server action
	QAction *stopAction;
};

#endif // TrayMenu_INCLUDED

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

#ifndef StatusDialog_INCLUDED
#define StatusDialog_INCLUDED

#include <QDialog>
#include <QMutex>

class QLineEdit;
class QLabel;
class QGroupBox;
class Server;

//! Server status dialog
class StatusDialog
	: public QDialog
{
	Q_OBJECT
	
public:
	//! Constructor
	StatusDialog(const Server *srv, QWidget *parent);
	
public slots:
	//! Slot
	void serverStarted();
	//! Slot
	void serverStopped();
	/*
signals:
	void message(const QString& title, const QString& message);
	*/
	
private slots:
	void update();
	void updateUsers();
	void updateSessions();
	
protected:
	//! Pointer to server instance
	const Server *srv;
	
	QLineEdit *hostname;
	
	// these might need to be updated occasionally
	QGroupBox *session_group;
	QGroupBox *user_group;
	
	QMutex mutex;
};

#endif // StatusDialog_INCLUDED

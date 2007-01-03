/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class Ui_LoginDialog;

namespace dialogs {

//! Login progress dialog
/**
 * This dialog displays the progress of login/host/join sequence.
 * The sequence is made up of 7 parts:
 * - Establishing a connection to remote host
 * - Logging in
 * - Requesting server password from user (if required)
 * - Creating the session (if hosting)
 * - Selecting a session (if not hosting and there are more than one sessions)
 * - Joining the selected session
 * - Downloading raster data
 */
class LoginDialog : public QDialog
{
	Q_OBJECT
	public:
		LoginDialog(QWidget *parent=0);
		~LoginDialog();

	public slots:
		//! Show dialog and display connecting status
		void connecting(const QString& address);

		//! Connection established, show login
		void connected();

		//! Login complete
		void loggedin();

		//! Joined a session
		void joined();

		//! Raster data received
		void raster(int p);

		//! Request password from user
		void getPassword(bool session);

		//! Disconnected before login sequence was finished
		void disconnected();

		//! No sessions were available
		void noSessions();

	signals:
		//! User has entered a password
		void password(const QString& password);

		//! User has selected a session
		void session(int id);

	private slots:
		void sendPassword();

	private:
		Ui_LoginDialog *ui_;
		bool appenddisconnect_;
};

}

#endif


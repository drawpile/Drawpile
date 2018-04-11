/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <QDialog>

class Ui_HostDialog;

class SessionLoader;

namespace dialogs {

class HostDialog : public QDialog
{
	Q_OBJECT
public:
	explicit HostDialog(QWidget *parent=0);
	~HostDialog();

	//! Store settings in configuration file
	void rememberSettings() const;

	//! Get the remote host address
	QString getRemoteAddress() const;

	//! Host on a remote server?
	bool useRemoteAddress() const;

	//! Get the username
	QString getUserName() const;

	//! Get session title
	QString getTitle() const;

	//! Get session password
	QString getPassword() const;

	//! Get the desired session alias
	QString getSessionAlias() const;

	//! Get the announcement server URL (empty if not selected)
	QString getAnnouncementUrl() const;

	//! Make a private (room code only) announcement instead of a public one?
	bool getAnnouncmentPrivate() const;

private:
	Ui_HostDialog *_ui;
};

}

#endif


/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <QDialog>

class Ui_HostDialog;

namespace dialogs {

class HostDialog : public QDialog
{
	Q_OBJECT
	public:
		HostDialog(const QImage &original, QWidget *parent=0);
		~HostDialog();

		//! Get the remote host address
		QString getRemoteAddress() const;

		//! Host on a remote server?
		bool useRemoteAddress() const;

		//! Get the port to use for local server
		int getPort() const;

		//! Check if selected port is the built in default port
		bool isDefaultPort() const;

		//! Get the username
		QString getUserName() const;

		//! Get session title
		QString getTitle() const;

		//! Get session password
		QString getPassword() const;

		//! Get server admin password
		QString getAdminPassword() const;

		//! Get max. user count
		int getUserLimit() const;

		//! Should users be allowed to draw by default
		bool getAllowDrawing() const;

		//! Should users be allowed to chat by default
		bool getAllowChat() const;

		//! Get session image
		QImage getImage() const;

		//! Use the original image?
		bool useOriginalImage() const;

	private slots:
		void selectColor();
		void selectPicture();
		void newSelected();

	private:
		Ui_HostDialog *ui_;
		QString prevpath_;
};

}

#endif


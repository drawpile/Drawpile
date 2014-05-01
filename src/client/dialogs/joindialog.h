/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#ifndef JOINDIALOG_H
#define JOINDIALOG_H

#include <QDialog>

class Ui_JoinDialog;

namespace dialogs {

class JoinDialog : public QDialog
{
	Q_OBJECT
	public:
		JoinDialog(QWidget *parent=0);
		~JoinDialog();

		//! Get the host address
		QString getAddress() const;

		//! Get the username
		QString getUserName() const;

		//! Store settings in configuration file
		void rememberSettings() const;

	private:
		Ui_JoinDialog *ui_;
};

}

#endif


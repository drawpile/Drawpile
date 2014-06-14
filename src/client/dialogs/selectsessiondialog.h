/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef SELECTSESSIONDIALOG_H
#define SELECTSESSIONDIALOG_H

#include <QDialog>

class Ui_SelectSession;

namespace net {
	class LoginSessionModel;
}

namespace dialogs {

class SelectSessionDialog : public QDialog
{
	Q_OBJECT
public:
	explicit SelectSessionDialog(net::LoginSessionModel *model, QWidget *parent = 0);

	void setServerTitle(const QString &title);

signals:
	/**
	 * @brief A selection was made
	 * @param id session ID
	 * @param needPassword if true, a password is needed to join the session
	 */
	void selected(const QString &id, bool needPassword);

public slots:

private:
	Ui_SelectSession *_ui;

};

}

#endif

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>

class Ui_AuthDialog;

namespace server {
namespace gui {

class AuthDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit AuthDialog(QWidget *parent=nullptr);
	~AuthDialog() override;

	static void init();

private:
	Ui_AuthDialog *m_ui;
};

}
}

#endif

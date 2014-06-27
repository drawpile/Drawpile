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

#include "logindialog.h"
#include "ui_logindialog.h"

#include "utils/mandatoryfields.h"
#include "utils/usernamevalidator.h"

#include <QPushButton>

namespace dialogs {

LoginDialog::LoginDialog(QWidget *parent) :
	QDialog(parent), _ui(new Ui_LoginDialog)
{
	_ui->setupUi(this);

	_ui->username->setValidator(new UsernameValidator(this));
	_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Continue"));

	new MandatoryFields(this, _ui->buttonBox->button(QDialogButtonBox::Ok));

	connect(this, &LoginDialog::accepted, [this]() {
		emit login(_ui->password->text(), _ui->username->text());
	});
}

void LoginDialog::setIntroText(const QString &text)
{
	if(text.isEmpty()) {
		_ui->intro->hide();
	} else {
		_ui->intro->setText(text);
		_ui->intro->show();
	}
}

void LoginDialog::setUsername(const QString &username, bool enabled)
{
	_ui->username->setText(username);
	_ui->username->setEnabled(enabled);
}


}

/*
   Drawpile - a collaborative drawing program.

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

#include <QPushButton>
#include <QSettings>

#include "joindialog.h"
#include "../utils/mandatoryfields.h"
#include "../utils/usernamevalidator.h"

#include "ui_joindialog.h"

namespace dialogs {

JoinDialog::JoinDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_JoinDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	_ui->username->setValidator(new UsernameValidator(this));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("history");
	_ui->address->insertItems(0, cfg.value("recenthosts").toStringList());
	_ui->username->setText(cfg.value("username").toString());

	new MandatoryFields(this, _ui->buttons->button(QDialogButtonBox::Ok));
}

void JoinDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("username", getUserName());
	QStringList hosts;
	// Move current item to the top of the list
	const QString current = _ui->address->currentText();
	int curindex = _ui->address->findText(current);
	if(curindex>=0)
		_ui->address->removeItem(curindex);
	hosts << current;
	for(int i=0;i<_ui->address->count();++i)
		hosts << _ui->address->itemText(i);
	cfg.setValue("recenthosts", hosts);
}

JoinDialog::~JoinDialog()
{
	delete _ui;
}

QString JoinDialog::getAddress() const {
	return _ui->address->currentText();
}

QString JoinDialog::getUserName() const {
	return _ui->username->text();
}

bool JoinDialog::recordSession() const {
	return _ui->recordSession->isChecked();
}

}


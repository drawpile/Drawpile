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
	ui_ = new Ui_JoinDialog;
	ui_->setupUi(this);
	ui_->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	ui_->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	ui_->username->setValidator(new UsernameValidator(this));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("history");
	ui_->address->insertItems(0, cfg.value("recenthosts").toStringList());
	ui_->username->setText(cfg.value("username").toString());

	new MandatoryFields(this, ui_->buttons->button(QDialogButtonBox::Ok));
}

void JoinDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("username", getUserName());
	QStringList hosts;
	// Move current item to the top of the list
	const QString current = ui_->address->currentText();
	int curindex = ui_->address->findText(current);
	if(curindex>=0)
		ui_->address->removeItem(curindex);
	hosts << current;
	for(int i=0;i<ui_->address->count();++i)
		hosts << ui_->address->itemText(i);
	cfg.setValue("recenthosts", hosts);
}

JoinDialog::~JoinDialog()
{
	delete ui_;
}

QString JoinDialog::getAddress() const {
	return ui_->address->currentText();
}

QString JoinDialog::getUserName() const {
	return ui_->username->text();
}

}


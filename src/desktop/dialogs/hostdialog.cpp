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

#include "dialogs/hostdialog.h"

#include "utils/mandatoryfields.h"
#include "utils/usernamevalidator.h"
#include "utils/sessionidvalidator.h"
#include "utils/images.h"
#include "utils/listservermodel.h"

#include <QPushButton>
#include <QFileDialog>
#include <QImageReader>
#include <QSettings>
#include <QMessageBox>
#include <QSettings>

#include "ui_hostdialog.h"

namespace dialogs {

HostDialog::HostDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_HostDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	_ui->idAlias->setValidator(new SessionIdAliasValidator(this));

	// Session tab defaults
	QSettings cfg;
	cfg.beginGroup("history");

	_ui->sessiontitle->setText(cfg.value("sessiontitle").toString());
	_ui->idAlias->setText(cfg.value("idalias").toString());

	_ui->listingserver->setModel(new sessionlisting::ListServerModel(this));
	_ui->announce->setChecked(cfg.value("announce", false).toBool());
	_ui->listingserver->setCurrentIndex(cfg.value("listingserver", 0).toInt());

	// Server box defaults
	if(cfg.value("hostremote", false).toBool())
		_ui->useremote->setChecked(true);
	_ui->remotehost->insertItems(0, cfg.value("recentremotehosts").toStringList());

	new MandatoryFields(this, _ui->buttons->button(QDialogButtonBox::Ok));
}

HostDialog::~HostDialog()
{
	delete _ui;
}

void HostDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");

	cfg.setValue("sessiontitle", getTitle());
	cfg.setValue("announce", _ui->announce->isChecked());
	cfg.setValue("listingserver", _ui->listingserver->currentIndex());

	// Move current address to the top of the list
	QStringList hosts;
	const QString current = _ui->remotehost->currentText();
	int curind = _ui->remotehost->findText(current);
	if(curind!=-1)
		_ui->remotehost->removeItem(curind);
	hosts << current;
	for(int i=0;i<_ui->remotehost->count();++i)
			hosts << _ui->remotehost->itemText(i);
	_ui->remotehost->setCurrentText(current);

	cfg.setValue("recentremotehosts", hosts);
	cfg.setValue("hostremote", _ui->useremote->isChecked());

	// Remember settings tab values
	cfg.setValue("idalias", _ui->idAlias->text());

}

QString HostDialog::getRemoteAddress() const
{
	return _ui->remotehost->currentText();
}

bool HostDialog::useRemoteAddress() const
{
	return _ui->useremote->isChecked();
}

QString HostDialog::getTitle() const
{
	return _ui->sessiontitle->text();
}

QString HostDialog::getPassword() const
{
	return _ui->sessionpassword->text();
}

QString HostDialog::getSessionAlias() const
{
	return _ui->idAlias->text();
}

QString HostDialog::getAnnouncementUrl() const
{
	if(_ui->announce->isChecked())
		return _ui->listingserver->currentData().toString();
	return QString();
}

bool HostDialog::getAnnouncmentPrivate() const
{
	return _ui->listPrivate->isChecked();
}

}

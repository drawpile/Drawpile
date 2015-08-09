/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#include "joindialog.h"
#include "sessionlistingdialog.h"
#include "../utils/mandatoryfields.h"
#include "../utils/usernamevalidator.h"

#include "ui_joindialog.h"

#include <QPushButton>
#include <QSettings>
#include <QUrl>

namespace dialogs {

JoinDialog::JoinDialog(const QUrl &url, QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_JoinDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	QPushButton *findBtn = _ui->buttons->addButton(tr("Find..."), QDialogButtonBox::ActionRole);

	_ui->username->setValidator(new UsernameValidator(this));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("history");
	_ui->address->insertItems(0, cfg.value("recenthosts").toStringList());
	_ui->username->setText(cfg.value("username").toString());

	if(!url.isEmpty())
		_ui->address->setCurrentText(url.toString());

	new MandatoryFields(this, _ui->buttons->button(QDialogButtonBox::Ok));

	connect(findBtn, &QPushButton::clicked, this, &JoinDialog::showListingDialog);
}

static QString cleanAddress(const QString &addr)
{
	if(addr.startsWith("drawpile://")) {
		QUrl url(addr);
		if(url.isValid()) {
			QString a = url.host();
			if(url.port()!=-1)
				a += ":" + QString::number(url.port());
			return a;
		}
	}
	return addr;
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
	hosts << cleanAddress(current);
	for(int i=0;i<_ui->address->count();++i) {
		if(!_ui->address->itemText(i).isEmpty())
			hosts << _ui->address->itemText(i);
	}
	cfg.setValue("recenthosts", hosts);
}

JoinDialog::~JoinDialog()
{
	delete _ui;
}

QString JoinDialog::getAddress() const {
	return _ui->address->currentText().trimmed();
}

QString JoinDialog::getUserName() const {
	return _ui->username->text().trimmed();
}

bool JoinDialog::recordSession() const {
	return _ui->recordSession->isChecked();
}

QUrl JoinDialog::getUrl() const
{
	QString address = getAddress();
	QString username = getUserName();

	QString scheme;
	if(address.startsWith("drawpile://")==false)
		scheme = "drawpile://";

	QUrl url = QUrl(scheme + address, QUrl::TolerantMode);
	if(!url.isValid() || url.host().isEmpty() || username.isEmpty())
		return QUrl();

	url.setUserName(username);

	return url;
}

void JoinDialog::setUrl(const QUrl &url)
{
	_ui->address->setCurrentText(url.toString());
}

void JoinDialog::showListingDialog()
{
	SessionListingDialog *ld = new SessionListingDialog(this);
	connect(ld, &SessionListingDialog::selected, this, &JoinDialog::setUrl);
	ld->setWindowModality(Qt::WindowModal);
	ld->setAttribute(Qt::WA_DeleteOnClose);
	ld->show();
}

}


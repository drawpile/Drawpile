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

#include "thinsrv/gui/authdialog.h"
#include "ui_authdialog.h"
#include "libshared/util/networkaccess.h"

#include <QNetworkAccessManager>
#include <QAuthenticator>
#include <QNetworkReply>

namespace server {
namespace gui {

AuthDialog::AuthDialog(QWidget *parent)
	: QDialog(parent)
	, m_ui(new Ui_AuthDialog)
{
	m_ui->setupUi(this);
}

AuthDialog::~AuthDialog()
{
	delete m_ui;
}

void AuthDialog::init()
{
	QNetworkAccessManager *nm = networkaccess::getInstance();
	connect(nm, &QNetworkAccessManager::authenticationRequired, [](QNetworkReply *reply, QAuthenticator *auth) {
		AuthDialog dlg;
		dlg.m_ui->label->setText(QStringLiteral("Authentication required for \"%1\"").arg(reply->request().url().host()));
		if(dlg.exec() == QDialog::Accepted) {
			auth->setUser(dlg.m_ui->username->text());
			auth->setPassword(dlg.m_ui->password->text());
		}
	});
}

}
}

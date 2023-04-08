// SPDX-License-Identifier: GPL-3.0-or-later

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

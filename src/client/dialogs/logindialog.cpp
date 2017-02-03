/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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
#include "net/login.h"
#include "net/loginsessions.h"

#include "utils/usernamevalidator.h"
#include "utils/html.h"

#include "ui_logindialog.h"

#include <QPushButton>

namespace dialogs {

static const int PAGE_START = 0;
static const int PAGE_AUTH = 1;
static const int PAGE_SESSIONLIST = 2;
static const int PAGE_CERTWARNING = 3;
static const int PAGE_CATCHUP = 3;

LoginDialog::LoginDialog(net::LoginHandler *login, QWidget *parent) :
	QDialog(parent), m_mode(LABEL), m_login(login), m_ui(new Ui_LoginDialog)
{
	setWindowModality(Qt::WindowModal);
	setWindowTitle(login->url().host());

	m_ui->setupUi(this);
	m_ui->servertitle->hide();

	// Login page
	m_ui->username->setText(login->url().userName());
	m_ui->username->setValidator(new UsernameValidator(this));

	auto requireFields = [this]() {
		// Enable Continue button when required fields are not empty
		bool ok = true;
		if(m_ui->username->isEnabled() && m_ui->username->text().trimmed().isEmpty())
			ok = false;
		if(m_ui->password->text().isEmpty())
			ok = false;
		m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
	};
	connect(m_ui->username, &QLineEdit::textChanged, requireFields);
	connect(m_ui->password, &QLineEdit::textChanged, requireFields);

	// Session list page
	connect(m_ui->sessionlist, &QTableView::doubleClicked, [this](const QModelIndex&) {
		if(m_ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled())
			m_ui->buttonBox->button(QDialogButtonBox::Ok)->click();
	});

	// Login process
	connect(m_ui->buttonBox, &QDialogButtonBox::clicked, this, &LoginDialog::onButtonClick);
	connect(this, &QDialog::rejected, login, &net::LoginHandler::cancelLogin);

	m_loginDestructConnection = connect(login, &net::LoginHandler::destroyed, this, &LoginDialog::deleteLater);

	connect(login, &net::LoginHandler::passwordNeeded, this, &LoginDialog::onPasswordNeeded);
	connect(login, &net::LoginHandler::loginNeeded, this, &LoginDialog::onLoginNeeded);
	connect(login, &net::LoginHandler::sessionChoiceNeeded, this, &LoginDialog::onSessionChoiceNeeded);
	connect(login, &net::LoginHandler::certificateCheckNeeded, this, &LoginDialog::onCertificateCheckNeeded);
	connect(login, &net::LoginHandler::serverTitleChanged, this, &LoginDialog::onServerTitleChanged);

	resetMode();
}

LoginDialog::~LoginDialog()
{
	delete m_ui;
}

void LoginDialog::resetMode(Mode mode)
{
	m_mode = mode;

	if(mode == LABEL) {
		m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
	} else if(mode == CATCHUP) {
		m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok);
	} else {
		m_ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
		m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
	}

	switch(mode) {
	case LABEL:
		m_ui->pages->setCurrentIndex(PAGE_START);
		return;

	case PASSWORD:
	case LOGIN:
		m_ui->pages->setCurrentIndex(PAGE_AUTH);
		m_ui->username->setEnabled(mode == LOGIN);
		break;

	case SESSION:
		m_ui->pages->setCurrentIndex(PAGE_SESSIONLIST);
		break;

	case CERT:
		m_ui->pages->setCurrentIndex(PAGE_CERTWARNING);
		break;

	case CATCHUP:
		m_ui->pages->setCurrentIndex(PAGE_CATCHUP);
		break;
	}

	m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(mode == CERT || mode == CATCHUP);
	m_ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
}

void LoginDialog::onPasswordNeeded(const QString &prompt)
{
	m_ui->intro->setText(prompt);
	m_ui->password->setText(QString());
	resetMode(PASSWORD);
}

void LoginDialog::onLoginNeeded(const QString &prompt)
{
	m_ui->intro->setText(prompt);
	m_ui->password->setText(QString());
	resetMode(LOGIN);
}

void LoginDialog::onSessionChoiceNeeded(net::LoginSessionModel *sessions)
{
	m_ui->sessionlist->setModel(sessions);

	QHeaderView *header = m_ui->sessionlist->horizontalHeader();
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(0, QHeaderView::Fixed);
	header->resizeSection(0, 24);

	connect(m_ui->sessionlist->selectionModel(), &QItemSelectionModel::selectionChanged, [this](const QItemSelection &sel) {
		// Enable/disable OK button depending on the selection
		bool ok;

		if(sel.indexes().isEmpty())
			ok = false;
		else
			ok = sel.indexes().at(0).data(net::LoginSessionModel::JoinableRole).toBool();

		m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
	});

	resetMode(SESSION);
}

void LoginDialog::onCertificateCheckNeeded(const QSslCertificate &newCert, const QSslCertificate &oldCert)
{
	Q_UNUSED(newCert);
	Q_UNUSED(oldCert);
	// TODO display certificate for comparison
	resetMode(CERT);
}

void LoginDialog::onLoginDone(bool join)
{
	if(join) {
		// Show catchup progress page when joining
		disconnect(m_loginDestructConnection);
		resetMode(CATCHUP);
	}
}

void LoginDialog::onServerTitleChanged(const QString &title)
{
	if(title.isEmpty()) {
		m_ui->servertitle->setVisible(false);

	} else {
		m_ui->servertitle->setVisible(true);
		m_ui->servertitle->setText(htmlutils::newlineToBr(htmlutils::linkify(title.toHtmlEscaped())));
	}
}

void LoginDialog::onButtonClick(QAbstractButton *btn)
{
	QDialogButtonBox::StandardButton b = m_ui->buttonBox->standardButton(btn);

	if(b == QDialogButtonBox::Ok) {
		switch(m_mode) {
		case LABEL: /* no OK button in this mode */; break;
		case PASSWORD:
			m_login->gotPassword(m_ui->password->text());
			resetMode();
			break;
		case LOGIN:
			m_login->selectIdentity(m_ui->username->text(), m_ui->password->text());
			resetMode();
			break;
		case SESSION: {
			Q_ASSERT(!m_ui->sessionlist->selectionModel()->selectedIndexes().isEmpty());

			const int row = m_ui->sessionlist->selectionModel()->selectedIndexes().at(0).row();
			const net::LoginSession &s = static_cast<net::LoginSessionModel*>(m_ui->sessionlist->model())->sessionAt(row);
			m_login->joinSelectedSession(s.idOrAlias(), s.needPassword);
			break; }
		case CERT:
			m_login->acceptServerCertificate();
			resetMode();
			break;
		case CATCHUP:
			deleteLater();
			break;
		}

	} else {
		reject();
	}
}

void LoginDialog::catchupProgress(int value)
{
	m_ui->catchupProgress->setValue(value);
	if(m_mode == CATCHUP && value >= 100)
		this->deleteLater();
}

}

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

#include "joindialog.h"
#include "sessionlistingdialog.h"
#include "utils/mandatoryfields.h"
#include "utils/usernamevalidator.h"
#include "utils/listservermodel.h"
#include "../shared/util/announcementapi.h"

#include "ui_joindialog.h"

#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QTimer>

namespace dialogs {

JoinDialog::JoinDialog(const QUrl &url, QWidget *parent)
	: QDialog(parent), m_announcementApi(nullptr)
{
	m_ui = new Ui_JoinDialog;
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	QPushButton *findBtn = m_ui->buttons->addButton(tr("Find..."), QDialogButtonBox::ActionRole);

	m_ui->username->setValidator(new UsernameValidator(this));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("history");
	m_ui->address->insertItems(0, cfg.value("recenthosts").toStringList());
	m_ui->username->setText(cfg.value("username").toString());

	if(!url.isEmpty())
		m_ui->address->setCurrentText(url.toString());

	connect(m_ui->address, &QComboBox::editTextChanged, this, &JoinDialog::addressChanged);

	new MandatoryFields(this, m_ui->buttons->button(QDialogButtonBox::Ok));

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

static bool isRoomcode(const QString &str) {
	// Roomcodes are always exactly 5 letters long
	if(str.length() != 5)
		return false;

	// And consist of characters in range A-Z
	for(int i=0;i<str.length();++i)
		if(str.at(i) < 'A' || str.at(i) > 'Z')
			return false;

	return true;
}

void JoinDialog::addressChanged(const QString &addr)
{
	if(isRoomcode(addr)) {
		// A room code was just entered. Trigger session URL query
		m_roomcode = addr;
		m_ui->address->setEditText(QString());
		m_ui->address->lineEdit()->setPlaceholderText(tr("Searching..."));
		m_ui->address->lineEdit()->setReadOnly(true);
		sessionlisting::ListServerModel servermodel(false);
		for(const sessionlisting::ListServer &s : servermodel.servers()) {
			m_roomcodeServers << s.url;
		}
		resolveRoomcode();
	}
}

void JoinDialog::resolveRoomcode()
{
	if(m_roomcodeServers.isEmpty()) {
		// Tried all the servers and didn't find the code
		qDebug("Join code %s not found!", qPrintable(m_roomcode));

		m_ui->address->lineEdit()->setPlaceholderText(tr("Room code not found!"));
		QTimer::singleShot(1500, this, [this]() {
			m_ui->address->setEditText(QString());
			m_ui->address->lineEdit()->setReadOnly(false);
			m_ui->address->lineEdit()->setPlaceholderText(QString());
			m_ui->address->setFocus();
		});

	} else {
		QUrl listServer = m_roomcodeServers.takeFirst();
		qDebug("Querying join code %s at server: %s", qPrintable(m_roomcode), qPrintable(listServer.toString()));
		if(!m_announcementApi) {
			m_announcementApi = new sessionlisting::AnnouncementApi(this);
			connect(m_announcementApi, &sessionlisting::AnnouncementApi::error, this, &JoinDialog::resolveRoomcode);
			connect(m_announcementApi, &sessionlisting::AnnouncementApi::sessionFound, this, [this](const sessionlisting::Session &s) {
				QString url = "drawpile://" + s.host;
				if(s.port != 27750)
					url += QString(":%1").arg(s.port);
				url += '/';
				url += s.id;
				m_ui->address->lineEdit()->setReadOnly(false);
				m_ui->address->lineEdit()->setPlaceholderText(QString());
				m_ui->address->setEditText(url);
				m_ui->address->setEnabled(true);
			});
		}
		m_announcementApi->queryRoomcode(listServer, m_roomcode);
	}
}

void JoinDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("username", getUserName());
	QStringList hosts;
	// Move current item to the top of the list
	const QString current = m_ui->address->currentText();
	int curindex = m_ui->address->findText(current);
	if(curindex>=0)
		m_ui->address->removeItem(curindex);
	hosts << cleanAddress(current);
	for(int i=0;i<qMin(8, m_ui->address->count());++i) {
		if(!m_ui->address->itemText(i).isEmpty())
			hosts << m_ui->address->itemText(i);
	}
	cfg.setValue("recenthosts", hosts);
}

JoinDialog::~JoinDialog()
{
	delete m_ui;
}

QString JoinDialog::getAddress() const {
	return m_ui->address->currentText().trimmed();
}

QString JoinDialog::getUserName() const {
	return m_ui->username->text().trimmed();
}

bool JoinDialog::recordSession() const {
	return m_ui->recordSession->isChecked();
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
	m_ui->address->setCurrentText(url.toString());
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


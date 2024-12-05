// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/session.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Session::Session(bool connected, QWidget *parent)
	: QWidget(parent)
	, m_connected(connected)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	QFormLayout *settingsForm = new QFormLayout;
	settingsForm->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(settingsForm);

	m_typeCombo = new QComboBox;
	m_typeCombo->addItem(
		QIcon::fromTheme("object-locked"),
		tr("Personal session, only people you invite can join"),
		int(Type::Passworded));
	m_typeCombo->addItem(
		QIcon::fromTheme("globe"), tr("Public session, anyone can freely join"),
		int(Type::Public));
	settingsForm->addRow(m_typeCombo);

	m_passwordLabel = new QLabel(tr("Password:"));
	utils::setWidgetRetainSizeWhenHidden(m_passwordLabel, true);

	QHBoxLayout *passwordLayout = new QHBoxLayout;
	passwordLayout->setContentsMargins(0, 0, 0, 0);
	settingsForm->addRow(m_passwordLabel, passwordLayout);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setPlaceholderText(
		tr("Enter or generate a session password"));
	utils::setWidgetRetainSizeWhenHidden(m_passwordEdit, true);
	passwordLayout->addWidget(m_passwordEdit, 1);

	m_passwordButton = new QPushButton(tr("Generate"));
	utils::setWidgetRetainSizeWhenHidden(m_passwordButton, true);
	passwordLayout->addWidget(m_passwordButton);
	connect(
		m_passwordButton, &QPushButton::clicked, this,
		&Session::generatePassword);

	// These checkbox labels are just here to keep the form from jiggering left
	// and right as rows are made visible and invisible.
	m_nsfmBox = new QCheckBox(tr("Not suitable for minors (NSFM)"));
	settingsForm->addRow(m_nsfmBox);

	m_ephemeralBox =
		new QCheckBox(tr("Terminate session after everyone has left"));
	settingsForm->addRow(m_ephemeralBox);

	m_keepChatBox = new QCheckBox(tr("Keep chat history"));
	settingsForm->addRow(m_keepChatBox);

	utils::addFormSeparator(layout);

	if(connected) {
		// TODO
	} else {
		QFormLayout *serverForm = new QFormLayout;
		serverForm->setContentsMargins(0, 0, 0, 0);
		layout->addLayout(serverForm);

		m_serverCombo = new QComboBox;
		m_serverCombo->addItem(QStringLiteral("pub.drawpile.net"), -1);
		m_serverCombo->addItem(tr("Host or IP address"), -2);
		serverForm->addRow(tr("Server:"), m_serverCombo);

		m_hostEdit = new QLineEdit;
		m_hostEdit->setPlaceholderText(tr("Enter a host or IP address"));
		serverForm->addRow(tr("Address:"), m_hostEdit);

		m_serverInfo = new QTextBrowser;
		m_serverInfo->setOpenLinks(false);
		m_serverInfo->setOpenExternalLinks(false);
		m_serverInfo->setMinimumHeight(24);

		desktop::settings::Settings &settings = dpApp().settings();
		if(settings.lastSessionPassword().trimmed().isEmpty()) {
			generatePassword();
		}
		settings.bindLastHostType(m_typeCombo, Qt::UserRole);
		settings.bindLastHostType(this, &Session::updateType);
		settings.bindLastSessionPassword(m_passwordEdit);
		settings.bindLastNsfm(m_nsfmBox);
		settings.bindLastEphemeral(m_ephemeralBox);
		settings.bindLastKeepChat(m_keepChatBox);
	}

	layout->addStretch();
}

void Session::updateType(int type)
{
	utils::ScopedUpdateDisabler disabler(this);
	bool passwordVisible = type != int(Type::Public);
	m_passwordLabel->setVisible(passwordVisible);
	m_passwordEdit->setVisible(passwordVisible);
	m_passwordButton->setVisible(passwordVisible);
}

void Session::generatePassword()
{
	// Passwords are just a mechanism to facilitate invite-only sessions.
	// They're not secret and are meant to be shared with anyone who wants to
	// join the session, so we don't need cryptographic security for them.
	QString characters = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
	QString password;
	int length = QRandomGenerator::global()->bounded(8, 12);
	for(int i = 0; i < length; ++i) {
		password.append(characters[QRandomGenerator::global()->bounded(
			0, characters.length())]);
	}
	dpApp().settings().setLastSessionPassword(password);
}

}
}
}

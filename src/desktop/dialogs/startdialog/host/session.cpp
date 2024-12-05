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
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Session::Session(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	QFormLayout *settingsForm = new QFormLayout;
	settingsForm->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(settingsForm);

	QString typeText = tr("Type:");
	m_typeCombo = new QComboBox;
	m_typeCombo->addItem(tr("Invite-only session"), int(Type::Hidden));
	m_typeCombo->addItem(
		tr("Invite- or password-only session"), int(Type::Visible));
	m_typeCombo->addItem(
		tr("Public session, anyone can join"), int(Type::Public));
	settingsForm->addRow(typeText, m_typeCombo);

	m_titleLabel = new QLabel(tr("Title:"));
	m_titleEdit = new QLineEdit;
	m_titleEdit->setPlaceholderText(tr("Enter a name for your session"));
	settingsForm->addRow(m_titleLabel, m_titleEdit);

	m_passwordLabel = new QLabel(tr("Password:"));
	QHBoxLayout *passwordLayout = new QHBoxLayout;
	passwordLayout->setContentsMargins(0, 0, 0, 0);
	settingsForm->addRow(m_passwordLabel, passwordLayout);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setPlaceholderText(
		tr("Enter or generate a session password"));
	passwordLayout->addWidget(m_passwordEdit, 1);

	m_passwordButton = new QPushButton(tr("Generate"));
	passwordLayout->addWidget(m_passwordButton);

	// These checkbox labels are just here to keep the form from jiggering left
	// and right as rows are made visible and invisible.
	m_nsfmBox = new QCheckBox(tr("Not suitable for minors (NSFM)"));
	settingsForm->addRow(makeSpacerLabel(typeText), m_nsfmBox);

	m_persistentBox =
		new QCheckBox(tr("Terminate session after everyone has left"));
	settingsForm->addRow(
		makeSpacerLabel(m_titleLabel->text()), m_persistentBox);

	m_keepChatBox = new QCheckBox(tr("Keep chat history"));
	settingsForm->addRow(
		makeSpacerLabel(m_passwordLabel->text()), m_keepChatBox);

	utils::addFormSeparator(layout);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_stack);

	QWidget *serverPage = new QWidget;
	serverPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(serverPage);

	QVBoxLayout *serverLayout = new QVBoxLayout(serverPage);
	serverLayout->setContentsMargins(0, 0, 0, 0);

	QFormLayout *serverForm = new QFormLayout;
	serverForm->setContentsMargins(0, 0, 0, 0);
	serverLayout->addLayout(serverForm);

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
	serverLayout->addWidget(m_serverInfo);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindLastHostType(m_typeCombo, Qt::UserRole);
	settings.bindLastHostType(this, &Session::updateType);
}

QLabel *Session::makeSpacerLabel(const QString &text)
{
	QLabel *label = new QLabel(text);
	label->setVisible(false);
	utils::setWidgetRetainSizeWhenHidden(label, true);
	return label;
}

void Session::updateType(int type)
{
	bool titleVisible, passwordVisible;
	switch(type) {
	case int(Type::Public):
		titleVisible = true;
		passwordVisible = false;
		break;
	case int(Type::Visible):
		titleVisible = true;
		passwordVisible = true;
		break;
	default:
		titleVisible = false;
		passwordVisible = true;
		break;
	}
	utils::ScopedUpdateDisabler disabler(this);
	m_titleLabel->setVisible(titleVisible);
	m_titleEdit->setVisible(titleVisible);
	m_passwordLabel->setVisible(passwordVisible);
	m_passwordEdit->setVisible(passwordVisible);
	m_passwordButton->setVisible(passwordVisible);
}

}
}
}

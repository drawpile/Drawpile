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

#include "serversummarypage.h"
#include "subheaderwidget.h"
#include "localserver.h"
#include "trayicon.h"
#include "../shared/server/serverconfig.h"
#include "ui_settings.h"

#include <QDebug>
#include <QJsonObject>
#include <QGridLayout>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QDialog>
#include <QSettings>
#include <QFileDialog>

#include <functional>

namespace server {
namespace gui {

static const QString REQ_ID = "serversettings";

struct ServerSummaryPage::Private {
	Server *server;
	QLabel
		*status,
		*address,
		*port
		;

	QDoubleSpinBox *clientTimeout;
	QCheckBox *allowGuests;
	QCheckBox *allowGuestHosts;

	QDoubleSpinBox *sessionSizeLimit;
	QDoubleSpinBox *idleTimeout;
	QSpinBox *maxSessions;
	QCheckBox *persistence;
	QCheckBox *privateUserList;
	QCheckBox *archiveSessions;

	QPushButton *startStopButton;
	QJsonObject lastUpdate;
	QTimer *saveTimer;

	Private()
		:
		  status(new QLabel),
		  address(new QLabel),
		  port(new QLabel),
		  clientTimeout(new QDoubleSpinBox),
		  allowGuests(new QCheckBox),
		  allowGuestHosts(new QCheckBox),
		  sessionSizeLimit(new QDoubleSpinBox),
		  idleTimeout(new QDoubleSpinBox),
		  maxSessions(new QSpinBox),
		  persistence(new QCheckBox),
		  privateUserList(new QCheckBox),
		  archiveSessions(new QCheckBox)
	{
		clientTimeout->setSuffix(" min");
		clientTimeout->setSingleStep(0.5);
		clientTimeout->setSpecialValueText(tr("unlimited"));
		allowGuests->setText(ServerSummaryPage::tr("Allow unauthenticated users"));
		allowGuestHosts->setText(ServerSummaryPage::tr("Allow anyone to host sessions"));

		sessionSizeLimit->setSuffix(" MB");
		sessionSizeLimit->setSpecialValueText(tr("unlimited"));
		idleTimeout->setSuffix(" min");
		idleTimeout->setSingleStep(1);
		idleTimeout->setSpecialValueText(tr("unlimited"));
		idleTimeout->setMaximum(7 * 24 * 60);

		persistence->setText(ServerSummaryPage::tr("Allow sessions to persist without users"));
		privateUserList->setText(ServerSummaryPage::tr("Do not include user list is session announcement"));
		archiveSessions->setText(ServerSummaryPage::tr("Archive terminated sessions"));
	}
};

static void addWidgets(struct ServerSummaryPage::Private *d, QGridLayout *layout, int row, const QString labelText, QWidget *value, bool addSpacer=false)
{
	if(!labelText.isEmpty())
		layout->addWidget(new QLabel(labelText + ":"), row, 0, 1, 1, Qt::AlignRight);

	if(addSpacer) {
		auto *l2 = new QHBoxLayout;
		l2->addWidget(value);
		l2->addStretch(1);
		layout->addLayout(l2, row, 1);
	} else {
		layout->addWidget(value, row, 1);
	}

	if(value->inherits("QAbstractSpinBox"))
		value->connect(value, SIGNAL(valueChanged(QString)), d->saveTimer, SLOT(start()));
	else if(value->inherits("QAbstractButton"))
		value->connect(value, SIGNAL(clicked(bool)), d->saveTimer, SLOT(start()));
}

static void addLabels(QGridLayout *layout, int row, const QString labelText, QLabel *value)
{
	value->setTextFormat(Qt::PlainText);
	value->setTextInteractionFlags(Qt::TextBrowserInteraction);
	addWidgets(nullptr, layout, row, labelText, value);
}

ServerSummaryPage::ServerSummaryPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;

	d->saveTimer = new QTimer(this);
	d->saveTimer->setSingleShot(true);
	d->saveTimer->setInterval(1000);
	connect(d->saveTimer, &QTimer::timeout, this, &ServerSummaryPage::saveSettings);

	auto *layout = new QGridLayout;
	layout->setColumnStretch(0, 1);
	layout->setColumnStretch(1, 10);
	setLayout(layout);

	int row=0;

	layout->addWidget(new SubheaderWidget(tr("Server"), 1), row++, 0, 1,	2);

	if(server->isLocal())
		addLabels(layout, row++, tr("Status"), d->status);

	// General info about the server
	addLabels(layout, row++, tr("Address"), d->address);
	addLabels(layout, row++, tr("Port"), d->port);

	// Server start/config buttons
	if(server->isLocal()) {
		layout->addItem(new QSpacerItem(1,10), row++, 0);
		auto *buttons = new QHBoxLayout;

		d->startStopButton = new QPushButton;
		connect(d->startStopButton, &QPushButton::clicked, this, &ServerSummaryPage::startOrStopServer);
		buttons->addWidget(d->startStopButton);

		auto *settingsButton = new QPushButton(tr("Settings"));
		connect(settingsButton, &QPushButton::clicked, this, &ServerSummaryPage::showSettingsDialog);
		buttons->addWidget(settingsButton);

		buttons->addStretch(1);

		layout->addLayout(buttons, row++, 0, 1, 2);
	}

	layout->addItem(new QSpacerItem(1,10), row++, 0);


	// Serverwide settings that are adjustable via the API
	layout->addWidget(new SubheaderWidget(tr("Settings"), 2), row++, 0, 1,	2);

	addWidgets(d, layout, row++, tr("Connection timeout"), d->clientTimeout, true);
	addWidgets(d, layout, row++, QString(), d->allowGuests);
	addWidgets(d, layout, row++, QString(), d->allowGuestHosts);

	layout->addItem(new QSpacerItem(1,10), row++, 0);

	addWidgets(d, layout, row++, tr("Session size limit"), d->sessionSizeLimit, true);
	addWidgets(d, layout, row++, tr("Session idle timeout"), d->idleTimeout, true);
	addWidgets(d, layout, row++, tr("Maximum sessions"), d->maxSessions, true);
	addWidgets(d, layout, row++, QString(), d->persistence);
	addWidgets(d, layout, row++, QString(), d->archiveSessions);
	addWidgets(d, layout, row++, QString(), d->privateUserList);

	layout->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);

	if(d->server->isLocal()) {
		connect(static_cast<LocalServer*>(d->server), &LocalServer::serverStateChanged, this, &ServerSummaryPage::refreshPage);
	}

	connect(d->server, &Server::apiResponse, this, &ServerSummaryPage::handleResponse);

	refreshPage();
}

ServerSummaryPage::~ServerSummaryPage()
{
	if(d->saveTimer->isActive())
		saveSettings();
	delete d;
}

void ServerSummaryPage::startOrStopServer()
{
	Q_ASSERT(d->server->isLocal());
	auto server = static_cast<LocalServer*>(d->server);

	if(server->isRunning()) {
		server->stopServer();
	} else {
		server->startServer();
	}
	d->startStopButton->setEnabled(false);
}

void ServerSummaryPage::refreshPage()
{
	d->address->setText(d->server->address());
	d->port->setText(QString::number(d->server->port()));
	if(d->server->isLocal()) {
		const bool isRunning = static_cast<LocalServer*>(d->server)->isRunning();
		d->status->setText(isRunning ? tr("Started") : tr("Stopped"));

		d->startStopButton->setText(isRunning ? tr("Stop") : tr("Start"));
		d->startStopButton->setEnabled(true);

	}

	d->server->makeApiRequest(REQ_ID, JsonApiMethod::Get, QStringList() << "server", QJsonObject());
}

void ServerSummaryPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId != REQ_ID)
		return;
	const QJsonObject o = result.body.object();

	d->lastUpdate = o;

	d->clientTimeout->setValue(o[config::ClientTimeout.name].toDouble() / 60);
	d->allowGuests->setChecked(o[config::AllowGuests.name].toBool());
	d->allowGuestHosts->setChecked(o[config::AllowGuestHosts.name].toBool());

	d->sessionSizeLimit->setValue(o[config::SessionSizeLimit.name].toDouble() / (1024*1024));
	d->idleTimeout->setValue(o[config::IdleTimeLimit.name].toDouble() / 60);
	d->maxSessions->setValue(o[config::SessionCountLimit.name].toInt());
	d->persistence->setChecked(o[config::EnablePersistence.name].toBool());
	d->archiveSessions->setChecked(o[config::ArchiveMode.name].toBool());
	d->privateUserList->setChecked(o[config::PrivateUserList.name].toBool());
}

void ServerSummaryPage::saveSettings()
{
	QJsonObject o {
		{config::ClientTimeout.name, int(d->clientTimeout->value() * 60)},
		{config::AllowGuests.name, d->allowGuests->isChecked()},
		{config::AllowGuestHosts.name, d->allowGuestHosts->isChecked()},
		{config::SessionSizeLimit.name, d->sessionSizeLimit->value() * 1024 * 1024},
		{config::IdleTimeLimit.name, d->idleTimeout->value() * 60},
		{config::SessionCountLimit.name, d->maxSessions->value()},
		{config::EnablePersistence.name, d->persistence->isChecked()},
		{config::ArchiveMode.name, d->archiveSessions->isChecked()},
		{config::PrivateUserList.name, d->privateUserList->isChecked()}
	};

	QJsonObject update;
	for(auto i=o.constBegin();i!=o.constEnd();++i) {
		if(d->lastUpdate[i.key()] != i.value())
			update[i.key()] = i.value();
	}

	if(!update.isEmpty()) {
		d->lastUpdate = o;

		qDebug() << "update" << update;
		d->server->makeApiRequest(REQ_ID, JsonApiMethod::Update, QStringList() << "server", update);
	}
}

static void pickFilePath(QWidget *parent, QLineEdit *target)
{
	QFileDialog *dlg = new QFileDialog(parent, QString(), QDir(target->text()).absolutePath());
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	QObject::connect(dlg, &QFileDialog::fileSelected, target, &QLineEdit::setText);
	dlg->show();
}

void ServerSummaryPage::showSettingsDialog()
{
	Q_ASSERT(d->server->isLocal());

	// Show a dialog for changing the local server's settings.
	QDialog *dlg = new QDialog(parentWidget());
	Ui_SettingsDialog ui;
	ui.setupUi(dlg);

	ui.restartAlert->setVisible(static_cast<const LocalServer*>(d->server)->isRunning());

	connect(ui.pickCert, &QPushButton::clicked, std::bind(&pickFilePath, this, ui.certFile));
	connect(ui.pickKey, &QPushButton::clicked, std::bind(&pickFilePath, this, ui.keyFile));

	QSettings cfg;
	cfg.beginGroup("ui");

	ui.trayIcon->setChecked(cfg.value("trayicon", true).toBool());
	ui.trayIcon->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());

	cfg.endGroup();

	cfg.beginGroup("guiserver");
	if(cfg.value("session-storage", "file").toString() == "file")
		ui.storageFile->setChecked(true);
	else
		ui.storageMemory->setChecked(true);

	ui.port->setValue(cfg.value("port", 27750).toInt());
	ui.localAddress->setText(cfg.value("local-address").toString());

	if(cfg.value("use-ssl", false).toBool()) {
		if(cfg.value("force-ssl", false).toBool())
			ui.tlsRequired->setChecked(true);
		else
			ui.tlsOn->setChecked(true);
	} else {
		ui.tlsOff->setChecked(true);
	}
	ui.certFile->setText(cfg.value("sslcert").toString());
	ui.keyFile->setText(cfg.value("sslkey").toString());

	connect(dlg, &QDialog::accepted, this, [ui]() {
		QSettings cfg;
		cfg.setValue("ui/trayicon", ui.trayIcon->isChecked());
		cfg.beginGroup("guiserver");
		cfg.setValue("session-storage", ui.storageFile->isChecked() ? "file" : "memory");
		cfg.setValue("port", ui.port->value());
		cfg.setValue("local-address", ui.localAddress->text());
		cfg.setValue("use-ssl", !ui.tlsOff->isChecked());
		cfg.setValue("force-ssl", ui.tlsRequired->isChecked());
		cfg.setValue("sslcert", ui.certFile->text());
		cfg.setValue("sslkey", ui.keyFile->text());

		if(ui.trayIcon->isChecked())
			TrayIcon::showTrayIcon();
		else
			TrayIcon::hideTrayIcon();
	});

	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

}
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/serversummarypage.h"
#include "thinsrv/gui/subheaderwidget.h"
#include "thinsrv/gui/localserver.h"
#include "thinsrv/gui/trayicon.h"
#include "libserver/serverconfig.h"
#include "ui_settings.h"

#include <QDebug>
#include <QJsonObject>
#include <QGridLayout>
#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

	QLineEdit *serverTitle;
	QLineEdit *welcomeMessage;
	QDoubleSpinBox *clientTimeout;
	QCheckBox *allowGuests;
	QCheckBox *allowGuestHosts;

	QDoubleSpinBox *sessionSizeLimit;
	QDoubleSpinBox *autoresetTreshold;
	QDoubleSpinBox *idleTimeout;
	QSpinBox *maxSessions;
	QCheckBox *persistence;
	QCheckBox *privateUserList;
	QCheckBox *archiveSessions;
	QSpinBox *logPurge;

	QCheckBox *useExtAuth;
	QLineEdit *extAuthKey;
	QLineEdit *extAuthGroup;
	QCheckBox *extAuthFallback;
	QCheckBox *extAuthMod;
	QCheckBox *extAuthHost;

	QCheckBox *customAvatars;
	QCheckBox *extAuthAvatars;

	QPushButton *startStopButton;
	QJsonObject lastUpdate;
	QTimer *saveTimer;

	Private()
		:
		  status(new QLabel),
		  address(new QLabel),
		  port(new QLabel),
		  serverTitle(new QLineEdit),
		  welcomeMessage(new QLineEdit),
		  clientTimeout(new QDoubleSpinBox),
		  allowGuests(new QCheckBox),
		  allowGuestHosts(new QCheckBox),
		  sessionSizeLimit(new QDoubleSpinBox),
		  autoresetTreshold(new QDoubleSpinBox),
		  idleTimeout(new QDoubleSpinBox),
		  maxSessions(new QSpinBox),
		  persistence(new QCheckBox),
		  privateUserList(new QCheckBox),
		  archiveSessions(new QCheckBox),
		  logPurge(new QSpinBox),
		  useExtAuth(new QCheckBox),
		  extAuthKey(new QLineEdit),
		  extAuthGroup(new QLineEdit),
		  extAuthFallback(new QCheckBox),
		  extAuthMod(new QCheckBox),
		  extAuthHost(new QCheckBox),
		  customAvatars(new QCheckBox),
		  extAuthAvatars(new QCheckBox)
	{
		clientTimeout->setSuffix(" min");
		clientTimeout->setSingleStep(0.5);
		clientTimeout->setSpecialValueText(tr("unlimited"));
		allowGuests->setText(ServerSummaryPage::tr("Allow unauthenticated users"));
		allowGuestHosts->setText(ServerSummaryPage::tr("Allow anyone to host sessions"));

		sessionSizeLimit->setSuffix(" MB");
		sessionSizeLimit->setSpecialValueText(tr("unlimited"));
		autoresetTreshold->setSuffix(" MB");
		autoresetTreshold->setSpecialValueText(tr("none"));
		idleTimeout->setSuffix(" min");
		idleTimeout->setSingleStep(1);
		idleTimeout->setSpecialValueText(tr("unlimited"));
		idleTimeout->setMaximum(7 * 24 * 60);

		logPurge->setMinimum(0);
		logPurge->setMaximum(365*10);
		logPurge->setPrefix("purge older than ");
		logPurge->setSuffix(tr(" days"));
		logPurge->setSpecialValueText("keep all");

		persistence->setText(ServerSummaryPage::tr("Allow sessions to persist without users"));
		privateUserList->setText(ServerSummaryPage::tr("Do not include user list is session announcement"));
		archiveSessions->setText(ServerSummaryPage::tr("Archive terminated sessions"));

		customAvatars->setText(ServerSummaryPage::tr("Allow custom avatars"));

		useExtAuth->setText(ServerSummaryPage::tr("Enable"));
		extAuthFallback->setText(ServerSummaryPage::tr("Permit guest logins when ext-auth server is unreachable"));
		extAuthMod->setText(ServerSummaryPage::tr("Allow ext-auth moderators"));
		extAuthHost->setText(ServerSummaryPage::tr("Allow ext-auth hosts"));
		extAuthAvatars->setText(ServerSummaryPage::tr("Use ext-auth avatars"));
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
	else if(value->inherits("QLineEdit"))
		value->connect(value, SIGNAL(textEdited(QString)), d->saveTimer, SLOT(start()));
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

	addWidgets(d, layout, row++, tr("Server Title"), d->serverTitle);
	addWidgets(d, layout, row++, tr("Welcome Message"), d->welcomeMessage);

	addWidgets(d, layout, row++, tr("Connection timeout"), d->clientTimeout, true);
	addWidgets(d, layout, row++, QString(), d->allowGuests);
	addWidgets(d, layout, row++, QString(), d->allowGuestHosts);
	addWidgets(d, layout, row++, tr("Server log"), d->logPurge, true);

	layout->addItem(new QSpacerItem(1,10), row++, 0);

	addWidgets(d, layout, row++, tr("Session size limit"), d->sessionSizeLimit, true);
	addWidgets(d, layout, row++, tr("Default autoreset threshold"), d->autoresetTreshold, true);
	addWidgets(d, layout, row++, tr("Session idle timeout"), d->idleTimeout, true);
	addWidgets(d, layout, row++, tr("Maximum sessions"), d->maxSessions, true);
	addWidgets(d, layout, row++, QString(), d->persistence);
	addWidgets(d, layout, row++, QString(), d->archiveSessions);
	addWidgets(d, layout, row++, QString(), d->privateUserList);
	addWidgets(d, layout, row++, QString(), d->customAvatars);

	layout->addItem(new QSpacerItem(1,10), row++, 0);

	addWidgets(d, layout, row++, tr("External authentication"), d->useExtAuth);
	addWidgets(d, layout, row++, tr("Validation key"), d->extAuthKey);
	addWidgets(d, layout, row++, tr("User group"), d->extAuthGroup, true);
	addWidgets(d, layout, row++, QString(), d->extAuthFallback);
	addWidgets(d, layout, row++, QString(), d->extAuthMod);
	addWidgets(d, layout, row++, QString(), d->extAuthHost);
	addWidgets(d, layout, row++, QString(), d->extAuthAvatars);


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

	d->serverTitle->setText(o[config::ServerTitle.name].toString());
	d->welcomeMessage->setText(o[config::WelcomeMessage.name].toString());
	d->clientTimeout->setValue(o[config::ClientTimeout.name].toDouble() / 60);
	d->allowGuests->setChecked(o[config::AllowGuests.name].toBool());
	d->allowGuestHosts->setChecked(o[config::AllowGuestHosts.name].toBool());

	d->sessionSizeLimit->setValue(o[config::SessionSizeLimit.name].toDouble() / (1024*1024));
	d->autoresetTreshold->setValue(o[config::AutoresetThreshold.name].toDouble() / (1024*1024));
	d->idleTimeout->setValue(o[config::IdleTimeLimit.name].toDouble() / 60);
	d->maxSessions->setValue(o[config::SessionCountLimit.name].toInt());
	d->logPurge->setValue(o[config::LogPurgeDays.name].toInt());
	d->persistence->setChecked(o[config::EnablePersistence.name].toBool());
	d->archiveSessions->setChecked(o[config::ArchiveMode.name].toBool());
	d->privateUserList->setChecked(o[config::PrivateUserList.name].toBool());
	d->customAvatars->setChecked(o[config::AllowCustomAvatars.name].toBool());

	d->useExtAuth->setChecked(o[config::UseExtAuth.name].toBool());
	d->extAuthKey->setText(o[config::ExtAuthKey.name].toString());
	d->extAuthGroup->setText(o[config::ExtAuthGroup.name].toString());
	d->extAuthFallback->setChecked(o[config::ExtAuthFallback.name].toBool());
	d->extAuthMod->setChecked(o[config::ExtAuthMod.name].toBool());
	d->extAuthHost->setChecked(o[config::ExtAuthHost.name].toBool());
	d->extAuthAvatars->setChecked(o[config::ExtAuthAvatars.name].toBool());
	const bool supportsExtAuth = o.contains(config::UseExtAuth.name);
	d->useExtAuth->setEnabled(supportsExtAuth);
	d->extAuthGroup->setEnabled(supportsExtAuth);
	d->extAuthFallback->setEnabled(supportsExtAuth);
	d->extAuthMod->setEnabled(supportsExtAuth);
	d->extAuthHost->setEnabled(supportsExtAuth);
	d->extAuthAvatars->setEnabled(supportsExtAuth);
}

void ServerSummaryPage::saveSettings()
{
	QJsonObject o {
		{config::ServerTitle.name, d->serverTitle->text()},
		{config::WelcomeMessage.name, d->welcomeMessage->text()},
		{config::ClientTimeout.name, int(d->clientTimeout->value() * 60)},
		{config::AllowGuests.name, d->allowGuests->isChecked()},
		{config::AllowGuestHosts.name, d->allowGuestHosts->isChecked()},
		{config::SessionSizeLimit.name, d->sessionSizeLimit->value() * 1024 * 1024},
		{config::AutoresetThreshold.name, d->autoresetTreshold->value() * 1024 * 1024},
		{config::IdleTimeLimit.name, d->idleTimeout->value() * 60},
		{config::SessionCountLimit.name, d->maxSessions->value()},
		{config::LogPurgeDays.name, d->logPurge->value()},
		{config::EnablePersistence.name, d->persistence->isChecked()},
		{config::ArchiveMode.name, d->archiveSessions->isChecked()},
		{config::PrivateUserList.name, d->privateUserList->isChecked()},
		{config::AllowCustomAvatars.name, d->customAvatars->isChecked()},
		{config::UseExtAuth.name, d->useExtAuth->isChecked()},
		{config::ExtAuthKey.name, d->extAuthKey->text()},
		{config::ExtAuthGroup.name, d->extAuthGroup->text()},
		{config::ExtAuthFallback.name, d->extAuthFallback->isChecked()},
		{config::ExtAuthMod.name, d->extAuthMod->isChecked()},
		{config::ExtAuthHost.name, d->extAuthHost->isChecked()},
		{config::ExtAuthAvatars.name, d->extAuthAvatars->isChecked()}
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

	{
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

	#ifdef HAVE_LIBSODIUM
		ui.extAuthUrl->setText(cfg.value("extauth").toString());
	#else
		ui.extAuthUrl->setEnabled(false);
	#endif

		ui.enableTls->setChecked(cfg.value("use-ssl", false).toBool());
		ui.certFile->setText(cfg.value("sslcert").toString());
		ui.keyFile->setText(cfg.value("sslkey").toString());
	}

	connect(dlg, &QDialog::accepted, this, [ui]() {
		QSettings cfg;
		cfg.setValue("ui/trayicon", ui.trayIcon->isChecked());
		cfg.beginGroup("guiserver");
		cfg.setValue("session-storage", ui.storageFile->isChecked() ? "file" : "memory");
		cfg.setValue("port", ui.port->value());
		cfg.setValue("local-address", ui.localAddress->text());
		cfg.setValue("extauth", ui.extAuthUrl->text());
		cfg.setValue("use-ssl", ui.enableTls->isChecked());
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

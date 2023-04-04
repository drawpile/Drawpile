// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/logindialog.h"
#include "desktop/dialogs/abusereport.h"
#include "desktop/dialogs/certificateview.h"
#include "libclient/net/login.h"
#include "libclient/net/loginsessions.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/utils/usernamevalidator.h"
#include "libclient/utils/html.h"
#include "libclient/utils/icon.h"

#include "ui_logindialog.h"

#ifdef HAVE_QTKEYCHAIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <qt6keychain/keychain.h>
#else
#include <qt5keychain/keychain.h>
#endif
#endif

#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QStyle>
#include <QItemSelectionModel>
#include <QAction>
#include <QPointer>

namespace dialogs {

enum class Mode {
	loading,         // used whenever we're waiting for the server
	identity,        // ask user for username
	authenticate,    // ask user for password (for login)
	sessionlist,     // select session to join
	sessionpassword, // ask user for password (for session)
	catchup,         // logged in: catching up (dialog can be closed at this point)
	certChanged      // SSL certificate has changed (can be ignored)
};

struct LoginDialog::Private {
	Mode mode;

	QPointer<net::LoginHandler> loginHandler;
	AvatarListModel *avatars;
	SessionFilterProxyModel *sessions;
	Ui_LoginDialog *ui;

	QPushButton *okButton;
	QPushButton *reportButton;

	QUrl extauthurl;
	QSslCertificate oldCert, newCert;

	QMetaObject::Connection loginDestructConnection;

	Private(net::LoginHandler *login, LoginDialog *dlg)
		: mode(Mode::loading), loginHandler(login),
		  ui(new Ui_LoginDialog)
	{
		Q_ASSERT(loginHandler);

		ui->setupUi(dlg);

		ui->serverTitle->setVisible(false);

		// Identity & authentication page
		ui->username->setValidator(new UsernameValidator(dlg));
		avatars = new AvatarListModel(dlg);
		avatars->loadAvatars(true);
		ui->avatarList->setModel(avatars);

#ifndef HAVE_QTKEYCHAIN
		ui->rememberPassword->setEnabled(false);
#endif

#ifdef Q_OS_MACOS
		// The avatar selection combobox looks terrible on macOS
		// With this style, it looks slightly less terrible.
		ui->avatarList->setStyleSheet(
			"QComboBox {"
				"border: none;"
				"background: transparent;"
			"}"
			"QComboBox:hover {"
				"border: 1px dotted black;"
			"}"
			"QComboBox::drop-down {"
				"background: transparent;"
			"}"
		);
#endif

		ui->usernameIcon->setText(QString());
		ui->usernameIcon->setPixmap(icon::fromTheme("im-user").pixmap(22, 22));

		ui->passwordIcon->setText(QString());
		ui->passwordIcon->setPixmap(icon::fromTheme("object-locked").pixmap(22, 22));

		// Session list page
		QObject::connect(ui->sessionList, &QTableView::doubleClicked, [this](const QModelIndex&) {
			if(okButton->isEnabled())
				okButton->click();
		});

		ui->showNsfw->setEnabled(parentalcontrols::level() == parentalcontrols::Level::Unrestricted);
		sessions = new SessionFilterProxyModel(dlg);
		sessions->setFilterCaseSensitivity(Qt::CaseInsensitive);
		sessions->setFilterKeyColumn(-1);
		sessions->setShowNsfw(false);

		connect(ui->showNsfw, &QAbstractButton::toggled, [this](bool show) {
			QSettings().setValue("history/filternsfw", show);
			sessions->setShowNsfw(show);
		});
        connect(ui->filter, &QLineEdit::textChanged,
                        sessions, &SessionFilterProxyModel::setFilterFixedString);

		ui->sessionList->setModel(sessions);

		// Cert changed page
		ui->warningIcon->setText(QString());
		ui->warningIcon->setPixmap(dlg->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(64, 64));

		// Buttons
		okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
		okButton->setDefault(true);

		reportButton = ui->buttonBox->addButton(LoginDialog::tr("Report..."), QDialogButtonBox::ActionRole);
		reportButton->setEnabled(false); // needs a selected session to be enabled

		resetMode(Mode::loading);
	}

	~Private() {
		delete ui;
	}

	void resetMode(Mode mode);
	void setLoginMode(const QString &prompt);
};

void LoginDialog::Private::resetMode(Mode newMode)
{
	if(!loginHandler) {
		qWarning("LoginDialog::resetMode: login process already ended!");
		return;
	}

	mode = newMode;

	QWidget *page = nullptr;

	okButton->setVisible(true);
	reportButton->setVisible(false);

	switch(mode) {
	case Mode::loading:
		okButton->setVisible(false);
		ui->loginPromptLabel->setText(loginHandler->url().host());
		page = ui->loadingPage;
		break;
	case Mode::identity:
		ui->avatarList->setEnabled(true);
		ui->username->setEnabled(true);
		ui->username->setFocus();
		ui->password->setVisible(false);
		ui->passwordIcon->setVisible(false);
		ui->badPasswordLabel->setVisible(false);
		ui->rememberPassword->setVisible(false);
		page = ui->authPage;
		break;
	case Mode::authenticate:
		ui->avatarList->setEnabled(false);
		ui->username->setEnabled(false);
		ui->password->setVisible(true);
		ui->passwordIcon->setVisible(true);
		ui->badPasswordLabel->setVisible(false);
		ui->rememberPassword->setVisible(true);
		ui->password->setFocus();
		page = ui->authPage;
		break;
	case Mode::sessionlist:
		reportButton->setVisible(true);
		page = ui->listingPage;
		break;
	case Mode::sessionpassword:
		ui->sessionPassword->setFocus();
		page = ui->sessionPasswordPage;
		break;
	case Mode::catchup:
		okButton->setVisible(false);
		ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(LoginDialog::tr("Close"));
		page = ui->catchupPage;
		break;
	case Mode::certChanged:
		page = ui->certChangedPage;
		break;
	}

	Q_ASSERT(page);
	ui->pages->setCurrentWidget(page);
}

#ifdef HAVE_QTKEYCHAIN
static const QString KEYCHAIN_NAME = QStringLiteral("Drawpile");

static QString keychainSecretName(const QString &username, const QUrl &extAuthUrl, const QString &server)
{
	QString prefix, host;
	if(extAuthUrl.isValid()) {
		prefix = "ext:";
		host = extAuthUrl.host();
	} else {
		prefix = "srv:";
		host = server;
	}

	return prefix + username.toLower() + "@" + host;
}
#endif


LoginDialog::LoginDialog(net::LoginHandler *login, QWidget *parent) :
	QDialog(parent), d(new Private(login, this))
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(login->url().host());

	connect(d->ui->username, &QLineEdit::textChanged, this, &LoginDialog::updateOkButtonEnabled);
	connect(d->ui->password, &QLineEdit::textChanged, this, &LoginDialog::updateOkButtonEnabled);
	connect(d->ui->sessionPassword, &QLineEdit::textChanged, this, &LoginDialog::updateOkButtonEnabled);
	connect(d->ui->sessionList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &LoginDialog::updateOkButtonEnabled);
	connect(d->ui->replaceCert, &QAbstractButton::toggled, this, &LoginDialog::updateOkButtonEnabled);

	connect(d->okButton, &QPushButton::clicked, this, &LoginDialog::onOkClicked);
	connect(d->reportButton, &QPushButton::clicked, this, &LoginDialog::onReportClicked);
	connect(this, &QDialog::rejected, login, &net::LoginHandler::cancelLogin);

	connect(d->ui->viewOldCert, &QPushButton::clicked, this, &LoginDialog::showOldCert);
	connect(d->ui->viewNewCert, &QPushButton::clicked, this, &LoginDialog::showNewCert);

	d->loginDestructConnection = connect(login, &net::LoginHandler::destroyed, this, &LoginDialog::deleteLater);

	connect(login, &net::LoginHandler::usernameNeeded, this, &LoginDialog::onUsernameNeeded);
	connect(login, &net::LoginHandler::loginNeeded, this, &LoginDialog::onLoginNeeded);
	connect(login, &net::LoginHandler::extAuthNeeded, this, &LoginDialog::onExtAuthNeeded);
	connect(login, &net::LoginHandler::sessionPasswordNeeded, this, &LoginDialog::onSessionPasswordNeeded);
	connect(login, &net::LoginHandler::loginOk, this, &LoginDialog::onLoginOk);
	connect(login, &net::LoginHandler::badLoginPassword, this, &LoginDialog::onBadLoginPassword);
	connect(login, &net::LoginHandler::extAuthComplete, this, &LoginDialog::onExtAuthComplete);
	connect(login, &net::LoginHandler::sessionChoiceNeeded, this, &LoginDialog::onSessionChoiceNeeded);
	connect(login, &net::LoginHandler::certificateCheckNeeded, this, &LoginDialog::onCertificateCheckNeeded);
	connect(login, &net::LoginHandler::serverTitleChanged, this, &LoginDialog::onServerTitleChanged);
}

LoginDialog::~LoginDialog()
{
	delete d;
}

void LoginDialog::updateOkButtonEnabled()
{
	bool enabled = false;
	switch(d->mode) {
	case Mode::loading:
	case Mode::catchup:
		break;
	case Mode::identity:
		enabled = UsernameValidator::isValid(d->ui->username->text());
		break;
	case Mode::authenticate:
		enabled = !d->ui->password->text().isEmpty();
		break;
	case Mode::sessionpassword:
		enabled = !d->ui->sessionPassword->text().isEmpty();
		break;
	case Mode::sessionlist: {
		QModelIndexList sel = d->ui->sessionList->selectionModel()->selectedIndexes();
		if(sel.isEmpty())
			enabled = false;
		else
			enabled = sel.first().data(net::LoginSessionModel::JoinableRole).toBool();

		d->reportButton->setEnabled(!sel.isEmpty() && d->loginHandler && d->loginHandler->supportsAbuseReports());
		break; }
	case Mode::certChanged:
		enabled = d->ui->replaceCert->isChecked();
		break;
	}

	d->okButton->setEnabled(enabled);
}

void LoginDialog::showOldCert()
{
	auto dlg = new CertificateView(d->loginHandler ? d->loginHandler->url().host() : QString(), d->oldCert);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void LoginDialog::showNewCert()
{
	auto dlg = new CertificateView(d->loginHandler ? d->loginHandler->url().host() : QString(), d->newCert);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void LoginDialog::onUsernameNeeded(bool canSelectAvatar)
{
	QSettings cfg;
	d->ui->username->setText(cfg.value("history/username").toString());

	if(canSelectAvatar && d->avatars->rowCount() > 1) {
		d->ui->avatarList->show();
		const QString avatar = cfg.value("history/avatar").toString();
		if(avatar.isEmpty())
			d->ui->avatarList->setCurrentIndex(0);
		else
			d->ui->avatarList->setCurrentIndex(d->avatars->getAvatar(avatar).row());
	} else {
		d->ui->avatarList->hide();
	}

	d->resetMode(Mode::identity);
	updateOkButtonEnabled();
}

void LoginDialog::Private::setLoginMode(const QString &prompt)
{
	ui->loginPromptLabel->setText(prompt);
	if(extauthurl.isValid())
		ui->loginPromptLabel->setStyleSheet(QStringLiteral(
			"background: #3498db;"
			"color: #fcfcfc;"
			"padding: 16px"
			));
	else
		ui->loginPromptLabel->setStyleSheet(QStringLiteral(
			"background: #fdbc4b;"
			"color: #31363b;"
			"padding: 16px"
			));

	resetMode(Mode::authenticate);

#ifdef HAVE_QTKEYCHAIN
	if(!loginHandler) {
		qWarning("LoginDialog::setLoginMode: login process already ended!");
		return;
	}

	auto *readJob = new QKeychain::ReadPasswordJob(KEYCHAIN_NAME);
	readJob->setInsecureFallback(QSettings().value("settings/insecurepasswordstorage", false).toBool());
	readJob->setKey(
		keychainSecretName(
			loginHandler->url().userName(),
			extauthurl,
			loginHandler->url().host()
		)
	);

	connect(readJob, &QKeychain::ReadPasswordJob::finished, okButton, [this, readJob]() {
		if(readJob->error() != QKeychain::NoError) {
			qWarning("Keychain error (key=%s): %s",
				qPrintable(readJob->key()),
				qPrintable(readJob->errorString())
			);
			return;
		}

		if(mode != Mode::authenticate) {
			// Unlikely, but...
			qWarning("Keychain returned too late!");
			return;
		}

		const QString password = readJob->textData();
		if(!password.isEmpty()) {
			ui->password->setText(password);
			okButton->click();
		}
	});

	readJob->start();
#endif
}

void LoginDialog::onLoginNeeded(const QString &forUsername, const QString &prompt)
{
	if(!forUsername.isEmpty())
		d->ui->username->setText(forUsername);

	d->extauthurl = QUrl();
	d->setLoginMode(prompt);
}

void LoginDialog::onExtAuthNeeded(const QString &forUsername, const QUrl &url)
{
	Q_ASSERT(url.isValid());

	if(!forUsername.isEmpty())
		d->ui->username->setText(forUsername);

	QString prompt = tr("Log in with %1 credentials").arg("<i>" + url.host() + "</i>");
	if(url.scheme() != "https")
		prompt += " (INSECURE CONNECTION!)";

	d->extauthurl = url;
	d->setLoginMode(prompt);
}

void LoginDialog::onExtAuthComplete(bool success)
{
	if(!success) {
		onBadLoginPassword();
	}

	// If success == true, onLoginOk is called too
}

void LoginDialog::onLoginOk()
{
	if(d->ui->rememberPassword->isChecked()) {
#ifdef HAVE_QTKEYCHAIN
		auto *writeJob = new QKeychain::WritePasswordJob(KEYCHAIN_NAME);
		writeJob->setInsecureFallback(QSettings().value("settings/insecurepasswordstorage", false).toBool());
		writeJob->setKey(
			keychainSecretName(
				d->loginHandler->url().userName(),
				d->extauthurl,
				d->loginHandler->url().host()
			)
		);
		writeJob->setTextData(d->ui->password->text());
		writeJob->start();
#endif
	}
}

void LoginDialog::onBadLoginPassword()
{
	d->resetMode(Mode::authenticate);
	d->ui->password->setText(QString());

#ifdef HAVE_QTKEYCHAIN
	auto *deleteJob = new QKeychain::DeletePasswordJob(KEYCHAIN_NAME);
	deleteJob->setInsecureFallback(QSettings().value("settings/insecurepasswordstorage", false).toBool());
	deleteJob->setKey(
		keychainSecretName(
			d->loginHandler->url().userName(),
			d->extauthurl,
			d->loginHandler->url().host()
		)
	);
	deleteJob->start();
#endif

	d->ui->badPasswordLabel->show();
	QTimer::singleShot(2000, d->ui->badPasswordLabel, &QLabel::hide);
}

void LoginDialog::onSessionChoiceNeeded(net::LoginSessionModel *sessions)
{
	QRect geom = geometry();
	int newWidth = qMax(geom.width(), 600);
	int newHeight = qMax(geom.height(), 400);
	int newX = geom.x() + (geom.width() - newWidth) / 2;
	int newY = geom.y() + (geom.height() - newHeight) / 2;
	setGeometry(newX, newY, newWidth, newHeight);

	if(d->ui->showNsfw->isEnabled())
		d->ui->showNsfw->setChecked(QSettings().value("history/filternsfw").toBool());

	d->sessions->setSourceModel(sessions);
	d->ui->sessionList->resizeColumnsToContents();
	d->ui->sessionList->sortByColumn(
		net::LoginSessionModel::ColumnTitle, Qt::AscendingOrder);

	QHeaderView *header = d->ui->sessionList->horizontalHeader();
	header->setSectionResizeMode(net::LoginSessionModel::ColumnTitle, QHeaderView::Stretch);
	header->setSectionResizeMode(net::LoginSessionModel::ColumnFounder, QHeaderView::ResizeToContents);

	d->resetMode(Mode::sessionlist);
	updateOkButtonEnabled();
}

void LoginDialog::onSessionPasswordNeeded()
{
	d->ui->sessionPassword->setText(QString());
	d->resetMode(Mode::sessionpassword);
}

void LoginDialog::onCertificateCheckNeeded(const QSslCertificate &newCert, const QSslCertificate &oldCert)
{
	d->oldCert = oldCert;
	d->newCert = newCert;
	d->ui->replaceCert->setChecked(false);
	d->okButton->setEnabled(false);
	d->resetMode(Mode::certChanged);
}

void LoginDialog::onLoginDone(bool join)
{
	if(join) {
		// Show catchup progress page when joining
		// Login process is now complete and the login handler will
		// self-destruct. But we can keep the login dialog open and show
		// the catchup progress bar until fully caught up or the user
		// manually closes the dialog.
		if(d->loginHandler)
			disconnect(d->loginDestructConnection);
		d->resetMode(Mode::catchup);
	}

}

void LoginDialog::onServerTitleChanged(const QString &title)
{
	d->ui->serverTitle->setText(htmlutils::newlineToBr(htmlutils::linkify(title.toHtmlEscaped())));
	d->ui->serverTitle->setHidden(title.isEmpty());
}

void LoginDialog::onOkClicked()
{
	if(!d->loginHandler) {
		qWarning("LoginDialog::onOkClicked: login process already ended!");
		return;
	}

	const Mode mode = d->mode;
	d->resetMode(Mode::loading);

	switch(mode) {
	case Mode::loading:
	case Mode::catchup:
		// No OK button in these modes
		qWarning("OK button click in wrong mode!");
		break;
	case Mode::identity: {
		const QPixmap avatar = d->ui->avatarList->currentData(Qt::DecorationRole).value<QPixmap>();
		const QString avatarFile = avatar.isNull() ? QString() : d->ui->avatarList->currentData(AvatarListModel::FilenameRole).toString();

		QSettings cfg;
		cfg.setValue("history/username", d->ui->username->text());
		cfg.setValue("history/avatar", avatarFile);

		if(!avatar.isNull())
			d->loginHandler->selectAvatar(avatar.toImage());

		d->loginHandler->selectIdentity(d->ui->username->text(), QString());
		break; }
	case Mode::authenticate:
		if(d->extauthurl.isValid()) {
			d->loginHandler->requestExtAuth(d->ui->username->text(), d->ui->password->text());
		} else {
			d->loginHandler->selectIdentity(d->ui->username->text(), d->ui->password->text());
		}
		break;
	case Mode::sessionlist: {
		if(d->ui->sessionList->selectionModel()->selectedIndexes().isEmpty()) {
			qWarning("Ok clicked but no session selected!");
			return;
		}

		const QModelIndex i = d->ui->sessionList->selectionModel()->selectedIndexes().first();
		d->loginHandler->joinSelectedSession(
			i.data(net::LoginSessionModel::AliasOrIdRole).toString(),
			i.data(net::LoginSessionModel::NeedPasswordRole).toBool()
		);
		break;
		}
	case Mode::sessionpassword:
		d->loginHandler->sendSessionPassword(d->ui->sessionPassword->text());
		break;
	case Mode::certChanged:
		d->loginHandler->acceptServerCertificate();
		break;
	}
}

void LoginDialog::onReportClicked()
{
	if(d->ui->sessionList->selectionModel()->selectedIndexes().isEmpty()) {
		qWarning("Cannot open report dialog: no session selected!");
		return;
	}

	const QModelIndex idx = d->ui->sessionList->selectionModel()->selectedIndexes().first();

	AbuseReportDialog *reportDlg = new AbuseReportDialog(this);
	reportDlg->setAttribute(Qt::WA_DeleteOnClose);

	const QString sessionId = idx.data(net::LoginSessionModel::IdRole).toString();
	const QString sessionAlias = idx.data(net::LoginSessionModel::IdAliasRole).toString();
	const QString sessionTitle = idx.data(net::LoginSessionModel::TitleRole).toString();

	reportDlg->setSessionInfo(sessionId, sessionAlias, sessionTitle);

	connect(reportDlg, &AbuseReportDialog::accepted, this, [this, sessionId, reportDlg]() {
		if(!d->loginHandler) {
			qWarning("LoginDialog abuse report: login process already ended!");
			return;
		}
		d->loginHandler->reportSession(sessionId, reportDlg->message());
	});

	reportDlg->show();
}

void LoginDialog::catchupProgress(int value)
{
	d->ui->progressBar->setMaximum(100);
	d->ui->progressBar->setValue(value);
	if(d->mode == Mode::catchup && value >= 100)
		this->deleteLater();
}

}

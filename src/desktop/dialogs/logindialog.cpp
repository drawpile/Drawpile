// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/logindialog.h"
#include "desktop/dialogs/abusereport.h"
#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/loginsessions.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/html.h"
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/utils/usernamevalidator.h"
#include "ui_logindialog.h"
#include <QAction>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#ifdef HAVE_QTKEYCHAIN
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#		include <qt6keychain/keychain.h>
#	else
#		include <qt5keychain/keychain.h>
#	endif
#endif

namespace dialogs {

enum class Mode {
	Loading,		 // used whenever we're waiting for the server
	LoginMethod,	 // ask the user to pick a login method
	GuestLogin,		 // log in with guest username
	AuthLogin,		 // log in with internal account
	ExtAuthLogin,	 // log in with external account
	Identity,		 // (old login flow) ask user for username
	Authenticate,	 // (old login flow) ask user for password
	SessionList,	 // select session to join
	SessionPassword, // ask user for password (for session)
	Catchup,	 // logged in: catching up (dialog can be closed at this point)
	CertChanged, // SSL certificate has changed (can be ignored)
	ConfirmNsfm, // confirm NSFM session join
};

struct LoginDialog::Private {
	Mode mode;

	QPointer<net::LoginHandler> loginHandler;
	AvatarListModel *avatars;
	SessionFilterProxyModel *sessions;
	Ui_LoginDialog *ui;

	QPushButton *okButton;
	QPushButton *cancelButton;
	QPushButton *reportButton;
	QPushButton *yesButton;
	QPushButton *noButton;
	QString originalNoButtonText;

	QUrl loginExtAuthUrl;
	QUrl extauthurl;
	QSslCertificate oldCert, newCert;
	bool autoJoin;

	QMetaObject::Connection loginDestructConnection;

	Private(net::LoginHandler *login, LoginDialog *dlg)
		: mode(Mode::Loading)
		, loginHandler(login)
		, ui(new Ui_LoginDialog)
	{
		Q_ASSERT(loginHandler);

		ui->setupUi(dlg);

		ui->serverTitle->setVisible(false);

		// Identity & authentication page
		ui->username->setValidator(new UsernameValidator(dlg));
		avatars = new AvatarListModel(false, dlg);
		avatars->loadAvatars(true, true);
		ui->avatarList->setModel(avatars);

#ifndef HAVE_QTKEYCHAIN
		ui->rememberPassword->setEnabled(false);
#endif

		ui->usernameIcon->setText(QString());
		ui->usernameIcon->setPixmap(QIcon::fromTheme("im-user").pixmap(22, 22));

		ui->passwordIcon->setText(QString());
		ui->passwordIcon->setPixmap(
			QIcon::fromTheme("object-locked").pixmap(22, 22));

		// Session list page
		utils::initKineticScrolling(ui->sessionList);
		QObject::connect(
			ui->sessionList, &QTableView::doubleClicked,
			[this](const QModelIndex &) {
				if(okButton->isEnabled())
					okButton->click();
			});

		ui->showNsfw->setEnabled(
			parentalcontrols::level() == parentalcontrols::Level::Unrestricted);
		sessions = new SessionFilterProxyModel(dlg);
		sessions->setFilterCaseSensitivity(Qt::CaseInsensitive);
		sessions->setFilterKeyColumn(-1);

		auto &settings = dpApp().settings();
		settings.bindFilterNsfm(ui->showNsfw);
		settings.bindFilterNsfm(
			sessions, &SessionFilterProxyModel::setShowNsfw);

		settings.bindLastUsername(ui->username);
		revertAvatar(settings);

		if(settings.parentalControlsLocked().isEmpty()) {
			settings.bindShowNsfmWarningOnJoin(ui->nsfmConfirmAgainBox);
		} else {
			ui->nsfmConfirmAgainBox->hide();
		}

		connect(
			ui->filter, &QLineEdit::textChanged, sessions,
			&SessionFilterProxyModel::setFilterFixedString);

		ui->sessionList->setModel(sessions);

		// Cert changed page
		ui->warningIcon->setText(QString());
		ui->warningIcon->setPixmap(
			dlg->style()
				->standardIcon(QStyle::SP_MessageBoxWarning)
				.pixmap(64, 64));

		// Buttons
		okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
		okButton->setDefault(true);

		cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
		yesButton = ui->buttonBox->button(QDialogButtonBox::Yes);
		noButton = ui->buttonBox->button(QDialogButtonBox::No);
		originalNoButtonText = noButton->text();

		reportButton = ui->buttonBox->addButton(
			LoginDialog::tr("Report..."), QDialogButtonBox::ActionRole);
		reportButton->setEnabled(
			false); // needs a selected session to be enabled

		resetMode(Mode::Loading);
	}

	~Private() { delete ui; }

	void revertAvatar(desktop::settings::Settings &settings)
	{
		QModelIndex lastAvatarIndex = avatars->getAvatar(settings.lastAvatar());
		ui->avatarList->setCurrentIndex(
			lastAvatarIndex.isValid() ? lastAvatarIndex.row() : 0);
	}

	void resetMode(Mode mode);
	QWidget *setupAuthPage(bool usernameEnabled, bool passwordVisible);
	void setLoginExplanation(const QString &explanation, bool isError);
	void setLoginMode(const QString &prompt);
	void applyRememberedPassword();
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
	cancelButton->setVisible(true);
	reportButton->setVisible(false);
	yesButton->setVisible(false);

	switch(mode) {
	case Mode::Loading: {
		okButton->setVisible(false);
		QString host = loginHandler->url().host();
		ui->loginPromptLabel->setText(host);
		ui->authModePromptLabel->setText(host);
		page = ui->loadingPage;
		break;
	}
	case Mode::LoginMethod:
		page = ui->authModePage;
		okButton->setVisible(false);
		break;
	case Mode::GuestLogin:
		extauthurl.clear();
		page = setupAuthPage(true, false);
		break;
	case Mode::AuthLogin:
		extauthurl.clear();
		page = setupAuthPage(true, true);
		setLoginMode(tr("Log in with server account"));
		break;
	case Mode::ExtAuthLogin:
		extauthurl = loginExtAuthUrl;
		page = setupAuthPage(true, true);
		setLoginMode(formatExtAuthPrompt(extauthurl));
		break;
	case Mode::Identity:
		page = setupAuthPage(true, false);
		setLoginExplanation(QString(), false);
		break;
	case Mode::Authenticate:
		page = setupAuthPage(false, true);
		setLoginExplanation(
			tr("This username belongs to a registered account. If this isn't "
			   "your account, cancel and try again with a different name."),
			false);
		break;
	case Mode::SessionList:
		reportButton->setVisible(true);
		page = ui->listingPage;
		break;
	case Mode::SessionPassword:
		ui->sessionPassword->setFocus();
		ui->badSessionPasswordLabel->hide();
		page = ui->sessionPasswordPage;
		break;
	case Mode::Catchup:
		okButton->setVisible(false);
		ui->buttonBox->button(QDialogButtonBox::Cancel)
			->setText(LoginDialog::tr("Close"));
		page = ui->catchupPage;
		break;
	case Mode::CertChanged:
		page = ui->certChangedPage;
		break;
	case Mode::ConfirmNsfm:
		okButton->setVisible(false);
		cancelButton->setVisible(false);
		yesButton->setVisible(true);
		page = ui->nsfmConfirmPage;
		break;
	}

	switch(mode) {
	case Mode::GuestLogin:
	case Mode::AuthLogin:
	case Mode::ExtAuthLogin:
		noButton->setText(tr("Back"));
		noButton->setVisible(true);
		break;
	case Mode::ConfirmNsfm:
		noButton->setText(originalNoButtonText);
		noButton->setVisible(true);
		break;
	default:
		noButton->setVisible(false);
	}

	Q_ASSERT(page);
	ui->pages->setCurrentWidget(page);
}

QWidget *
LoginDialog::Private::setupAuthPage(bool usernameEnabled, bool passwordVisible)
{
	ui->avatarList->setEnabled(true);
	ui->username->setEnabled(usernameEnabled);
	ui->password->setVisible(passwordVisible);
	ui->passwordIcon->setVisible(passwordVisible);
	ui->badPasswordLabel->setVisible(false);
	ui->rememberPassword->setVisible(passwordVisible);
	if(passwordVisible &&
	   (!usernameEnabled || !ui->username->text().isEmpty())) {
		ui->password->setFocus();
	} else if(usernameEnabled) {
		ui->username->setFocus();
	}
	return ui->authPage;
}

void LoginDialog::Private::setLoginExplanation(
	const QString &explanation, bool isError)
{
	QString content;
	if(!explanation.isEmpty()) {
		content = explanation.toHtmlEscaped();
		if(isError) {
			content = QStringLiteral("<em>%1</em>").arg(content);
		}
	}
	ui->authExplanationLabel->setHidden(content.isEmpty());
	ui->authExplanationLabel->setText(content);
}

#ifdef HAVE_QTKEYCHAIN
static const QString KEYCHAIN_NAME = QStringLiteral("Drawpile");

static QString keychainSecretName(
	const QString &username, const QUrl &extAuthUrl, const QString &server)
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


LoginDialog::LoginDialog(net::LoginHandler *login, QWidget *parent)
	: QDialog(parent)
	, d(new Private(login, this))
{
	setWindowModality(Qt::WindowModal);
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(login->url().host());

	connect(
		d->ui->username, &QLineEdit::textChanged, this,
		&LoginDialog::updateOkButtonEnabled);
	connect(
		d->ui->password, &QLineEdit::textChanged, this,
		&LoginDialog::updateOkButtonEnabled);
	connect(
		d->ui->password, &QLineEdit::textChanged, d->ui->badPasswordLabel,
		&QWidget::hide);
	connect(
		d->ui->sessionPassword, &QLineEdit::textChanged, this,
		&LoginDialog::updateOkButtonEnabled);
	connect(
		d->ui->sessionPassword, &QLineEdit::textChanged,
		d->ui->badSessionPasswordLabel, &QWidget::hide);
	connect(
		d->ui->sessionList->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&LoginDialog::updateOkButtonEnabled);
	connect(
		d->ui->replaceCert, &QAbstractButton::toggled, this,
		&LoginDialog::updateOkButtonEnabled);

	connect(
		d->okButton, &QPushButton::clicked, this, &LoginDialog::onOkClicked);
	connect(d->cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
	connect(
		d->reportButton, &QPushButton::clicked, this,
		&LoginDialog::onReportClicked);
	connect(
		d->yesButton, &QPushButton::clicked, this, &LoginDialog::onYesClicked);
	connect(
		d->noButton, &QPushButton::clicked, this, &LoginDialog::onNoClicked);
	connect(this, &QDialog::rejected, login, &net::LoginHandler::cancelLogin);

	connect(
		d->ui->methodExtAuthButton, &QAbstractButton::clicked, this,
		&LoginDialog::onLoginMethodExtAuthClicked);
	connect(
		d->ui->methodAuthButton, &QAbstractButton::clicked, this,
		&LoginDialog::onLoginMethodAuthClicked);
	connect(
		d->ui->methodGuestButton, &QAbstractButton::clicked, this,
		&LoginDialog::onLoginMethodGuestClicked);

	connect(
		d->ui->avatarList, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LoginDialog::updateAvatar);
	connect(
		d->avatars, &QAbstractItemModel::rowsInserted, this,
		&LoginDialog::pickNewAvatar);

	connect(
		d->ui->viewOldCert, &QPushButton::clicked, this,
		&LoginDialog::showOldCert);
	connect(
		d->ui->viewNewCert, &QPushButton::clicked, this,
		&LoginDialog::showNewCert);

	d->loginDestructConnection = connect(
		login, &net::LoginHandler::destroyed, this, &LoginDialog::deleteLater);

	connect(
		login, &net::LoginHandler::loginMethodChoiceNeeded, this,
		&LoginDialog::onLoginMethodChoiceNeeded);
	connect(
		login, &net::LoginHandler::loginMethodMismatch, this,
		&LoginDialog::onLoginMethodMismatch);
	connect(
		login, &net::LoginHandler::usernameNeeded, this,
		&LoginDialog::onUsernameNeeded);
	connect(
		login, &net::LoginHandler::loginNeeded, this,
		&LoginDialog::onLoginNeeded);
	connect(
		login, &net::LoginHandler::extAuthNeeded, this,
		&LoginDialog::onExtAuthNeeded);
	connect(
		login, &net::LoginHandler::sessionConfirmationNeeded, this,
		&LoginDialog::onSessionConfirmationNeeded);
	connect(
		login, &net::LoginHandler::sessionPasswordNeeded, this,
		&LoginDialog::onSessionPasswordNeeded);
	connect(
		login, &net::LoginHandler::badSessionPassword, this,
		&LoginDialog::onBadSessionPassword);
	connect(login, &net::LoginHandler::loginOk, this, &LoginDialog::onLoginOk);
	connect(
		login, &net::LoginHandler::badLoginPassword, this,
		&LoginDialog::onBadLoginPassword);
	connect(
		login, &net::LoginHandler::extAuthComplete, this,
		&LoginDialog::onExtAuthComplete);
	connect(
		login, &net::LoginHandler::sessionChoiceNeeded, this,
		&LoginDialog::onSessionChoiceNeeded);
	connect(
		login, &net::LoginHandler::certificateCheckNeeded, this,
		&LoginDialog::onCertificateCheckNeeded);
	connect(
		login, &net::LoginHandler::serverTitleChanged, this,
		&LoginDialog::onServerTitleChanged);

	selectCurrentAvatar();
}

LoginDialog::~LoginDialog()
{
	delete d;
}

void LoginDialog::updateOkButtonEnabled()
{
	bool enabled = false;
	switch(d->mode) {
	case Mode::Loading:
	case Mode::LoginMethod:
	case Mode::Catchup:
	case Mode::ConfirmNsfm:
		break;
	case Mode::Identity:
	case Mode::GuestLogin:
		enabled = UsernameValidator::isValid(d->ui->username->text());
		break;
	case Mode::Authenticate:
		enabled = !d->ui->password->text().isEmpty();
		break;
	case Mode::AuthLogin:
	case Mode::ExtAuthLogin:
		enabled = UsernameValidator::isValid(d->ui->username->text()) &&
				  !d->ui->password->text().isEmpty();
		break;
	case Mode::SessionPassword:
		enabled = !d->ui->sessionPassword->text().isEmpty();
		break;
	case Mode::SessionList: {
		QModelIndexList sel =
			d->ui->sessionList->selectionModel()->selectedIndexes();
		if(sel.isEmpty())
			enabled = false;
		else
			enabled =
				sel.first().data(net::LoginSessionModel::JoinableRole).toBool();

		d->reportButton->setEnabled(
			!sel.isEmpty() && d->loginHandler &&
			d->loginHandler->supportsAbuseReports());
		break;
	}
	case Mode::CertChanged:
		enabled = d->ui->replaceCert->isChecked();
		break;
	}

	d->okButton->setEnabled(enabled);
}

void LoginDialog::onLoginMethodExtAuthClicked()
{
	d->resetMode(Mode::ExtAuthLogin);
	updateOkButtonEnabled();
	d->setLoginExplanation(
		tr("Enter the username and password for your %1 account.")
			.arg(d->extauthurl.host()),
		false);
}

void LoginDialog::onLoginMethodAuthClicked()
{
	d->resetMode(Mode::AuthLogin);
	updateOkButtonEnabled();
	d->setLoginExplanation(
		tr("Enter the username and password for your server account."), false);
}

void LoginDialog::onLoginMethodGuestClicked()
{
	d->resetMode(Mode::GuestLogin);
	updateOkButtonEnabled();
	d->setLoginExplanation(tr("Enter the name you want to use."), false);
}

void LoginDialog::updateAvatar(int row)
{
	QModelIndex index = d->avatars->index(row);
	int type = index.data(AvatarListModel::TypeRole).toInt();
	desktop::settings::Settings &settings = dpApp().settings();
	switch(type) {
	case AvatarListModel::NoAvatar:
		settings.setLastAvatar(QString{});
		break;
	case AvatarListModel::FileAvatar:
		settings.setLastAvatar(
			index.data(AvatarListModel::FilenameRole).toString());
		break;
	case AvatarListModel::AddAvatar:
		d->revertAvatar(settings);
		AvatarImport::importAvatar(d->avatars, this);
		break;
	default:
		qWarning("Unknown avatar type %d", type);
		d->revertAvatar(settings);
		break;
	}
}

void LoginDialog::pickNewAvatar(const QModelIndex &parent, int first, int last)
{
	Q_UNUSED(parent);
	Q_UNUSED(last);
	d->avatars->commit();
	d->ui->avatarList->setCurrentIndex(first);
	// Not sure if this is just acting weird because of my funky window manager,
	// but sometimes picking a new avatar causes the login dialog to jump behind
	// the (currently inactive) start dialog. So I'll make sure it's in front.
	activateWindow();
	raise();
}

void LoginDialog::showOldCert()
{
	auto dlg = new CertificateView(
		d->loginHandler ? d->loginHandler->url().host() : QString(),
		d->oldCert);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void LoginDialog::showNewCert()
{
	auto dlg = new CertificateView(
		d->loginHandler ? d->loginHandler->url().host() : QString(),
		d->newCert);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void LoginDialog::onLoginMethodChoiceNeeded(
	const QVector<net::LoginHandler::LoginMethod> &methods,
	const QUrl &extAuthUrl, const QString &loginInfo)
{
	bool extAuth = methods.contains(net::LoginHandler::LoginMethod::ExtAuth);
	d->ui->methodExtAuthButton->setEnabled(extAuth);
	d->ui->methodExtAuthButton->setVisible(extAuth);
	if(extAuth) {
		d->loginExtAuthUrl = extAuthUrl;
		d->ui->methodExtAuthButton->setText(
			tr("Log in with %1 account").arg(extAuthUrl.host()));
	} else {
		d->loginExtAuthUrl.clear();
	}

	bool auth = methods.contains(net::LoginHandler::LoginMethod::Auth);
	d->ui->methodAuthButton->setEnabled(auth);
	d->ui->methodAuthButton->setVisible(auth);

	bool guest = methods.contains(net::LoginHandler::LoginMethod::Guest);
	d->ui->methodGuestButton->setEnabled(guest);
	d->ui->methodGuestButton->setVisible(guest);

	bool hasDrawpileExtAuth = extAuth && extAuthUrl.host().compare(
											 QStringLiteral("drawpile.net"),
											 Qt::CaseInsensitive) == 0;
	d->ui->methodExtAuthButton->setIcon(
		hasDrawpileExtAuth ? QIcon(":/icons/drawpile.png") : QIcon());

	bool guestsOnly = !extAuth && !auth;
	QString drawpileSignupLink = QStringLiteral(
		"<a href=\"https://drawpile.net/accounts/signup/\">drawpile.net</a>");
	QString explanation;
	if(loginInfo.isEmpty()) {
		if(guestsOnly) {
			explanation =
				tr("This server doesn't support logging in with an account.");
		} else if(hasDrawpileExtAuth) {
			if(guest) {
				explanation =
					tr("You can continue without an account. If you want to "
					   "register one anyway, you can do so on %1.")
						.arg(drawpileSignupLink);
			} else {
				explanation =
					tr("An account is required. You can register one on %1.")
						.arg(drawpileSignupLink);
			}
		} else if(guest) {
			explanation =
				tr("You can continue without an account. The server doesn't "
				   "provide any information on how to register one either.");
		} else {
			explanation = tr("An account is required, but the server doesn't "
							 "provide any information on how to register one.");
		}
	} else {
		QUrl url = QUrl::fromUserInput(loginInfo);
		QString link = url.isValid() ? QStringLiteral("<a href=\"%1\">%2</a>")
										   .arg(
											   url.toString().toHtmlEscaped(),
											   loginInfo.toHtmlEscaped())
									 : loginInfo.toHtmlEscaped();
		if(hasDrawpileExtAuth) {
			explanation = tr("See %1 for more information about this server. "
							 "To register an account, visit %2.")
							  .arg(link, drawpileSignupLink);
		} else {
			explanation =
				tr("See %1 for more information about this server.").arg(link);
		}
	}
	d->ui->methodExplanationLabel->setText(explanation);
	d->resetMode(Mode::LoginMethod);
}

void LoginDialog::onLoginMethodMismatch(
	net::LoginHandler::LoginMethod intent,
	net::LoginHandler::LoginMethod method, bool extAuthFallback)
{
	QString explanation;
	if(intent == net::LoginHandler::LoginMethod::Guest) {
		d->resetMode(Mode::GuestLogin);
		explanation =
			tr("This username belongs to an account, pick a different one.");
	} else if(intent == net::LoginHandler::LoginMethod::Auth) {
		d->resetMode(Mode::AuthLogin);
		explanation = tr("This username doesn't belong to a server account.");
	} else if(intent == net::LoginHandler::LoginMethod::ExtAuth) {
		d->resetMode(Mode::ExtAuthLogin);
		if(extAuthFallback) {
			explanation = tr("The %1 authentication is not working.")
							  .arg(d->extauthurl.host());
		} else if(method == net::LoginHandler::LoginMethod::Guest) {
			explanation = tr("This username doesn't belong an account on %1.")
							  .arg(d->extauthurl.host());
		} else if(method == net::LoginHandler::LoginMethod::Auth) {
			explanation =
				tr("This username belongs to a server account, you "
				   "can't use it to log in through %1 on this server.")
					.arg(d->extauthurl.host());
		} else {
			explanation =
				tr("This username belongs to some other login method, you "
				   "can't use it to log in through %1 on this server.")
					.arg(d->extauthurl.host());
		}
	} else {
		qWarning("Unhandled intent %d", int(intent));
		d->resetMode(Mode::LoginMethod);
		return;
	}
	updateOkButtonEnabled();
	d->setLoginExplanation(explanation, true);
}

void LoginDialog::onUsernameNeeded(bool canSelectAvatar)
{
	d->ui->avatarList->setVisible(canSelectAvatar);
	d->resetMode(Mode::Identity);
	updateOkButtonEnabled();
}

void LoginDialog::Private::setLoginMode(const QString &prompt)
{
	ui->loginPromptLabel->setText(prompt);
	if(extauthurl.isValid()) {
		ui->loginPromptLabel->setStyleSheet(
			QStringLiteral("background: #3498db;"
						   "color: #fcfcfc;"
						   "padding: 16px"));
	} else {
		ui->loginPromptLabel->setStyleSheet(
			QStringLiteral("background: #fdbc4b;"
						   "color: #31363b;"
						   "padding: 16px"));
	}
}

void LoginDialog::Private::applyRememberedPassword()
{
#ifdef HAVE_QTKEYCHAIN
	if(!loginHandler) {
		qWarning("applyRememberedPassword: login process already ended!");
		return;
	}

	auto *readJob = new QKeychain::ReadPasswordJob(KEYCHAIN_NAME);
	readJob->setInsecureFallback(dpApp().settings().insecurePasswordStorage());
	readJob->setKey(keychainSecretName(
		loginHandler->url().userName(), extauthurl,
		loginHandler->url().host()));

	connect(
		readJob, &QKeychain::ReadPasswordJob::finished, okButton,
		[this, readJob]() {
			if(readJob->error() != QKeychain::NoError) {
				qWarning(
					"Keychain error (key=%s): %s", qPrintable(readJob->key()),
					qPrintable(readJob->errorString()));
				return;
			}

			if(mode != Mode::Authenticate) {
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

void LoginDialog::onLoginNeeded(
	const QString &forUsername, const QString &prompt,
	net::LoginHandler::LoginMethod intent)
{
	if(!forUsername.isEmpty()) {
		d->ui->username->setText(forUsername);
	}

	if(intent == net::LoginHandler::LoginMethod::Auth) {
		d->loginHandler->selectIdentity(
			d->ui->username->text(), d->ui->password->text(),
			net::LoginHandler::LoginMethod::Auth);
	} else {
		d->extauthurl = QUrl();
		d->resetMode(Mode::Authenticate);
		updateOkButtonEnabled();
		d->setLoginMode(prompt);
		d->applyRememberedPassword();
	}
}

void LoginDialog::onExtAuthNeeded(
	const QString &forUsername, const QUrl &url,
	net::LoginHandler::LoginMethod intent)
{
	Q_ASSERT(url.isValid());
	if(!forUsername.isEmpty()) {
		d->ui->username->setText(forUsername);
	}

	if(intent == net::LoginHandler::LoginMethod::ExtAuth) {
		d->loginHandler->requestExtAuth(
			d->ui->username->text(), d->ui->password->text());
	} else {
		d->extauthurl = url;
		d->resetMode(Mode::Authenticate);
		updateOkButtonEnabled();
		d->setLoginMode(formatExtAuthPrompt(url));
		d->applyRememberedPassword();
	}
}

void LoginDialog::onExtAuthComplete(
	bool success, net::LoginHandler::LoginMethod intent)
{
	if(!success) {
		onBadLoginPassword(intent);
	}
	// If success == true, onLoginOk is called too
}

void LoginDialog::onLoginOk()
{
	if(d->ui->rememberPassword->isChecked()) {
#ifdef HAVE_QTKEYCHAIN
		auto *writeJob = new QKeychain::WritePasswordJob(KEYCHAIN_NAME);
		writeJob->setInsecureFallback(
			dpApp().settings().insecurePasswordStorage());
		writeJob->setKey(keychainSecretName(
			d->loginHandler->url().userName(), d->extauthurl,
			d->loginHandler->url().host()));
		writeJob->setTextData(d->ui->password->text());
		writeJob->start();
#endif
	}
}

void LoginDialog::onBadLoginPassword(net::LoginHandler::LoginMethod intent)
{
	Mode nextMode;
	switch(intent) {
	case net::LoginHandler::LoginMethod::Auth:
		nextMode = Mode::AuthLogin;
		break;
	case net::LoginHandler::LoginMethod::ExtAuth:
		nextMode = Mode::ExtAuthLogin;
		break;
	default:
		nextMode = Mode::Authenticate;
		break;
	}
	d->resetMode(nextMode);
	d->ui->password->setText(QString());

#ifdef HAVE_QTKEYCHAIN
	auto *deleteJob = new QKeychain::DeletePasswordJob(KEYCHAIN_NAME);
	deleteJob->setInsecureFallback(
		dpApp().settings().insecurePasswordStorage());
	deleteJob->setKey(keychainSecretName(
		d->loginHandler->url().userName(), d->extauthurl,
		d->loginHandler->url().host()));
	deleteJob->start();
#endif

	d->ui->badPasswordLabel->show();
}

void LoginDialog::onSessionChoiceNeeded(net::LoginSessionModel *sessions)
{
	adjustSize(600, 400, false);

	d->sessions->setSourceModel(sessions);
	d->ui->sessionList->resizeColumnsToContents();
	d->ui->sessionList->sortByColumn(
		net::LoginSessionModel::ColumnTitle, Qt::AscendingOrder);

	QHeaderView *header = d->ui->sessionList->horizontalHeader();
	header->setSectionResizeMode(
		net::LoginSessionModel::ColumnTitle, QHeaderView::Stretch);
	header->setSectionResizeMode(
		net::LoginSessionModel::ColumnFounder, QHeaderView::ResizeToContents);

	d->resetMode(Mode::SessionList);
	updateOkButtonEnabled();
}

void LoginDialog::onSessionConfirmationNeeded(
	const QString &title, bool nsfm, bool autoJoin)
{
	desktop::settings::Settings &settings = dpApp().settings();
	if(nsfm && settings.showNsfmWarningOnJoin()) {
		d->ui->nsfmConfirmTitle->setText(
			QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped()));
		d->autoJoin = autoJoin;
		d->resetMode(Mode::ConfirmNsfm);
	} else {
		d->loginHandler->confirmJoinSelectedSession();
	}
}

void LoginDialog::onSessionPasswordNeeded()
{
	adjustSize(400, 150, true);
	d->ui->sessionPassword->setText(QString());
	d->resetMode(Mode::SessionPassword);
	updateOkButtonEnabled();
}

void LoginDialog::onBadSessionPassword()
{
	d->ui->sessionPassword->setText(QString());
	d->resetMode(Mode::SessionPassword);
	d->ui->badSessionPasswordLabel->show();
	updateOkButtonEnabled();
}

void LoginDialog::onCertificateCheckNeeded(
	const QSslCertificate &newCert, const QSslCertificate &oldCert)
{
	d->oldCert = oldCert;
	d->newCert = newCert;
	d->ui->replaceCert->setChecked(false);
	d->okButton->setEnabled(false);
	d->resetMode(Mode::CertChanged);
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
		d->resetMode(Mode::Catchup);
	}
}

void LoginDialog::onServerTitleChanged(const QString &title)
{
	d->ui->serverTitle->setText(
		htmlutils::newlineToBr(htmlutils::linkify(title.toHtmlEscaped())));
	d->ui->serverTitle->setHidden(title.isEmpty());
}

void LoginDialog::onOkClicked()
{
	if(!d->loginHandler) {
		qWarning("LoginDialog::onOkClicked: login process already ended!");
		return;
	}

	const Mode mode = d->mode;
	d->resetMode(Mode::Loading);

	switch(mode) {
	case Mode::Loading:
	case Mode::LoginMethod:
	case Mode::Catchup:
	case Mode::ConfirmNsfm:
		// No OK button in these modes
		qWarning("OK button click in wrong mode!");
		break;
	case Mode::GuestLogin:
		selectCurrentAvatar();
		d->loginHandler->selectIdentity(
			d->ui->username->text(), QString(),
			net::LoginHandler::LoginMethod::Guest);
		break;
	case Mode::AuthLogin:
		selectCurrentAvatar();
		d->loginHandler->selectIdentity(
			d->ui->username->text(), QString(),
			net::LoginHandler::LoginMethod::Auth);
		break;
	case Mode::ExtAuthLogin:
		selectCurrentAvatar();
		d->loginHandler->selectIdentity(
			d->ui->username->text(), QString(),
			net::LoginHandler::LoginMethod::ExtAuth);
		break;
	case Mode::Identity:
		selectCurrentAvatar();
		d->loginHandler->selectIdentity(
			d->ui->username->text(), QString(),
			net::LoginHandler::LoginMethod::Unknown);
		break;
	case Mode::Authenticate:
		if(d->extauthurl.isValid()) {
			d->loginHandler->requestExtAuth(
				d->ui->username->text(), d->ui->password->text());
		} else {
			d->loginHandler->selectIdentity(
				d->ui->username->text(), d->ui->password->text(),
				net::LoginHandler::LoginMethod::Unknown);
		}
		break;
	case Mode::SessionList: {
		if(d->ui->sessionList->selectionModel()->selectedIndexes().isEmpty()) {
			qWarning("Ok clicked but no session selected!");
			return;
		}

		const QModelIndex i =
			d->ui->sessionList->selectionModel()->selectedIndexes().first();
		d->loginHandler->prepareJoinSelectedSession(
			i.data(net::LoginSessionModel::AliasOrIdRole).toString(),
			i.data(net::LoginSessionModel::NeedPasswordRole).toBool(),
			i.data(net::LoginSessionModel::CompatibilityModeRole).toBool(),
			i.data(net::LoginSessionModel::TitleRole).toString(),
			i.data(net::LoginSessionModel::NsfmRole).toBool(), false);
		break;
	}
	case Mode::SessionPassword:
		d->loginHandler->sendSessionPassword(d->ui->sessionPassword->text());
		break;
	case Mode::CertChanged:
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

	const QModelIndex idx =
		d->ui->sessionList->selectionModel()->selectedIndexes().first();

	AbuseReportDialog *reportDlg = new AbuseReportDialog(this);
	reportDlg->setAttribute(Qt::WA_DeleteOnClose);

	const QString sessionId =
		idx.data(net::LoginSessionModel::IdRole).toString();
	const QString sessionAlias =
		idx.data(net::LoginSessionModel::IdAliasRole).toString();
	const QString sessionTitle =
		idx.data(net::LoginSessionModel::TitleRole).toString();

	reportDlg->setSessionInfo(sessionId, sessionAlias, sessionTitle);

	connect(
		reportDlg, &AbuseReportDialog::accepted, this,
		[this, sessionId, reportDlg]() {
			if(!d->loginHandler) {
				qWarning(
					"LoginDialog abuse report: login process already ended!");
				return;
			}
			d->loginHandler->reportSession(sessionId, reportDlg->message());
		});

	reportDlg->show();
}

void LoginDialog::onYesClicked()
{
	if(!d->loginHandler) {
		qWarning("LoginDialog::onYesClicked: login process already ended!");
		return;
	}

	if(d->mode == Mode::ConfirmNsfm) {
		d->resetMode(Mode::Loading);
		d->loginHandler->confirmJoinSelectedSession();
	} else {
		qWarning("Yes button click in wrong mode!");
	}
}

void LoginDialog::onNoClicked()
{
	if(!d->loginHandler) {
		qWarning("LoginDialog::onNoClicked: login process already ended!");
		return;
	}

	switch(d->mode) {
	case Mode::GuestLogin:
	case Mode::AuthLogin:
	case Mode::ExtAuthLogin:
		d->resetMode(Mode::LoginMethod);
		break;
	case Mode::ConfirmNsfm:
		if(d->autoJoin) {
			reject();
		} else {
			d->resetMode(Mode::SessionList);
		}
		break;
	default:
		qWarning("No button click in wrong mode!");
	}
}

void LoginDialog::catchupProgress(int value)
{
	d->ui->progressBar->setMaximum(100);
	d->ui->progressBar->setValue(value);
	if(d->mode == Mode::Catchup && value >= 100)
		this->deleteLater();
}

void LoginDialog::adjustSize(int width, int height, bool allowShrink)
{
	QRect geom = geometry();
	int newWidth = allowShrink ? width : qMax(geom.width(), width);
	int newHeight = allowShrink ? height : qMax(geom.height(), height);
	int newX = geom.x() + (geom.width() - newWidth) / 2;
	int newY = geom.y() + (geom.height() - newHeight) / 2;
	setGeometry(newX, newY, newWidth, newHeight);
}

void LoginDialog::selectCurrentAvatar()
{
	QPixmap avatar =
		d->ui->avatarList->currentData(Qt::DecorationRole).value<QPixmap>();
	d->loginHandler->selectAvatar(avatar);
}

QString LoginDialog::formatExtAuthPrompt(const QUrl &url)
{
	QString prompt =
		url.scheme() == "https"
			? tr("Log in with '%1' credentials")
			: tr("Log in with '%1' credentials (INSECURE CONNECTION!)");
	return prompt.arg(url.host());
}

}

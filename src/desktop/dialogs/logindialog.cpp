// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/logindialog.h"
#include "desktop/dialogs/abusereport.h"
#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/certificateview.h"
#include "desktop/main.h"
#include "desktop/utils/accountlistmodel.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/loginsessions.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/html.h"
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/utils/statedatabase.h"
#include "libclient/utils/usernamevalidator.h"
#include "ui_logindialog.h"
#include <QAction>
#include <QCryptographicHash>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

namespace dialogs {

enum class Mode {
	Loading,		 // used whenever we're waiting for the server
	Rules,			 // show the server rules and request acceptance
	RecentAccounts,	 // ask the user to pick a recent account
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
	AccountListModel *accounts;
	SessionFilterProxyModel *sessions;
	Ui_LoginDialog *ui;

	QPushButton *okButton;
	QPushButton *cancelButton;
	QPushButton *reportButton;
	QPushButton *yesButton;
	QPushButton *noButton;
	QString originalYesButtonText;
	QString originalNoButtonText;
	QString avatarFilename;

	QString ruleKey;
	QString ruleHash;
	bool guestsOnly = false;
	QUrl loginExtAuthUrl;
	QUrl extauthurl;
	QSslCertificate oldCert, newCert;
	bool autoJoin;
	int readPasswordJobId = 0;
	int nextReadPasswordJobId = 1;
	bool wasRecentAccount = false;
	bool logInAfterPasswordRead = true;

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

		accounts = new AccountListModel(dpApp().state(), avatars, dlg);
		ui->recentAccountCombo->setModel(accounts);

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
		originalYesButtonText = yesButton->text();
		noButton = ui->buttonBox->button(QDialogButtonBox::No);
		originalNoButtonText = noButton->text();

		reportButton = ui->buttonBox->addButton(
			LoginDialog::tr("Report..."), QDialogButtonBox::ActionRole);
		reportButton->setEnabled(
			false); // needs a selected session to be enabled

		resetMode(Mode::Loading);
	}

	~Private() { delete ui; }

	void resetMode(Mode mode);
	QWidget *setupAuthPage(bool usernameEnabled, bool passwordVisible);
	void setLoginExplanation(const QString &explanation, bool isError);
	void setLoginMode(const QString &prompt);

	bool haveRuleText() const { return !ruleKey.isEmpty(); }

	QString
	buildOldKeychainSecretName(const QString &host, const QString &username)
	{
		// Not case-folded, for backward compatibility.
		QString lowercaseUsername = username.toLower();
		if(extauthurl.isValid()) {
			return AccountListModel::buildKeychainSecretName(
				AccountListModel::Type::ExtAuth, extauthurl.host(),
				lowercaseUsername);
		} else {
			return AccountListModel::buildKeychainSecretName(
				AccountListModel::Type::Auth, host, lowercaseUsername);
		}
	}

	void loadOldPassword(const QString &host, const QString &username)
	{
		readPasswordJobId = nextReadPasswordJobId++;
		logInAfterPasswordRead = true;
		accounts->readPassword(
			readPasswordJobId, buildOldKeychainSecretName(host, username));
	}

	void loadPassword(
		AccountListModel::Type type, const QString &username, bool logIn)
	{
		readPasswordJobId = nextReadPasswordJobId++;
		logInAfterPasswordRead = logIn;
		accounts->readAccountPassword(readPasswordJobId, type, username);
	}

	void restoreOldLogin()
	{
		const desktop::settings::Settings &settings = dpApp().settings();
		ui->username->setText(settings.lastUsername());
		restoreAvatar(settings.lastAvatar());
	}

	void saveOldLogin(
		const QString &host, const QString &username, const QString &password)
	{
		desktop::settings::Settings &settings = dpApp().settings();
		settings.setLastUsername(ui->username->text());
		settings.setLastAvatar(avatarFilename);
		if(!password.isEmpty()) {
			accounts->savePassword(
				password, buildOldKeychainSecretName(host, username),
				settings.insecurePasswordStorage());
		}
	}

	void restoreGuest()
	{
		const utils::StateDatabase &state = dpApp().state();
		ui->username->setText(state.get(LAST_GUEST_NAME_KEY).toString());
		restoreAvatar(state.get(LAST_GUEST_AVATAR_KEY).toString());
	}

	void saveGuest(const QString &username)
	{
		utils::StateDatabase &state = dpApp().state();
		state.put(LAST_GUEST_NAME_KEY, username);
		if(avatarFilename.isEmpty()) {
			state.remove(LAST_GUEST_AVATAR_KEY);
		} else {
			state.put(LAST_GUEST_AVATAR_KEY, avatarFilename);
		}
	}

	void saveAccount(
		AccountListModel::Type type, const QString &username,
		const QString &password)
	{
		accounts->saveAccount(
			type, username,
			ui->rememberPassword->isChecked() ? password : QString(),
			avatarFilename, dpApp().settings().insecurePasswordStorage());
	}

	void restoreAvatar(const QString &filename)
	{
		QModelIndex idx = avatars->getAvatar(filename);
		ui->avatarList->setCurrentIndex(idx.isValid() ? idx.row() : 0);
	}
};

void LoginDialog::Private::resetMode(Mode newMode)
{
	if(!loginHandler) {
		qWarning("LoginDialog::resetMode: login process already ended!");
		return;
	}

	mode = newMode;
	readPasswordJobId = 0;

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
		ui->recentPromptLabel->setText(host);
		page = ui->loadingPage;
		break;
	}
	case Mode::Rules:
		page = ui->rulesPage;
		okButton->setVisible(false);
		cancelButton->setVisible(false);
		yesButton->setText(tr("Accept"));
		yesButton->setVisible(true);
		break;
	case Mode::RecentAccounts:
		page = ui->recentPage;
		ui->recentLogInButton->setFocus();
		okButton->setVisible(false);
		break;
	case Mode::LoginMethod:
		page = ui->authModePage;
		okButton->setVisible(false);
		break;
	case Mode::GuestLogin:
		extauthurl.clear();
		page = setupAuthPage(true, false);
		ui->loginPromptLabel->setText(ui->authModePromptLabel->text());
		ui->loginPromptLabel->setStyleSheet(
			QStringLiteral("background: #4d4d4d;"
						   "color: #fcfcfc;"
						   "padding: 16px"));
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
		yesButton->setText(originalYesButtonText);
		page = ui->nsfmConfirmPage;
		break;
	}

	switch(mode) {
	case Mode::Rules:
		noButton->setVisible(true);
		noButton->setText(tr("Decline"));
		break;
	case Mode::RecentAccounts:
	case Mode::Identity:
		if(haveRuleText()) {
			noButton->setText(tr("Rules"));
			noButton->setVisible(true);
		} else {
			noButton->setVisible(false);
		}
		break;
	case Mode::LoginMethod:
		if(accounts->isEmpty()) {
			if(haveRuleText()) {
				noButton->setText(tr("Rules"));
				noButton->setVisible(true);
			} else {
				noButton->setVisible(false);
			}
		} else {
			noButton->setText(tr("Back"));
			noButton->setVisible(true);
		}
		break;
	case Mode::GuestLogin:
	case Mode::AuthLogin:
	case Mode::ExtAuthLogin:
		if(guestsOnly) {
			if(haveRuleText()) {
				noButton->setText(tr("Rules"));
				noButton->setVisible(true);
			} else {
				noButton->setVisible(false);
			}
		} else {
			noButton->setText(tr("Back"));
			noButton->setVisible(true);
		}
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
	ui->username->setEnabled(usernameEnabled);
	ui->password->setVisible(passwordVisible);
	ui->passwordIcon->setVisible(passwordVisible);
	ui->badPasswordLabel->setVisible(false);
	ui->rememberPassword->setChecked(false);
	ui->rememberPassword->setEnabled(AccountListModel::canSavePasswords(
		dpApp().settings().insecurePasswordStorage()));
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
		d->ui->recentLogInButton, &QAbstractButton::clicked, this,
		&LoginDialog::onRecentLogInClicked);
	connect(
		d->ui->recentEditButton, &QAbstractButton::clicked, this,
		&LoginDialog::onRecentEditClicked);
	connect(
		d->ui->recentForgetButton, &QAbstractButton::clicked, this,
		&LoginDialog::onRecentForgetClicked);
	connect(
		d->ui->recentBreakButton, &QAbstractButton::clicked, this,
		&LoginDialog::onRecentBreakClicked);

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
		login, &net::LoginHandler::ruleAcceptanceNeeded, this,
		&LoginDialog::onRuleAcceptanceNeeded);
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

	connect(
		d->accounts, &AccountListModel::passwordReadFinished, this,
		&LoginDialog::onPasswordReadFinished);

	d->restoreAvatar(dpApp().settings().lastAvatar());
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
	case Mode::Rules:
	case Mode::RecentAccounts:
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

void LoginDialog::onRecentLogInClicked()
{
	selectRecentAccount(true);
}

void LoginDialog::onRecentEditClicked()
{
	selectRecentAccount(false);
}

void LoginDialog::onRecentForgetClicked()
{
	QMessageBox::StandardButton result = QMessageBox::question(
		this, tr("Remove Account"),
		tr("Really forget this account? This will only remove it from your "
		   "recent account list, it won't delete the account."));
	if(result == QMessageBox::Yes) {
		d->accounts->deleteAccountAt(d->ui->recentAccountCombo->currentIndex());
		if(d->accounts->isEmpty()) {
			onRecentBreakClicked();
		}
	}
}

void LoginDialog::onRecentBreakClicked()
{
	d->resetMode(Mode::LoginMethod);
}

void LoginDialog::onLoginMethodExtAuthClicked()
{
	d->wasRecentAccount = false;
	d->resetMode(Mode::ExtAuthLogin);
	updateOkButtonEnabled();
	setExtAuthLoginExplanation();
}

void LoginDialog::onLoginMethodAuthClicked()
{
	d->wasRecentAccount = false;
	d->resetMode(Mode::AuthLogin);
	updateOkButtonEnabled();
	setAuthLoginExplanation();
}

void LoginDialog::onLoginMethodGuestClicked()
{
	d->wasRecentAccount = false;
	d->restoreGuest();
	d->resetMode(Mode::GuestLogin);
	updateOkButtonEnabled();
	d->setLoginExplanation(tr("Enter the name you want to use."), false);
}

void LoginDialog::updateAvatar(int row)
{
	QModelIndex idx = d->avatars->index(row);
	int type = idx.data(AvatarListModel::TypeRole).toInt();
	switch(type) {
	case AvatarListModel::NoAvatar:
		d->avatarFilename.clear();
		break;
	case AvatarListModel::FileAvatar:
		d->avatarFilename = idx.data(AvatarListModel::FilenameRole).toString();
		break;
	case AvatarListModel::AddAvatar:
		d->restoreAvatar(d->avatarFilename);
		AvatarImport::importAvatar(d->avatars, this);
		break;
	default:
		qWarning("Unknown avatar type %d", type);
		d->restoreAvatar(d->avatarFilename);
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

void LoginDialog::onRuleAcceptanceNeeded(const QString &ruleText)
{
	QString host = d->loginHandler->url().host();
	d->ruleKey = QStringLiteral("login/rules/%1").arg(host.toCaseFolded());

	QString title =
		tr("Server Rules for %1").arg(d->loginHandler->url().host());
	QString ruleHtml =
		QStringLiteral("<h2>%1</h2>").arg(title) +
		htmlutils::newlineToBr(htmlutils::linkify(ruleText.toHtmlEscaped()));
	d->ui->rulesTextBrowser->setHtml(ruleHtml);

	// This is not actually a cryptographic context, we just want a quick
	// way to tell if the rules text changed without saving the whole thing.
	d->ruleHash = QString::fromUtf8(
		QCryptographicHash::hash(ruleText.toUtf8(), QCryptographicHash::Md5)
			.toHex());
	QString lastAcceptedHash = dpApp().state().get(d->ruleKey).toString();
	if(d->ruleHash == lastAcceptedHash) {
		d->loginHandler->acceptRules();
	} else {
		d->resetMode(Mode::Rules);
	}
}

void LoginDialog::onLoginMethodChoiceNeeded(
	const QVector<net::LoginHandler::LoginMethod> &methods, const QUrl &url,
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

	// If only guests are an option, we can skip forward, since there's no
	// accounts that could be remembered and no password to be entered.
	d->guestsOnly = !extAuth && !auth;
	if(d->guestsOnly) {
		d->restoreGuest();
		d->resetMode(Mode::GuestLogin);
		QString explanation;
		if(loginInfo.isEmpty()) {
			explanation = tr("Enter the name you want to use.");
		} else {
			explanation = tr("Enter the name you want to use. See %1 for more "
							 "information about this server.")
							  .arg(formatLoginInfo(loginInfo));
		}
		d->setLoginExplanation(explanation, false);
		updateOkButtonEnabled();
	} else {
		QString drawpileSignupLink =
			QStringLiteral("<a href=\"https://drawpile.net/accounts/signup/\""
						   ">drawpile.net</a>");
		QString explanation;
		if(loginInfo.isEmpty()) {
			if(hasDrawpileExtAuth) {
				if(guest) {
					explanation =
						tr("You can continue without an account. If you want "
						   "to register one anyway, you can do so on %1.")
							.arg(drawpileSignupLink);
				} else {
					explanation = tr("An account is required. You can register "
									 "one on %1.")
									  .arg(drawpileSignupLink);
				}
			} else if(guest) {
				explanation = tr(
					"You can continue without an account. The server doesn't "
					"provide any information on how to register one either.");
			} else {
				explanation =
					tr("An account is required, but the server doesn't "
					   "provide any information on how to register one.");
			}
		} else {
			if(hasDrawpileExtAuth) {
				explanation =
					tr("See %1 for more information about this server. "
					   "To register an account, visit %2.")
						.arg(formatLoginInfo(loginInfo), drawpileSignupLink);
			} else {
				explanation =
					tr("See %1 for more information about this server.")
						.arg(formatLoginInfo(loginInfo));
			}
		}
		d->ui->methodExplanationLabel->setText(explanation);
		bool changed = d->accounts->load(url, extAuthUrl);
		if(d->accounts->isEmpty()) {
			d->resetMode(Mode::LoginMethod);
		} else {
			d->resetMode(Mode::RecentAccounts);
			if(changed) {
				d->ui->recentAccountCombo->setCurrentIndex(
					d->accounts->getMostRecentIndex());
			}
		}
		updateOkButtonEnabled();
	}
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
		explanation =
			tr("This username doesn't belong to an account on this server. "
			   "This is not your drawpile.net account!");
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
	d->restoreOldLogin();
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

void LoginDialog::onLoginNeeded(
	const QString &forUsername, const QString &prompt, const QString &host,
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
		delayUpdate();
		d->extauthurl = QUrl();
		d->resetMode(Mode::Authenticate);
		updateOkButtonEnabled();
		d->setLoginMode(prompt);
		d->loadOldPassword(host, d->ui->username->text());
	}
}

void LoginDialog::onExtAuthNeeded(
	const QString &forUsername, const QUrl &url, const QString &host,
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
		delayUpdate();
		d->extauthurl = url;
		d->resetMode(Mode::Authenticate);
		updateOkButtonEnabled();
		d->setLoginMode(formatExtAuthPrompt(url));
		d->loadOldPassword(host, d->ui->username->text());
	}
}

void LoginDialog::onExtAuthComplete(
	bool success, net::LoginHandler::LoginMethod intent, const QString &host,
	const QString &username)
{
	if(!success) {
		onBadLoginPassword(intent, host, username);
	}
	// If success == true, onLoginOk is called too
}

void LoginDialog::onLoginOk(
	net::LoginHandler::LoginMethod intent, const QString &host,
	const QString &username)
{
	QString password = d->ui->rememberPassword->isChecked()
						   ? d->ui->password->text()
						   : QString();
	switch(intent) {
	case net::LoginHandler::LoginMethod::Guest:
		d->saveGuest(username);
		break;
	case net::LoginHandler::LoginMethod::Auth:
		d->saveAccount(AccountListModel::Type::Auth, username, password);
		break;
	case net::LoginHandler::LoginMethod::ExtAuth:
		d->saveAccount(AccountListModel::Type::ExtAuth, username, password);
		break;
	case net::LoginHandler::LoginMethod::Unknown:
		d->saveOldLogin(host, username, password);
		break;
	}
}

void LoginDialog::onBadLoginPassword(
	net::LoginHandler::LoginMethod intent, const QString &host,
	const QString &username)
{
	Mode nextMode;
	switch(intent) {
	case net::LoginHandler::LoginMethod::Auth:
		nextMode = Mode::AuthLogin;
		d->accounts->deleteAccountPassword(
			AccountListModel::Type::Auth, username);
		break;
	case net::LoginHandler::LoginMethod::ExtAuth:
		nextMode = Mode::ExtAuthLogin;
		d->accounts->deleteAccountPassword(
			AccountListModel::Type::ExtAuth, username);
		break;
	default:
		nextMode = Mode::Authenticate;
		d->accounts->deletePassword(
			d->buildOldKeychainSecretName(host, username));
		break;
	}
	d->resetMode(nextMode);
	d->ui->password->setText(QString());
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
	updateOkButtonEnabled();
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

void LoginDialog::onPasswordReadFinished(int jobId, const QString &password)
{
	if(d->readPasswordJobId == jobId) {
		if(!password.isEmpty()) {
			d->ui->password->setText(password);
			if(d->logInAfterPasswordRead) {
				d->okButton->click();
			} else if(d->ui->rememberPassword->isEnabled()) {
				d->ui->rememberPassword->setChecked(true);
			}
		}
	} else {
		qWarning("Read password job %d returned too late", jobId);
	}
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
	case Mode::Rules:
	case Mode::RecentAccounts:
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

	switch(d->mode) {
	case Mode::Rules:
		if(d->ui->rulesRememberBox->isChecked()) {
			dpApp().state().put(d->ruleKey, d->ruleHash);
		} else {
			dpApp().state().remove(d->ruleKey);
		}
		d->loginHandler->acceptRules();
		break;
	case Mode::ConfirmNsfm:
		d->resetMode(Mode::Loading);
		d->loginHandler->confirmJoinSelectedSession();
		break;
	default:
		qWarning("Yes button click in wrong mode!");
		break;
	}
}

void LoginDialog::onNoClicked()
{
	if(!d->loginHandler) {
		qWarning("LoginDialog::onNoClicked: login process already ended!");
		return;
	}

	switch(d->mode) {
	case Mode::Rules:
		dpApp().state().remove(d->ruleKey);
		d->cancelButton->click();
		break;
	case Mode::LoginMethod:
		if(d->accounts->isEmpty()) {
			if(d->haveRuleText()) {
				d->resetMode(Mode::Rules);
			} else {
				qWarning("No button click for login method!");
			}
		} else {
			d->resetMode(Mode::RecentAccounts);
			updateOkButtonEnabled();
		}
		break;
	case Mode::GuestLogin:
	case Mode::AuthLogin:
	case Mode::ExtAuthLogin:
		if(d->guestsOnly) {
			if(d->haveRuleText()) {
				d->resetMode(Mode::Rules);
			} else {
				qWarning("No button click for guests-only login!");
			}
		} else {
			d->resetMode(
				d->wasRecentAccount ? Mode::RecentAccounts : Mode::LoginMethod);
			updateOkButtonEnabled();
		}
		break;
	case Mode::RecentAccounts:
	case Mode::Identity:
		if(d->haveRuleText()) {
			d->resetMode(Mode::Rules);
		} else {
			qWarning("No button click with no rules available!");
		}
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
		break;
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

void LoginDialog::selectRecentAccount(bool logIn)
{
	AccountListModel::Type type =
		d->ui->recentAccountCombo->currentData(AccountListModel::TypeRole)
			.value<AccountListModel::Type>();
	Mode mode = Mode::Authenticate;
	switch(type) {
	case AccountListModel::Type::Auth:
		mode = Mode::AuthLogin;
		setAuthLoginExplanation();
		break;
	case AccountListModel::Type::ExtAuth:
		mode = Mode::ExtAuthLogin;
		setExtAuthLoginExplanation();
		break;
	}
	if(mode == Mode::Authenticate) {
		qWarning("Unhandled recent account type %d", int(type));
	}

	QString avatarFilename =
		d->ui->recentAccountCombo
			->currentData(AccountListModel::AvatarFilenameRole)
			.toString();
	d->restoreAvatar(avatarFilename);

	QString username = d->ui->recentAccountCombo
						   ->currentData(AccountListModel::DisplayUsernameRole)
						   .toString();
	d->ui->username->setText(username);

	delayUpdate();
	d->wasRecentAccount = true;
	d->resetMode(mode);
	updateOkButtonEnabled();
	d->loadPassword(type, username, logIn);
}

void LoginDialog::setAuthLoginExplanation()
{
	d->setLoginExplanation(
		tr("Enter the username and password for your account on this server. "
		   "This is not your drawpile.net account!"),
		false);
}

void LoginDialog::setExtAuthLoginExplanation()
{
	d->setLoginExplanation(
		tr("Enter the username and password for your %1 account.")
			.arg(d->extauthurl.host()),
		false);
}

void LoginDialog::delayUpdate()
{
	// Hack: reading passwords is asynchronous. We can't wait for it to return
	// though, because depending on how borked the user's system is, it may take
	// a ridiculous amount of time to do so, e.g. because it's waiting for a
	// DBus timeout. If we don't wait for it to return though, we flash the
	// login prompt for a split-second, which is unnerving for the user. So
	// instead, just suspend updates for a moment, avoiding any flashing if the
	// password is read in a reasonable amount of time, but continuing if not.
	setUpdatesEnabled(false);
	QTimer::singleShot(100, this, [this] {
		setUpdatesEnabled(true);
		repaint();
	});
}

QString LoginDialog::formatLoginInfo(const QString &loginInfo)
{
	QString escaped = loginInfo.toHtmlEscaped();
	QUrl url = QUrl::fromUserInput(loginInfo);
	if(url.isValid()) {
		return QStringLiteral("<a href=\"%1\">%2</a>")
			.arg(url.toString().toHtmlEscaped(), escaped);
	} else {
		return escaped;
	}
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

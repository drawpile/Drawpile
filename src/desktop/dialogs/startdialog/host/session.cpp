// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/session.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/recents.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/imageresourcetextbrowser.h"
#include "libclient/net/server.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>
#ifdef __EMSCRIPTEN__
#	include "libclient/wasmsupport.h"
#endif

namespace dialogs {
namespace startdialog {
namespace host {

Session::Session(QWidget *parent)
	: QWidget(parent)
	, m_drawpileIcon(QStringLiteral(":/icons/drawpile.png"))
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_typeCombo = new QComboBox;
	m_typeCombo->addItem(
		QIcon::fromTheme("object-locked"),
		tr("Personal session, only people you invite can join"),
		int(Type::Passworded));
	m_typeCombo->addItem(
		QIcon::fromTheme("globe"), tr("Public session, anyone can freely join"),
		int(Type::Public));
	layout->addWidget(m_typeCombo);

	QHBoxLayout *passwordLayout = new QHBoxLayout;
	passwordLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(passwordLayout);

	m_titleNote = new utils::FormNote(
		tr("Set a session title in the Listing tab."), false,
		QIcon::fromTheme("edit-find"), true);
	passwordLayout->addWidget(m_titleNote);
	connect(
		m_titleNote, &utils::FormNote::linkClicked, this,
		&Session::requestTitleEdit);

	m_passwordLabel = new QLabel(tr("Session password:"));
	passwordLayout->addWidget(m_passwordLabel);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setPlaceholderText(
		tr("Enter or generate a session password"));
	passwordLayout->addWidget(m_passwordEdit, 1);

	m_passwordButton = new QPushButton(tr("Generate"));
	utils::setWidgetRetainSizeWhenHidden(m_passwordButton, true);
	passwordLayout->addWidget(m_passwordButton);
	connect(
		m_passwordButton, &QPushButton::clicked, this,
		&Session::generatePassword);

	m_nsfmBox = new QCheckBox(tr("Not suitable for minors (NSFM)"));
	layout->addWidget(m_nsfmBox);

	m_keepChatBox = new QCheckBox(tr("Keep chat history"));
	layout->addWidget(m_keepChatBox);

	utils::addFormSeparator(layout);

	QFormLayout *serverForm = new QFormLayout;
	serverForm->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(serverForm);

	m_serverCombo = new QComboBox;
	m_serverCombo->addItem(
		QIcon(":/icons/drawpile.png"), QStringLiteral("pub.drawpile.net"),
		SERVER_PUB);
	m_serverCombo->addItem(
		QIcon::fromTheme("edit-rename"), tr("Host or IP address"),
		SERVER_ADDRESS);
#ifdef DP_HAVE_BUILTIN_SERVER
	m_serverCombo->addItem(
		QIcon::fromTheme("network-server"),
		tr("Builtin server on this computer"), SERVER_BUILTIN);
#endif
	serverForm->addRow(tr("Server:"), m_serverCombo);
	connect(
		m_serverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &Session::updateServer);

	m_addressCombo = new QComboBox;
	m_addressCombo->setEditable(true);
	m_addressCombo->setInsertPolicy(QComboBox::NoInsert);
	m_addressCombo->lineEdit()->setPlaceholderText(
		tr("Enter a host or IP address"));
	serverForm->addRow(m_addressCombo);

	m_serverInfo = new widgets::ImageResourceTextBrowser;
	m_serverInfo->setOpenLinks(false);
	m_serverInfo->setOpenExternalLinks(false);
	m_serverInfo->setMinimumHeight(24);
	layout->addWidget(m_serverInfo, 10);
	serverForm->addRow(m_serverInfo);
	connect(
		m_serverInfo, &widgets::ImageResourceTextBrowser::anchorClicked, this,
		&Session::followServerInfoLink);

	desktop::settings::Settings &settings = dpApp().settings();
#ifdef __EMSCRIPTEN__
	QString hostpass = browser::getHostpassParam();
	if(!hostpass.isEmpty()) {
		settings.setLastSessionPassword(hostpass);
	}
#endif
	if(settings.lastSessionPassword().trimmed().isEmpty()) {
		generatePasswordWith(settings);
	}
	fixUpLastHostServer(settings);
	settings.bindLastHostType(m_typeCombo, Qt::UserRole);
	settings.bindLastHostType(this, &Session::updateType);
	settings.bindLastSessionPassword(m_passwordEdit);
	settings.bindLastNsfm(m_nsfmBox);
	settings.bindLastKeepChat(m_keepChatBox);
	settings.bindLastHostServer(m_serverCombo, Qt::UserRole);
	settings.bindLastHostServer(this, &Session::updateServer);
	settings.bindParentalControlsLevel(
		this, [this](parentalcontrols::Level level) {
			bool allowNsfm = level < parentalcontrols::Level::NoJoin;
			m_nsfmBox->setEnabled(allowNsfm);
			m_nsfmBox->setVisible(allowNsfm);
		});
	updateAddressCombo();
}

void Session::reset()
{
	desktop::settings::Settings &settings = dpApp().settings();
	m_typeCombo->setCurrentIndex(0);
	generatePasswordWith(settings);
	settings.setLastNsfm(false);
	settings.setLastKeepChat(false);
	settings.setLastHostServer(0);
}

void Session::load(const QJsonObject &json)
{
	desktop::settings::Settings &settings = dpApp().settings();
	if(json[QStringLiteral("hosttype")] != QStringLiteral("public")) {
		settings.setLastHostType(int(Type::Passworded));
		QString password =
			json[QStringLiteral("password")].toString().trimmed();
		if(!password.isEmpty()) {
			settings.setLastSessionPassword(password);
		}
	} else {
		settings.setLastHostType(int(Type::Public));
		m_typeCombo->setCurrentIndex(1);
	}

	if(json.contains(QStringLiteral("nsfm"))) {
		settings.setLastNsfm(json[QStringLiteral("nsfm")].toBool());
	}

	if(json.contains(QStringLiteral("keepchat"))) {
		settings.setLastKeepChat(json[QStringLiteral("keepchat")].toBool());
	}

	QString server = json[QStringLiteral("server")].toString();
	if(server == QStringLiteral("pub.drawpile.net")) {
		settings.setLastHostServer(SERVER_PUB);
	} else if(server == QStringLiteral("builtin")) {
#ifdef DP_HAVE_BUILTIN_SERVER
		settings.setLastHostServer(SERVER_BUILTIN);
#endif
	} else if(server == QStringLiteral("address")) {
		settings.setLastHostServer(SERVER_ADDRESS);
		QString address = json[QStringLiteral("address")].toString().trimmed();
		if(!address.isEmpty()) {
			m_addressCombo->setEditText(address);
		}
	}
}

QJsonObject Session::save() const
{
	QJsonObject json;
	if(isPersonal()) {
		json[QStringLiteral("hosttype")] = QStringLiteral("personal");
		QString password = m_passwordEdit->text().trimmed();
		if(!password.isEmpty()) {
			json[QStringLiteral("password")] = password;
		}
	} else {
		json[QStringLiteral("hosttype")] = QStringLiteral("public");
	}
	json[QStringLiteral("nsfm")] = m_nsfmBox->isChecked();
	json[QStringLiteral("keepchat")] = m_keepChatBox->isChecked();
	switch(m_server) {
	case SERVER_PUB:
		json[QStringLiteral("server")] = QStringLiteral("pub.drawpile.net");
		break;
#ifdef DP_HAVE_BUILTIN_SERVER
	case SERVER_BUILTIN:
		json[QStringLiteral("server")] = QStringLiteral("builtin");
		break;
#endif
	default: {
		json[QStringLiteral("server")] = QStringLiteral("address");
		QString address = m_addressCombo->currentText().trimmed();
		if(!address.isEmpty()) {
			json[QStringLiteral("address")] = address;
		}
		break;
	}
	}
	return json;
}

bool Session::isPersonal() const
{
	return m_typeCombo->currentData().toInt() != int(Type::Public);
}

void Session::host(
	QStringList &outErrors, QString &outPassword, bool &outNsfm,
	bool &outKeepChat, QString &outAddress, bool &outRememberAddress)
{
	if(isPersonal()) {
		outPassword = m_passwordEdit->text().trimmed();
		if(outPassword.isEmpty()) {
			outErrors.append(tr("Session: a password is required"));
		}
	} else {
		outPassword.clear();
	}

	outNsfm = m_nsfmBox->isChecked();
	outKeepChat = m_keepChatBox->isChecked();

	switch(m_server) {
	case SERVER_PUB:
		outAddress = QStringLiteral("drawpile://pub.drawpile.net");
		outRememberAddress = false;
		break;
#ifdef DP_HAVE_BUILTIN_SERVER
	case SERVER_BUILTIN:
		outAddress.clear();
		outRememberAddress = false;
		break;
#endif
	default: {
		QString address = m_addressCombo->currentText().trimmed();
		outRememberAddress = true;
		if(address.isEmpty()) {
			outAddress.clear();
			outErrors.append(tr("Session: a server is required"));
		} else {
			outAddress = net::Server::addSchemeToUserSuppliedAddress(address);
		}
		break;
	}
	}
}

bool Session::isNsfmAllowed() const
{
	return m_nsfmBox->isEnabled();
}

void Session::setNsfmBasedOnListing()
{
	if(m_nsfmBox->isEnabled()) {
		m_nsfmBox->setChecked(true);
	}
}

void Session::updateType(int type)
{
	utils::ScopedUpdateDisabler disabler(this);
	bool isPublic = type == int(Type::Public);
	if(isPublic) {
		m_passwordLabel->hide();
		m_passwordEdit->hide();
		m_passwordButton->hide();
		m_titleNote->show();
	} else {
		m_titleNote->hide();
		m_passwordLabel->show();
		m_passwordEdit->show();
		m_passwordButton->show();
	}
	emit personalChanged(!isPublic);
}

void Session::updateServer(int server)
{
	switch(server) {
	case SERVER_PUB:
		showServerPub();
		break;
	case SERVER_ADDRESS:
		showServerAddress();
		break;
	case SERVER_BUILTIN:
#ifdef DP_HAVE_BUILTIN_SERVER
		showServerBuiltin();
#else
		qWarning("Builtin server option picked, but not compiled in");
		showServerAddress();
#endif
		break;
	default:
		qWarning("Unknown server option %d", server);
		showServerAddress();
		break;
	}
}

void Session::showServerPub()
{
	m_server = SERVER_PUB;
	m_serverInfo->clearImages();
	m_serverInfo->setText(
		QStringLiteral("<p>%1</p><p>%2</p>")
			.arg(
				tr("This will host your session on the public Drawpile server. "
				   "It's freely available for anyone. To keep space available, "
				   "sessions will be stopped when they're inactive for a while "
				   "or after everybody leaves.")
					.toHtmlEscaped(),
				tr("Sessions must comply with the rules, <a "
				   "href=\"https://drawpile.net/pubrules\">click here to view "
				   "them</a>.")));
	m_addressCombo->hide();
	m_serverInfo->show();
}

void Session::showServerAddress()
{
	m_server = SERVER_ADDRESS;
	m_serverInfo->hide();
	m_addressCombo->show();
}

#ifdef DP_HAVE_BUILTIN_SERVER
void Session::showServerBuiltin()
{
	m_server = SERVER_BUILTIN;
	m_serverInfo->clearImages();
	m_serverInfo->setImage(
		QStringLiteral("favicon"),
		QIcon::fromTheme("dialog-information").pixmap(QSize(48, 48)).toImage());
	m_serverInfo->setText(
		QStringLiteral("<table><tr><td width=\"64\" align=\"center\" "
					   "valign=\"middle\"><img src=\"favicon\" width=\"48\" "
					   "height=\"48\"></td><td "
					   "valign=\"middle\"><p>%1</p><p>%2</p></td></tr></table>")
			.arg(
				tr("Hosting on your own computer requires additional setup!"),
				tr("<a href=\"https://drawpile.net/localhosthelp\">Click here "
				   "for instructions.</a>")));
	m_addressCombo->hide();
	m_serverInfo->show();
}
#endif

void Session::updateAddressCombo()
{
	utils::ScopedUpdateDisabler disabler(m_addressCombo);
	m_addressCombo->clear();

	QVector<utils::Recents::Host> rhs = dpApp().recents().getHosts();
	QString editText;
	for(const utils::Recents::Host &rh : rhs) {
		QString value = rh.toString();
		m_addressCombo->addItem(value);
		if(editText.isEmpty() && rh.hosted) {
			editText = value;
		}
	}

#ifdef __EMSCRIPTEN__
	QString hostaddress = browser::getHostaddressParam();
	if(!hostaddress.isEmpty()) {
		editText = hostaddress;
	}
#endif

	m_addressCombo->setEditText(editText);
}

void Session::followServerInfoLink(const QUrl &url)
{
	utils::openOrQuestionUrl(this, url);
}

void Session::generatePassword()
{
	generatePasswordWith(dpApp().settings());
}

void Session::generatePasswordWith(desktop::settings::Settings &settings)
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
	settings.setLastSessionPassword(password);
}

void Session::fixUpLastHostServer(desktop::settings::Settings &settings)
{
	int lastHostServer = settings.lastHostServer();
	switch(lastHostServer) {
	case SERVER_PUB:
		break;
	case SERVER_ADDRESS: {
		// If there's no last hosted-on address present or we effectively hosted
		// on pub by entering its address, we switch the server combo to pub.
		QString address = dpApp().recents().getMostRecentHostAddress();
		if(address.isEmpty() || looksLikePub(address)) {
			settings.setLastHostServer(SERVER_PUB);
		}
		break;
	}
#ifdef DP_HAVE_BUILTIN_SERVER
	case SERVER_BUILTIN:
		break;
#endif
	default:
		// Not a valid last server, fall back to pub.
		settings.setLastHostServer(SERVER_PUB);
		break;
	}
}

bool Session::looksLikePub(const QString &address)
{
	QRegularExpression re(
		QStringLiteral("\\A(?:(?:pub|www)\\.)?drawpile\\.net\\z"),
		QRegularExpression::CaseInsensitiveOption |
			QRegularExpression::DotMatchesEverythingOption);
	return re.match(address).hasMatch();
}

}
}
}

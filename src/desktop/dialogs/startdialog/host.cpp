// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host.h"
#include "desktop/main.h"
#include "desktop/utils/recents.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/server.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionidvalidator.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Host::Host(QWidget *parent)
	: Page{parent}
{
	QVBoxLayout *widgetLayout = new QVBoxLayout;
	widgetLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(widgetLayout);

	m_notes = new QWidget;
	m_notes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	widgetLayout->addWidget(m_notes);

	QVBoxLayout *notesLayout = new QVBoxLayout;
	notesLayout->setContentsMargins(0, 0, 0, 0);
	m_notes->setLayout(notesLayout);

	QHBoxLayout *iconLayout = new QHBoxLayout;
	iconLayout->setContentsMargins(0, 0, 0, 0);
	notesLayout->addLayout(iconLayout);

	iconLayout->addWidget(
		utils::makeIconLabel(QIcon::fromTheme("dialog-warning"), m_notes));

	QVBoxLayout *labelsLayout = new QVBoxLayout;
	labelsLayout->setContentsMargins(0, 0, 0, 0);
	iconLayout->addLayout(labelsLayout);

	m_titleNote = new QLabel;
	m_titleNote->setWordWrap(true);
	m_titleNote->setTextFormat(Qt::PlainText);
	m_titleNote->setText(tr("A session title is required."));
	labelsLayout->addWidget(m_titleNote);

	m_urlInTitleNote = new QLabel;
	m_urlInTitleNote->setWordWrap(true);
	m_urlInTitleNote->setTextFormat(Qt::RichText);
	m_urlInTitleNote->setText(tr(
		"Invalid session title. If you want to join a session using an invite "
		"link, <a href=\"#\">click here to go to the Join page</a>."));
	connect(
		m_urlInTitleNote, &QLabel::linkActivated, this,
		&Host::switchToJoinPageRequested);
	labelsLayout->addWidget(m_urlInTitleNote);

	m_passwordNote = new QLabel;
	m_passwordNote->setWordWrap(true);
	m_passwordNote->setTextFormat(Qt::RichText);
	m_passwordNote->setText(
		tr("Without a password set, anyone can join your session! If you want "
		   "to host a private session, choose a password or "
		   "<a href=\"#\">generate one</a>."));
	connect(
		m_passwordNote, &QLabel::linkActivated, this, &Host::generatePassword);
	labelsLayout->addWidget(m_passwordNote);

	m_localHostNote = new QLabel;
	m_localHostNote->setWordWrap(true);
	m_localHostNote->setTextFormat(Qt::RichText);
	m_localHostNote->setText(
		tr("Hosting on your computer requires additional setup! "
		   "<a href=\"#\">Click here for instructions.</a>"));
	connect(m_localHostNote, &QLabel::linkActivated, this, [] {
		QDesktopServices::openUrl(
			QStringLiteral("https://drawpile.net/localhosthelp"));
	});
	labelsLayout->addWidget(m_localHostNote);

	utils::addFormSpacer(notesLayout);
	notesLayout->addWidget(utils::makeSeparator());

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setFrameStyle(QFrame::NoFrame);
	utils::initKineticScrolling(scrollArea);
	widgetLayout->addWidget(scrollArea);

	QWidget *scroll = new QWidget;
	scroll->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(scroll);
	scrollArea->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setAlignment(Qt::AlignTop);
	layout->setContentsMargins(0, 0, 0, 0);
	scroll->setLayout(layout);
	utils::addFormSpacer(layout);

	QFormLayout *generalSection = utils::addFormSection(layout);
	m_titleEdit = new QLineEdit;
	m_titleEdit->setMaxLength(100);
	m_titleEdit->setToolTip(tr("The title is shown in the application title "
							   "bar and in the session selection dialog"));
	generalSection->addRow(tr("Title:"), m_titleEdit);
	connect(
		m_titleEdit, &QLineEdit::textChanged, this, &Host::updateHostEnabled);

	QHBoxLayout *passwordLayout = new QHBoxLayout;
	passwordLayout->setContentsMargins(0, 0, 0, 0);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setToolTip(
		tr("Optional. If left blank, no password will be needed "
		   "to join this session."));
	connect(
		m_passwordEdit, &QLineEdit::textChanged, this,
		&Host::updateHostEnabled);
	passwordLayout->addWidget(m_passwordEdit, 1);

	QPushButton *generatePasswordButton = new QPushButton(tr("Generate"));
	generatePasswordButton->setToolTip(tr("Generates a random password."));
	connect(
		generatePasswordButton, &QAbstractButton::clicked, this,
		&Host::generatePassword);
	passwordLayout->addWidget(generatePasswordButton);

	generalSection->addRow(tr("Password:"), passwordLayout);

	m_nsfmBox = new QCheckBox{tr("Not suitable for minors (NSFM)")};
	m_nsfmBox->setToolTip(
		tr("Marks the session as having age-restricted content."));
	generalSection->addRow(nullptr, m_nsfmBox);

	utils::addFormSeparator(layout);

	QHBoxLayout *remoteLayout = new QHBoxLayout;
	layout->addLayout(remoteLayout);

	QRadioButton *useRemoteRadio = new QRadioButton{tr("Host at:")};
	useRemoteRadio->setToolTip(tr("Use an external dedicated server"));
	remoteLayout->addWidget(useRemoteRadio);

	QRadioButton *useLocalRadio = new QRadioButton{tr("Host on this computer")};
	useLocalRadio->setToolTip(tr("Use Drawpile's built-in server"));
	layout->addWidget(useLocalRadio);
#ifndef DP_HAVE_BUILTIN_SERVER
	useLocalRadio->setEnabled(false);
#	ifdef Q_OS_ANDROID
	QString notAvailableMessage =
		tr("The built-in server is not available on Android.");
#	else
	QString notAvailableMessage = tr("The built-in server is not available on "
									 "this installation of Drawpile.");
#	endif
	layout->addWidget(
		utils::formNote(notAvailableMessage, QSizePolicy::RadioButton));
	utils::addFormSpacer(layout);
#endif

	m_useGroup = new QButtonGroup{this};
	m_useGroup->addButton(useLocalRadio, USE_LOCAL);
	m_useGroup->addButton(useRemoteRadio, USE_REMOTE);
	connect(
		m_useGroup,
		QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
		this, &Host::updateHostEnabled);

	m_remoteHostCombo = new QComboBox;
	m_remoteHostCombo->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_remoteHostCombo->setEditable(true);
	m_remoteHostCombo->setInsertPolicy(QComboBox::NoInsert);
	remoteLayout->addWidget(m_remoteHostCombo);
	connect(
		useRemoteRadio, &QAbstractButton::toggled, m_remoteHostCombo,
		&QWidget::setEnabled);
	m_remoteHostCombo->setEnabled(m_useGroup->checkedId() == USE_REMOTE);

	utils::addFormSeparator(layout);

	m_advancedBox = new QCheckBox(tr("Enable advanced options"));
	layout->addWidget(m_advancedBox);
	connect(
		m_advancedBox, &QCheckBox::stateChanged, this,
		&Host::updateAdvancedSectionVisible);

	m_advancedSection = utils::addFormSection(layout);

	m_idAliasLabel = new QLabel(tr("ID Alias:"));
	m_idAliasEdit = new QLineEdit;
	m_idAliasEdit->setValidator(new SessionIdAliasValidator{this});
	m_idAliasEdit->setToolTip(
		tr("An optional user friendly ID for the session"));
	m_advancedSection->addRow(m_idAliasLabel, m_idAliasEdit);

	m_announceBox = new QCheckBox{tr("List at:")};
	m_announceBox->setToolTip(tr("Announce the session at a public list"));
	m_listServerCombo = new QComboBox;
	m_listServerCombo->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_advancedSection->addRow(m_announceBox, m_listServerCombo);
	connect(
		m_announceBox, &QAbstractButton::toggled, m_listServerCombo,
		&QWidget::setEnabled);
	m_listServerCombo->setEnabled(m_announceBox->isChecked());

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindLastSessionTitle(m_titleEdit);
	settings.bindLastSessionPassword(m_passwordEdit);
	settings.bindLastIdAlias(m_idAliasEdit);
	settings.bindLastAnnounce(m_announceBox);

	m_listServerModel =
		new sessionlisting::ListServerModel{settings, false, this};
	m_listServerCombo->setModel(m_listServerModel);
	settings.bindListServers(this, &Host::updateListServers);

	settings.bindLastListingServer(m_listServerCombo, std::nullopt);
	settings.bindLastNsfm(m_nsfmBox);
	settings.bindParentalControlsLevel(
		this, [this](parentalcontrols::Level level) {
			m_allowNsfm = level < parentalcontrols::Level::NoJoin;
			m_nsfmBox->setVisible(m_allowNsfm);
			updateNsfmBasedOnTitle();
		});
	settings.bindLastHostRemote(m_useGroup);

	connect(
		&dpApp().recents(), &utils::Recents::recentHostsChanged, this,
		&Host::updateRemoteHosts);
	updateRemoteHosts();

	settings.bindHostEnableAdvanced(m_advancedBox);
	settings.bindHostEnableAdvanced(this, &Host::updateAdvancedSectionVisible);

#ifndef DP_HAVE_BUILTIN_SERVER
	useRemoteRadio->setChecked(true);
#endif

	updateListServers();
	updateHostEnabled();
}

void Host::activate()
{
	emit showButtons();
	updateHostEnabled();
}

void Host::accept()
{
	if(canHost()) {
		bool advanced = m_advancedBox->isChecked();
		emit host(
			m_titleEdit->text().trimmed(), m_passwordEdit->text().trimmed(),
			advanced ? m_idAliasEdit->text() : QString(),
			m_nsfmBox->isChecked(),
			advanced && m_announceBox->isChecked()
				? m_listServerCombo->currentData().toString()
				: QString{},
			m_useGroup->checkedId() == USE_LOCAL ? QString{}
												 : getRemoteAddress());
	}
}

void Host::setHostEnabled(bool enabled)
{
	setEnabled(enabled);
	updateHostEnabled();
}

void Host::updateHostEnabled()
{
	utils::ScopedUpdateDisabler disabler(this);
	bool missingTitle, urlInTitle;
	hasValidTitle(&missingTitle, &urlInTitle);
	bool isPublic = m_passwordEdit->text().isEmpty();
	bool isLocal = m_useGroup->checkedId() == USE_LOCAL;
	m_titleNote->setVisible(missingTitle);
	m_urlInTitleNote->setVisible(urlInTitle);
	m_passwordNote->setVisible(isPublic);
	m_localHostNote->setVisible(isLocal);
	m_notes->setVisible(missingTitle || urlInTitle || isPublic || isLocal);
	emit enableHost(canHost());
}

void Host::generatePassword()
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

void Host::updateNsfmBasedOnTitle()
{
	bool nsfmTitle = parentalcontrols::isNsfmTitle(m_titleEdit->text());
	if(nsfmTitle) {
		m_nsfmBox->setChecked(true);
	} else if(!m_nsfmBox->isVisible()) {
		m_nsfmBox->setChecked(false);
	}
}

void Host::updateListServers()
{
	m_listServerModel->reload();
	if(m_listServerCombo->count() == 0) {
		m_announceBox->setChecked(false);
		m_announceBox->setEnabled(false);
	} else {
		m_announceBox->setEnabled(true);
		if(m_listServerCombo->currentIndex() == -1) {
			m_listServerCombo->setCurrentIndex(0);
		}
	}
}

void Host::updateRemoteHosts()
{
	utils::ScopedUpdateDisabler diabler{m_remoteHostCombo};
	m_remoteHostCombo->clear();

	QVector<utils::Recents::Host> rhs = dpApp().recents().getHosts();
	QString editText;
	for(const utils::Recents::Host &rh : rhs) {
		QString value = rh.toString();
		m_remoteHostCombo->addItem(value);
		if(editText.isEmpty() && rh.hosted) {
			editText = value;
		}
	}

	if(editText.isEmpty()) {
		m_remoteHostCombo->setEditText("pub.drawpile.net");
	} else {
		m_remoteHostCombo->setEditText(editText);
	}
}

void Host::updateAdvancedSectionVisible(bool visible)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_idAliasLabel->setVisible(visible);
	m_idAliasEdit->setVisible(visible);
	m_announceBox->setVisible(visible);
	m_listServerCombo->setVisible(visible);
}

bool Host::canHost() const
{
	return isEnabled() && hasValidTitle() &&
		   (m_allowNsfm || !m_nsfmBox->isChecked()) &&
		   (m_useGroup->checkedId() == USE_LOCAL ||
			!m_remoteHostCombo->currentText().trimmed().isEmpty());
}

bool Host::hasValidTitle(bool *outMissingTitle, bool *outUrlInTitle) const
{
	QString title = m_titleEdit->text().trimmed();
	bool missingTitle = title.isEmpty();
	bool urlInTitle = !missingTitle && title.contains(QStringLiteral("://"));
	if(outMissingTitle) {
		*outMissingTitle = missingTitle;
	}
	if(outUrlInTitle) {
		*outUrlInTitle = urlInTitle;
	}
	return !missingTitle && !urlInTitle;
}

QString Host::getRemoteAddress() const
{
	QString remoteAddress = m_remoteHostCombo->currentText();
	return net::Server::addSchemeToUserSuppliedAddress(remoteAddress);
}

}
}

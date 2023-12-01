// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host.h"
#include "desktop/main.h"
#include "desktop/utils/recents.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionidvalidator.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QRadioButton>
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

	QFormLayout *generalSection = utils::addFormSection(layout);
	m_titleEdit = new QLineEdit;
	m_titleEdit->setMaxLength(100);
	m_titleEdit->setToolTip(tr("The title is shown in the application title "
							   "bar and in the session selection dialog"));
	generalSection->addRow(tr("Title:"), m_titleEdit);
	connect(
		m_titleEdit, &QLineEdit::textChanged, this, &Host::updateHostEnabled);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setToolTip(
		tr("Optional. If left blank, no password will be needed "
		   "to join this session."));
	generalSection->addRow(tr("Password:"), m_passwordEdit);

	m_nsfmBox = new QCheckBox{tr("Not suitable for minors (NSFM)")};
	m_nsfmBox->setToolTip(
		tr("Marks the session as having age-restricted content."));
	layout->addWidget(m_nsfmBox);

	utils::addFormSeparator(layout);

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

	QHBoxLayout *remoteLayout = new QHBoxLayout;
	layout->addLayout(remoteLayout);

	QRadioButton *useRemoteRadio = new QRadioButton{tr("Host at:")};
	useRemoteRadio->setToolTip(tr("Use an external dedicated server"));
	remoteLayout->addWidget(useRemoteRadio);

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
	emit enableHost(canHost());
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
	return isEnabled() && !m_titleEdit->text().trimmed().isEmpty() &&
		   (m_allowNsfm || !m_nsfmBox->isChecked()) &&
		   (m_useGroup->checkedId() == USE_LOCAL ||
			!m_remoteHostCombo->currentText().trimmed().isEmpty());
}

QString Host::getRemoteAddress() const
{
	QString remoteAddress = m_remoteHostCombo->currentText();
	QString scheme = QStringLiteral("drawpile://");
	return remoteAddress.startsWith(scheme, Qt::CaseInsensitive)
			   ? remoteAddress
			   : scheme + remoteAddress;
}

}
}

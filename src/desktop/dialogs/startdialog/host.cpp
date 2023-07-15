// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/host.h"
#include "desktop/main.h"
#include "desktop/utils/sanerformlayout.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionidvalidator.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPalette>
#include <QRadioButton>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Host::Host(QWidget *parent)
	: Page{parent}
{
	utils::SanerFormLayout *layout = new utils::SanerFormLayout{this};
	layout->setContentsMargins(0, 0, 0, 0);

	m_titleEdit = new QLineEdit;
	m_titleEdit->setMaxLength(100);
	m_titleEdit->setToolTip(tr("The title is shown in the application title "
							   "bar and in the session selection dialog"));
	layout->addRow(tr("Title:"), m_titleEdit);
	connect(
		m_titleEdit, &QLineEdit::textChanged, this, &Host::updateHostEnabled);

	m_passwordEdit = new QLineEdit;
	m_passwordEdit->setEchoMode(QLineEdit::Password);
	m_passwordEdit->setToolTip(
		tr("Optional. If left blank, no password will be needed "
		   "to join this session."));
	layout->addRow(tr("Password:"), m_passwordEdit);

	m_idAliasEdit = new QLineEdit;
	m_idAliasEdit->setValidator(new SessionIdAliasValidator{this});
	m_idAliasEdit->setToolTip(
		tr("An optional user friendly ID for the session"));
	layout->addRow(tr("ID Alias:"), m_idAliasEdit);

	m_nsfmBox = new QCheckBox{tr("Not suitable for minors (NSFM)")};
	m_nsfmBox->setToolTip(
		tr("Marks the session as having age-restricted content."));
	layout->addSpanningRow(m_nsfmBox);

	layout->addSeparator();

	QHBoxLayout *listingLayout = new QHBoxLayout;
	layout->addSpanningRow(listingLayout);

	m_announceBox = new QCheckBox{tr("List at:")};
	m_announceBox->setToolTip(tr("Announce the session at a public list"));
	listingLayout->addWidget(m_announceBox);

	m_listServerCombo = new QComboBox;
	m_listServerCombo->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Fixed);
	listingLayout->addWidget(m_listServerCombo);
	connect(
		m_announceBox, &QAbstractButton::toggled, m_listServerCombo,
		&QWidget::setEnabled);
	m_listServerCombo->setEnabled(m_announceBox->isChecked());

	layout->addSeparator();

	QRadioButton *useLocalRadio = new QRadioButton{tr("Host on this computer")};
	useLocalRadio->setToolTip(tr("Use Drawpile's built-in server"));
	useLocalRadio->setEnabled(false);
	layout->addSpanningRow(useLocalRadio);
	layout->addSpanningRow(utils::note(
		tr("Not available in the 2.2 beta yet. This option will return later!"),
		QSizePolicy::RadioButton));

	layout->addSpacer();

	QHBoxLayout *remoteLayout = new QHBoxLayout;
	layout->addSpanningRow(remoteLayout);

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

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindLastSessionTitle(m_titleEdit);
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
	m_remoteHostCombo->insertItems(0, settings.recentHosts());
	settings.bindLastRemoteHost(this, [this](const QString &lastRemoteHost) {
		m_remoteHostCombo->setEditText(lastRemoteHost);
	});

	// TODO: remove when the local server is implemented.
	useRemoteRadio->setChecked(true);

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
		emit host(
			m_titleEdit->text().trimmed(), m_passwordEdit->text().trimmed(),
			m_idAliasEdit->text(), m_nsfmBox->isChecked(),
			m_announceBox->isChecked()
				? m_listServerCombo->currentData().toString()
				: QString{},
			m_useGroup->checkedId() == USE_LOCAL ? QString{}
												 : getRemoteAddress());
	}
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

bool Host::canHost() const
{
	return !m_titleEdit->text().trimmed().isEmpty() &&
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

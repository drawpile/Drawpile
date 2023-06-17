// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/hostdialog.h"
#include "desktop/main.h"
#include "desktop/utils/mandatoryfields.h"
#include "libclient/utils/images.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionidvalidator.h"

#include "ui_hostdialog.h"

#include <QPushButton>

namespace dialogs {

HostDialog::HostDialog(QWidget *parent)
	: QDialog{parent}
	, m_ui{new Ui_HostDialog}
	, m_allowNsfm{true}
{
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	m_ui->idAlias->setValidator(new SessionIdAliasValidator(this));

	auto mandatoryfields =
		new MandatoryFields{this, m_ui->buttons->button(QDialogButtonBox::Ok)};
	connect(
		m_ui->useremote, &QCheckBox::toggled,
		[this, mandatoryfields](bool checked) {
			m_ui->remotehost->setProperty("mandatoryfield", checked);
			mandatoryfields->update();
		});

	auto &settings = dpApp().settings();
	settings.bindLastSessionTitle(m_ui->sessiontitle);
	settings.bindLastIdAlias(m_ui->idAlias);
	settings.bindLastAnnounce(m_ui->announce);
	settings.bindLastNsfm(m_ui->nsfm);
	settings.bindParentalControlsLevel(
		this, [this](parentalcontrols::Level level) {
			m_allowNsfm = level < parentalcontrols::Level::NoJoin;
			m_ui->nsfm->setVisible(m_allowNsfm);
			updateNsfmBasedOnTitle();
		});

	m_ui->listingserver->setModel(
		new sessionlisting::ListServerModel{settings, false, this});
	settings.bindLastListingServer(m_ui->listingserver, std::nullopt);

	settings.bindLastHostRemote(m_ui->useremote);

	m_ui->remotehost->insertItems(0, settings.recentRemoteHosts());

	connect(
		m_ui->sessiontitle, &QLineEdit::textChanged, this,
		&HostDialog::updateNsfmBasedOnTitle);
	updateNsfmBasedOnTitle();
}

HostDialog::~HostDialog()
{
	delete m_ui;
}

void HostDialog::rememberSettings() const
{
	// Move current address to the top of the list
	const QString current = m_ui->remotehost->currentText();
	if(!current.isEmpty() && m_ui->useremote->isChecked()) {
		auto &settings = dpApp().settings();
		const int curIdx = m_ui->remotehost->findText(current);
		if(curIdx != -1)
			m_ui->remotehost->removeItem(curIdx);

		m_ui->remotehost->setCurrentText(current);

		QStringList hosts;
		const auto max = settings.maxRecentFiles();
		if(max > 0) {
			hosts << current;
			for(auto i = 0; i < qMin(max - 1, m_ui->remotehost->count()); ++i) {
				hosts << m_ui->remotehost->itemText(i);
			}
		}
		settings.setRecentRemoteHosts(hosts);
	}
}

QString HostDialog::getRemoteAddress() const
{
	return m_ui->useremote->isChecked() ? m_ui->remotehost->currentText()
										: QString{};
}

QString HostDialog::getTitle() const
{
	return m_ui->sessiontitle->text();
}

QString HostDialog::getPassword() const
{
	return m_ui->sessionpassword->text();
}

QString HostDialog::getSessionAlias() const
{
	return m_ui->idAlias->text();
}

bool HostDialog::isNsfm() const
{
	return m_allowNsfm && m_ui->nsfm->isChecked();
}

QString HostDialog::getAnnouncementUrl() const
{
	return m_ui->announce->isChecked()
			   ? m_ui->listingserver->currentData().toString()
			   : QString{};
}

void HostDialog::updateNsfmBasedOnTitle()
{
	bool nsfmTitle = parentalcontrols::isNsfmTitle(m_ui->sessiontitle->text());
	if(nsfmTitle) {
		m_ui->nsfm->setChecked(true);
	}
	m_ui->buttons->button(QDialogButtonBox::Ok)
		->setEnabled(m_allowNsfm || !nsfmTitle);
}

}

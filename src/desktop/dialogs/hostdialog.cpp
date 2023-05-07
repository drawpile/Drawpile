// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/hostdialog.h"
#include "desktop/main.h"
#include "desktop/utils/mandatoryfields.h"
#include "libclient/utils/sessionidvalidator.h"
#include "libclient/utils/images.h"
#include "libclient/utils/listservermodel.h"

#include "ui_hostdialog.h"

#include <QPushButton>

namespace dialogs {

HostDialog::HostDialog(QWidget *parent)
	: QDialog(parent), m_ui(new Ui_HostDialog)
{
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Host"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	m_ui->idAlias->setValidator(new SessionIdAliasValidator(this));

	auto mandatoryfields = new MandatoryFields(this, m_ui->buttons->button(QDialogButtonBox::Ok));
	connect(m_ui->useremote, &QCheckBox::toggled,
		[this, mandatoryfields](bool checked) {
			m_ui->remotehost->setProperty("mandatoryfield", checked);
			mandatoryfields->update();
		}
	);

	auto &settings = dpApp().settings();
	settings.bindLastSessionTitle(m_ui->sessiontitle);
	settings.bindLastIdAlias(m_ui->idAlias);
	settings.bindLastAnnounce(m_ui->announce);

	m_ui->listingserver->setModel(new sessionlisting::ListServerModel(settings, false, this));
	settings.bindLastListingServer(m_ui->listingserver, std::nullopt);

	connect(m_ui->announce, &QCheckBox::toggled, this, &HostDialog::updateListingPermissions);
	connect(m_ui->listingserver, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &HostDialog::updateListingPermissions);

	settings.bindLastHostRemote(m_ui->useremote);

	m_ui->remotehost->insertItems(0, settings.recentRemoteHosts());

	updateListingPermissions();
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
		const int curIdx = m_ui->remotehost->findText(current);
		if(curIdx!=-1)
			m_ui->remotehost->removeItem(curIdx);

		QStringList hosts;
		hosts << current;

		for(int i=0;i<m_ui->remotehost->count();++i)
				hosts << m_ui->remotehost->itemText(i);
		m_ui->remotehost->setCurrentText(current);

		dpApp().settings().setRecentRemoteHosts(hosts);
	}
}

QString HostDialog::getRemoteAddress() const
{
	if(m_ui->useremote->isChecked())
		return m_ui->remotehost->currentText();
	return QString();
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

QString HostDialog::getAnnouncementUrl() const
{
	if(m_ui->announce->isChecked())
		return m_ui->listingserver->currentData().toString();
	return QString();
}

bool HostDialog::getAnnouncmentPrivate() const
{
	return m_ui->listPrivate->isChecked();
}

void HostDialog::updateListingPermissions()
{
	if(!m_ui->announce->isChecked()) {
		m_ui->listPublic->setEnabled(false);
		m_ui->listPrivate->setEnabled(false);
	} else {
		const bool pub = m_ui->listingserver->currentData(sessionlisting::ListServerModel::PublicListRole).toBool();
		const bool priv = m_ui->listingserver->currentData(sessionlisting::ListServerModel::PrivateListRole).toBool();

		if(!pub && m_ui->listPublic->isChecked())
			m_ui->listPrivate->setChecked(true);
		else if(!priv && m_ui->listPrivate->isChecked())
			m_ui->listPublic->setChecked(true);

		m_ui->listPublic->setEnabled(pub);
		m_ui->listPrivate->setEnabled(priv);
	}
}

}

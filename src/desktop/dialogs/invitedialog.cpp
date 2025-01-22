// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/invitedialog.h"
#include "desktop/main.h"
#include "desktop/widgets/netstatus.h"
#include "desktop/widgets/toolmessage.h"
#include "ui_invitedialog.h"
#include <QButtonGroup>
#include <QClipboard>
#include <QGuiApplication>

namespace dialogs {

struct InviteDialog::Private {
	widgets::NetStatus *netStatus;
	Ui::InviteDialog ui;
	QButtonGroup *linkTypeGroup;
	QString joinPassword;
	bool compatibilityMode;
	bool allowWeb;
	bool nsfm;
};

InviteDialog::InviteDialog(
	widgets::NetStatus *netStatus, bool compatibilityMode, bool allowWeb,
	bool nsfm, QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	d->netStatus = netStatus;
	d->joinPassword = netStatus->joinPassword();
	d->compatibilityMode = compatibilityMode;
	d->allowWeb = allowWeb;
	d->nsfm = nsfm;
	d->ui.setupUi(this);
	d->ui.urlEdit->setWordWrapMode(QTextOption::WrapAnywhere);
	d->ui.ipProgressBar->setVisible(false);

	desktop::settings::Settings &settings = dpApp().settings();

	d->linkTypeGroup = new QButtonGroup{this};
	d->linkTypeGroup->addButton(d->ui.webLinkRadio, int(LinkType::Web));
	d->linkTypeGroup->addButton(d->ui.directLinkRadio, int(LinkType::Direct));
	settings.bindInviteLinkType(d->linkTypeGroup);
	settings.bindInviteLinkType(this, &InviteDialog::updateInviteLink);
	settings.bindInviteIncludePassword(d->ui.includePasswordBox);
	settings.bindInviteIncludePassword(this, &InviteDialog::updateInviteLink);
	settings.bindShowInviteDialogOnHost(d->ui.showOnHostBox);

	connect(
		d->ui.copyButton, &QAbstractButton::clicked, this,
		&InviteDialog::copyInviteLink);

	d->ui.localExplanationLabel->setVisible(netStatus->isLocalHost());
	connect(
		d->ui.ipButton, &QAbstractButton::clicked, this,
		&InviteDialog::discoverAddress);
	connect(
		netStatus, &widgets::NetStatus::joinPasswordChanged, this,
		&InviteDialog::updatePage);
	connect(
		netStatus, &widgets::NetStatus::remoteAddressDiscovered, this,
		&InviteDialog::updatePage);
	updatePage();
}

InviteDialog::~InviteDialog()
{
	delete d;
}

void InviteDialog::setSessionCompatibilityMode(bool compatibilityMode)
{
	if(compatibilityMode != d->compatibilityMode) {
		d->compatibilityMode = compatibilityMode;
		updateInviteLink();
	}
}

void InviteDialog::setSessionAllowWeb(bool allowWeb)
{
	if(allowWeb != d->allowWeb) {
		d->allowWeb = allowWeb;
		updateInviteLink();
	}
}

void InviteDialog::setSessionNsfm(bool nsfm)
{
	if(nsfm != d->nsfm) {
		d->nsfm = nsfm;
		updateInviteLink();
	}
}

void InviteDialog::copyInviteLink()
{
	d->ui.urlEdit->selectAll();
	QGuiApplication::clipboard()->setText(d->ui.urlEdit->toPlainText());
	ToolMessage::showText(tr("Invite link copied to clipboard."));
}

void InviteDialog::discoverAddress()
{
	d->ui.ipButton->setEnabled(false);
	d->ui.ipButton->setVisible(false);
	d->ui.ipProgressBar->setVisible(true);
	d->netStatus->discoverAddress();
}

void InviteDialog::updatePage()
{
	d->joinPassword = d->netStatus->joinPassword();
	d->ui.includePasswordBox->setEnabled(!d->joinPassword.isEmpty());
	updateInviteLink();
	d->ui.pages->setCurrentIndex(
		d->netStatus->haveRemoteAddress() ? URL_PAGE_INDEX : IP_PAGE_INDEX);
}

void InviteDialog::updateInviteLink()
{
	QUrl url = d->netStatus->sessionUrl();
	bool includePassword =
		d->ui.includePasswordBox->isChecked() && !d->joinPassword.isEmpty();
	if(d->linkTypeGroup->checkedId() == int(LinkType::Web)) {
		// Because I guess nobody thought it through, IPv6 has colons, which is
		// incompatible with URLs and has to be enclosed with brackets to make
		// it work. As far as I can see, QUrl doesn't handle this sensibly.
		QString host = url.host();
		QString mangledHost =
			host.contains(':') ? QStringLiteral("[%1]").arg(host) : host;

		int port = url.port();
		QString portSuffix = port > 0 && port != cmake_config::proto::port()
								 ? QStringLiteral(":%1").arg(port)
								 : QString{};

		QStringList queryParams;
		if(d->allowWeb && !d->compatibilityMode) {
			queryParams.append(QStringLiteral("web"));
		}
		if(d->nsfm) {
			queryParams.append(QStringLiteral("nsfm"));
		}

		QString query;
		if(!queryParams.isEmpty()) {
			query = QStringLiteral("?%1").arg(queryParams.join('&'));
		}

		url = QUrl{QStringLiteral("https://drawpile.net/invites/%1%2%3%4")
					   .arg(mangledHost, portSuffix, url.path(), query)};
		if(includePassword) {
			url.setFragment(d->joinPassword);
		}
	} else if(includePassword) {
		url.setQuery(QStringLiteral("p=%1").arg(d->joinPassword));
	}
	d->ui.urlEdit->setText(url.toString());
}

}

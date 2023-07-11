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
};

InviteDialog::InviteDialog(widgets::NetStatus *netStatus, QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	d->netStatus = netStatus;
	d->joinPassword = netStatus->joinPassword();
	d->ui.setupUi(this);
	d->ui.urlEdit->setWordWrapMode(QTextOption::WrapAnywhere);
	d->ui.ipProgressBar->setVisible(false);

	desktop::settings::Settings &settings = dpApp().settings();

	d->linkTypeGroup = new QButtonGroup{this};
	d->linkTypeGroup->addButton(d->ui.webLinkRadio, int(LinkType::Web));
	d->linkTypeGroup->addButton(d->ui.directLinkRadio, int(LinkType::Direct));
	settings.bindInviteLinkType(d->linkTypeGroup);
	settings.bindInviteLinkType(this, &InviteDialog::updateInviteLink);

	if(d->joinPassword.isEmpty()) {
		d->ui.includePasswordBox->setEnabled(false);
		d->ui.includePasswordExplanationLabel->hide();
	} else {
		settings.bindInviteIncludePassword(d->ui.includePasswordBox);
		settings.bindInviteIncludePassword(
			d->ui.includePasswordExplanationLabel, &QWidget::setVisible);
		settings.bindInviteIncludePassword(
			this, &InviteDialog::updateInviteLink);
	}

	settings.bindShowInviteDialogOnHost(d->ui.showOnHostBox);

	connect(
		d->ui.copyButton, &QAbstractButton::clicked, this,
		&InviteDialog::copyInviteLink);

	d->ui.localExplanationLabel->setVisible(netStatus->isLocalHost());
	connect(
		d->ui.ipButton, &QAbstractButton::clicked, this,
		&InviteDialog::discoverAddress);
	connect(
		netStatus, &widgets::NetStatus::remoteAddressDiscovered, this,
		&InviteDialog::updatePage);
	updatePage();
}

InviteDialog::~InviteDialog()
{
	delete d;
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
	int pageIndex;
	if(d->netStatus->haveRemoteAddress()) {
		pageIndex = URL_PAGE_INDEX;
		updateInviteLink();
	} else {
		pageIndex = IP_PAGE_INDEX;
	}
	d->ui.pages->setCurrentIndex(pageIndex);
}

void InviteDialog::updateInviteLink()
{
	QUrl url = d->netStatus->sessionUrl();
	bool includePassword =
		d->ui.includePasswordBox->isChecked() && !d->joinPassword.isEmpty();
	if(d->linkTypeGroup->checkedId() == int(LinkType::Web)) {
		int port = url.port();
		QString portSuffix = port > 0 && port != cmake_config::proto::port()
								 ? QStringLiteral(":%1").arg(port)
								 : QString{};
		url = QUrl{QStringLiteral("https://drawpile.net/invites/%1%2%3")
					   .arg(url.host(), portSuffix, url.path())};
		if(includePassword) {
			url.setFragment(d->joinPassword);
		}
	} else if(includePassword) {
		url.setQuery(QStringLiteral("p=%1").arg(d->joinPassword));
	}
	d->ui.urlEdit->setText(url.toString());
}

}

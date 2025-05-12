// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/invitedialog.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/netstatus.h"
#include "desktop/widgets/toolmessage.h"
#include "libclient/net/invitelistmodel.h"
#include "ui_invitedialog.h"
#include <QAction>
#include <QButtonGroup>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QMenu>
#include <QRadioButton>
#include <QSpinBox>

namespace dialogs {

struct InviteDialog::Private {
	widgets::NetStatus *netStatus;
	net::InviteListModel *inviteListModel;
	Ui::InviteDialog ui;
	QMenu *inviteCodeMenu = nullptr;
	QAction *inviteCodeCreate = nullptr;
	QAction *inviteCodeRemove = nullptr;
	QAction *inviteCodeCopy = nullptr;
	QString joinPassword;
	bool webSupported;
	bool allowWeb;
	bool preferWebSockets;
	bool nsfm;
	bool op;
	bool moderator;
	bool supportsCodes;
	bool codesEnabled;
	bool expectSelectInviteCode = false;
};

InviteDialog::InviteDialog(
	widgets::NetStatus *netStatus, net::InviteListModel *inviteListModel,
	bool webSupported, bool allowWeb, bool preferWebSockets, bool nsfm, bool op,
	bool moderator, bool supportsCodes, bool codesEnabled, QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	d->netStatus = netStatus;
	d->inviteListModel = inviteListModel;
	d->joinPassword = netStatus->joinPassword();
	d->webSupported = webSupported;
	d->allowWeb = allowWeb;
	d->preferWebSockets = preferWebSockets;
	d->nsfm = nsfm;
	d->op = op;
	d->moderator = moderator;
	d->supportsCodes = supportsCodes;
	d->codesEnabled = codesEnabled;
	d->ui.setupUi(this);
	d->ui.urlEdit->setWordWrapMode(QTextOption::WrapAnywhere);
	d->ui.ipProgressBar->setVisible(false);

	desktop::settings::Settings &settings = dpApp().settings();
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

	connect(
		d->ui.codeLinkButton, &QAbstractButton::clicked, this,
		&InviteDialog::copyInviteCodeLinksWithMessage);

	utils::bindKineticScrolling(d->ui.codesView);
	d->ui.codesView->setHandleCopy(true);
	d->ui.codesView->setHandleDelete(true);
	connect(
		d->ui.codesView, &widgets::SpanAwareTreeView::copyRequested, this,
		&InviteDialog::copyInviteCodeLinks);
	connect(
		d->ui.codesView, &widgets::SpanAwareTreeView::deleteRequested, this,
		&InviteDialog::promptRemoveSelectedCodes);
	connect(
		d->ui.codesView,
		&widgets::SpanAwareTreeView::customContextMenuRequested, this,
		&InviteDialog::showInviteCodeContextMenu);

	d->ui.codesView->setModel(d->inviteListModel);
	connect(
		d->ui.codesView->selectionModel(), &QItemSelectionModel::currentChanged,
		this, &InviteDialog::updateCodes);
	connect(
		d->ui.codesView->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&InviteDialog::updateCodes);
	connect(
		d->inviteListModel, &net::InviteListModel::invitesUpdated, this,
		&InviteDialog::updateCodes);

	d->ui.codeExplanationLabel->setText(
		QStringLiteral("<a href=\"#\">%1</a>")
			.arg(tr("What are invite codes?").toHtmlEscaped()));
	connect(
		d->ui.codeExplanationLabel, &QLabel::linkActivated, this,
		&InviteDialog::showCodeExplanation);

	connect(
		d->ui.createCodeButton, &QPushButton::clicked, this,
		&InviteDialog::showCreateCodeDialog);
	connect(
		d->ui.removeCodeButton, &QPushButton::clicked, this,
		&InviteDialog::promptRemoveSelectedCodes);

	connect(
		d->ui.enableCodesBox, &QCheckBox::clicked, this,
		&InviteDialog::setInviteCodesEnabled);

	updatePage();
	updateCodes();
}

InviteDialog::~InviteDialog()
{
	delete d;
}

void InviteDialog::setSessionWebSupported(bool webSupported)
{
	d->webSupported = webSupported;
}

void InviteDialog::setSessionAllowWeb(bool allowWeb)
{
	if(allowWeb != d->allowWeb) {
		d->allowWeb = allowWeb;
		updateInviteLink();
	}
}

void InviteDialog::setSessionPreferWebSockets(bool preferWebSockets)
{
	if(preferWebSockets != d->preferWebSockets) {
		d->preferWebSockets = preferWebSockets;
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

void InviteDialog::setOp(bool op)
{
	if(op != d->op) {
		d->op = op;
		updateCodes();
	}
}

void InviteDialog::setSessionCodesEnabled(bool codesEnabled)
{
	if(codesEnabled != d->codesEnabled) {
		d->codesEnabled = codesEnabled;
		updateCodes();
	}
}

void InviteDialog::setServerSupportsInviteCodes(bool supportsCodes)
{
	if(supportsCodes != d->supportsCodes) {
		d->supportsCodes = supportsCodes;
		updateCodes();
	}
}

void InviteDialog::selectInviteCode(const QString &secret)
{
	if(d->expectSelectInviteCode) {
		d->expectSelectInviteCode = false;
		QModelIndex index = d->inviteListModel->indexOfSecret(secret);
		if(index.isValid()) {
			d->ui.codesView->clearSelection();
			d->ui.codesView->setCurrentIndex(index);
			copyInviteCodeLinksWithMessage();
		}
	}
}

QString InviteDialog::buildWebInviteLink(
	bool includePassword, bool web, const QString &secret) const
{
	QUrl url = d->netStatus->sessionUrl();

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
	if(d->preferWebSockets) {
		queryParams.append(QStringLiteral("w"));
	}
	if(web) {
		queryParams.append(QStringLiteral("web"));
	}
	if(d->nsfm) {
		queryParams.append(QStringLiteral("nsfm"));
	}

	QString query;
	if(!queryParams.isEmpty()) {
		query = QStringLiteral("?%1").arg(queryParams.join('&'));
	}

	QUrl inviteUrl(
		QStringLiteral("https://drawpile.net/invites/%1%2%3%4")
			.arg(
				mangledHost, portSuffix, buildPath(url.path(), secret), query));
	if(includePassword) {
		inviteUrl.setFragment(d->joinPassword);
	}
	return inviteUrl.toString();
}

QString InviteDialog::buildPath(QString path, const QString &secret)
{
	if(int pos = path.indexOf(':'); pos != -1) {
		path.truncate(pos);
	}
	if(!secret.isEmpty()) {
		path.append(':');
		path.append(secret);
	}
	return path;
}

void InviteDialog::copyInviteLink()
{
	d->ui.urlEdit->selectAll();
	QGuiApplication::clipboard()->setText(d->ui.urlEdit->toPlainText());
	ToolMessage::showText(tr("Invite link copied to clipboard."));
}

int InviteDialog::copyInviteCodeLinks()
{
	QStringList links =
		d->inviteListModel->getSecretsInOrder(gatherSelectedSecrets());
	int count = links.size();
	if(count != 0) {
		for(QString &link : links) {
			link = buildWebInviteLink(false, d->webSupported, link);
		}
		QGuiApplication::clipboard()->setText(links.join('\n'));
	}
	return count;
}

void InviteDialog::copyInviteCodeLinksWithMessage()
{
	int count = copyInviteCodeLinks();
	if(count != 0) {
		ToolMessage::showText(
			tr("Invite code link(s) copied to clipboard.", nullptr, count));
	}
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
	bool includePassword =
		d->ui.includePasswordBox->isChecked() && !d->joinPassword.isEmpty();
	d->ui.urlEdit->setPlainText(
		buildWebInviteLink(includePassword, d->allowWeb, QString()));
}

void InviteDialog::updateCodes()
{
	bool canManage = (d->codesEnabled && d->op) || d->moderator;
	QSet<QString> selectedSecrets = gatherSelectedSecrets();
	d->ui.codes->setCurrentIndex(canManage ? 0 : 1);
	d->ui.noCodesExplanation->setText(
		!d->supportsCodes  ? tr("This server does not support invite codes.")
		: !d->codesEnabled ? tr("Only server administrators can manage invite "
								"codes on this session.")
						   : tr("Only operators and server administrators can "
								"manage invite codes on this session."));
	d->ui.createCodeButton->setEnabled(canManage);
	d->ui.removeCodeButton->setEnabled(canManage && !selectedSecrets.isEmpty());

	bool canToggle = d->supportsCodes && d->moderator;
	d->ui.enableCodesBox->setEnabled(canToggle);
	d->ui.enableCodesBox->setVisible(canToggle);
	d->ui.enableCodesBox->setChecked(d->codesEnabled);

	d->ui.codeLinkButton->setEnabled(!selectedSecrets.isEmpty());
}

void InviteDialog::showCreateCodeDialog()
{
	CreateInviteCodeDialog *dlg = new CreateInviteCodeDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(
		dlg, &CreateInviteCodeDialog::createInviteCode, this,
		&InviteDialog::emitCreateInviteCode);
	dlg->show();
}

void InviteDialog::promptRemoveSelectedCodes()
{
	QSet<QString> secretsToRemove = gatherSelectedSecrets();
	int count = secretsToRemove.size();
	if(count != 0) {
		QMessageBox *box = utils::showQuestion(
			this, tr("Revoke Invite Codes"),
			tr("Really revoke %n selected invite code(s)?", nullptr, count));
		connect(
			box, &QMessageBox::accepted, this,
			&InviteDialog::removeSelectedCodes);
	}
}

void InviteDialog::removeSelectedCodes()
{
	for(const QString &secret : gatherSelectedSecrets()) {
		emit removeInviteCode(secret);
	}
}

QSet<QString> InviteDialog::gatherSelectedSecrets()
{
	QSet<QString> secrets;
	QItemSelectionModel *selectionModel = d->ui.codesView->selectionModel();
	if(selectionModel) {
		for(const QModelIndex index : selectionModel->selectedIndexes()) {
			QString secret =
				index.data(net::InviteListModel::SecretRole).toString();
			if(!secret.isEmpty()) {
				secrets.insert(secret);
			}
		}
	}
	return secrets;
}

void InviteDialog::showCodeExplanation()
{
	utils::showInformation(
		this, tr("What are invite codes?"),
		QStringLiteral("<p>%1</p><p>%2</p><p>%3</p>")
			.arg(
				tr("Invite codes let someone join the session via a "
				   "limited-use link. You can revoke the code later to take "
				   "away their access again.")
					.toHtmlEscaped(),
				tr("Someone joining via an invite code bypasses normal session "
				   "restrictions: they don't need the session password, can "
				   "join via web browser, don't need a registered account and "
				   "aren't affected by new joins being blocked.")
					.toHtmlEscaped(),
				//: "They" is referring to invite codes.
				tr("They don't bypass bans or server-wide restrictions.")
					.toHtmlEscaped()));
}

void InviteDialog::emitCreateInviteCode(int maxUses, bool op, bool trust)
{
	d->expectSelectInviteCode = true;
	emit createInviteCode(maxUses, op, trust);
}

void InviteDialog::showInviteCodeContextMenu(const QPoint &pos)
{
	if(!d->inviteCodeMenu) {
		d->inviteCodeMenu = new QMenu(d->ui.codesView);
		d->inviteCodeCreate = d->inviteCodeMenu->addAction(
			d->ui.createCodeButton->icon(), d->ui.createCodeButton->text());
		d->inviteCodeRemove = d->inviteCodeMenu->addAction(
			d->ui.removeCodeButton->icon(), d->ui.removeCodeButton->text());
		d->inviteCodeCopy = d->inviteCodeMenu->addAction(
			d->ui.codeLinkButton->icon(), d->ui.codeLinkButton->text());
		connect(
			d->inviteCodeCreate, &QAction::triggered, this,
			&InviteDialog::showCreateCodeDialog);
		connect(
			d->inviteCodeRemove, &QAction::triggered, this,
			&InviteDialog::promptRemoveSelectedCodes);
		connect(
			d->inviteCodeCopy, &QAction::triggered, this,
			&InviteDialog::copyInviteCodeLinksWithMessage);
	}
	d->inviteCodeCopy->setEnabled(d->ui.codeLinkButton->isEnabled());
	d->inviteCodeCreate->setEnabled(d->ui.createCodeButton->isEnabled());
	d->inviteCodeRemove->setEnabled(d->ui.removeCodeButton->isEnabled());
	d->inviteCodeMenu->popup(d->ui.codesView->mapToGlobal(pos));
}


CreateInviteCodeDialog::CreateInviteCodeDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Create Invite Code"));
	setWindowModality(Qt::WindowModal);
	resize(300, 250);

	QVBoxLayout *layout = new QVBoxLayout(this);
	QFormLayout *form = new QFormLayout;
	layout->addLayout(form);

	m_maxUsesSpinner = new QSpinBox;
	m_maxUsesSpinner->setRange(1, 50);
	m_maxUsesSpinner->setValue(1);
	form->addRow(tr("Uses:"), m_maxUsesSpinner);

	QRadioButton *roleNoneRadio = new QRadioButton(tr("&None"));
	QRadioButton *roleTrustRadio = new QRadioButton(tr("&Trusted"));
	QRadioButton *roleOpRadio = new QRadioButton(tr("&Operator"));
	m_roleGroup = new QButtonGroup(this);
	m_roleGroup->addButton(roleNoneRadio, ROLE_NONE);
	m_roleGroup->addButton(roleTrustRadio, ROLE_TRUST);
	m_roleGroup->addButton(roleOpRadio, ROLE_OP);
	m_roleGroup->button(ROLE_NONE)->setChecked(true);
	form->addRow(tr("Role:"), roleNoneRadio);
	form->addRow(nullptr, roleTrustRadio);
	form->addRow(nullptr, roleOpRadio);

	layout->addStretch();

	m_buttons =
		new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(
		m_buttons, &QDialogButtonBox::accepted, this,
		&CreateInviteCodeDialog::accept);
	connect(
		m_buttons, &QDialogButtonBox::rejected, this,
		&CreateInviteCodeDialog::reject);

	connect(
		this, &CreateInviteCodeDialog::accepted, this,
		&CreateInviteCodeDialog::emitCreateInviteCode);

	m_maxUsesSpinner->selectAll();
}

void CreateInviteCodeDialog::emitCreateInviteCode()
{
	int role = m_roleGroup->checkedId();
	emit createInviteCode(
		m_maxUsesSpinner->value(), role == ROLE_OP, role == ROLE_TRUST);
}

}

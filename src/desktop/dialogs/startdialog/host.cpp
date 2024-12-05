// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host.h"
#include "desktop/dialogs/startdialog/host/bans.h"
#include "desktop/dialogs/startdialog/host/dialogs.h"
#include "desktop/dialogs/startdialog/host/files.h"
#include "desktop/dialogs/startdialog/host/listing.h"
#include "desktop/dialogs/startdialog/host/permissions.h"
#include "desktop/dialogs/startdialog/host/roles.h"
#include "desktop/dialogs/startdialog/host/session.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/widgetutils.h"
#include "libshared/util/paths.h"
#include <QIcon>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Host::Host(QWidget *parent)
	: Page(parent)
{
	qRegisterMetaType<HostParams>();
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_tabs = new QTabBar;
	m_tabs->setExpanding(false);
	m_tabs->setDrawBase(false);
	layout->addWidget(m_tabs);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_stack, 1);

	m_sessionPage = new host::Session;
	m_tabs->addTab(QIcon::fromTheme("network-server"), tr("Session"));
	m_stack->addWidget(m_sessionPage);

	m_listingPage = new host::Listing;
	m_tabs->addTab(QIcon::fromTheme("edit-find"), tr("Listing"));
	m_stack->addWidget(m_listingPage);

	m_permissionsPage = new host::Permissions;
	m_tabs->addTab(QIcon::fromTheme("object-locked"), tr("Permissions"));
	m_stack->addWidget(m_permissionsPage);

	m_rolesPage = new host::Roles;
	m_tabs->addTab(QIcon::fromTheme("user-group-new"), tr("Roles"));
	m_stack->addWidget(m_rolesPage);

	m_bansPage = new host::Bans;
	m_tabs->addTab(QIcon::fromTheme("drawpile_ban"), tr("Bans"));
	m_stack->addWidget(m_bansPage);

	utils::addFormSeparator(layout);

	connect(
		m_tabs, &QTabBar::currentChanged, m_stack,
		&QStackedWidget::setCurrentIndex);
	connect(
		m_sessionPage, &host::Session::personalChanged, m_listingPage,
		&host::Listing::setPersonal);
	connect(
		m_sessionPage, &host::Session::requestTitleEdit, this,
		&Host::startEditingTitle);
	connect(
		m_listingPage, &host::Listing::requestNsfmBasedOnListing, m_sessionPage,
		&host::Session::setNsfmBasedOnListing);
	m_listingPage->setPersonal(m_sessionPage->isPersonal());
}

void Host::activate()
{
	emit hideLinks();
	emit showButtons();
}

void Host::accept()
{
	QStringList errors;
	HostParams params;
	m_sessionPage->host(
		errors, params.password, params.nsfm, params.keepChat, params.address,
		params.rememberAddress);
	m_listingPage->host(
		m_sessionPage->isNsfmAllowed(), errors, params.title, params.alias,
		params.announcementUrls);
	m_permissionsPage->host(
		params.undoLimit, params.featurePermissions, params.deputies);
	m_rolesPage->host(params.operatorPassword, params.auth);
	m_bansPage->host(params.bans);
	int errorCount = errors.size();
	if(errorCount == 0) {
		emit host(params);
	} else {
		QString joinedErrors;
		for(const QString &error : errors) {
			joinedErrors.append(
				QStringLiteral("<li>%1</li>").arg(error.toHtmlEscaped()));
		}
		utils::showCritical(
			this, tr("Host Error"),
			QStringLiteral("<p>%1</p><ul>%2</ul>")
				.arg(
					tr("Invalid input(s), please correct the following:",
					   nullptr, errorCount)
						.toHtmlEscaped(),
					joinedErrors));
	}
}

void Host::triggerReset()
{
	host::ResetDialog *dlg = new host::ResetDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(
		dlg, &host::ResetDialog::resetRequested, this, &Host::resetCategory);
	utils::showWindow(dlg);
}

void Host::triggerLoad()
{
	host::LoadDialog *dlg = new host::LoadDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &host::LoadDialog::loadRequested, this, &Host::loadCategory);
	connect(dlg, &host::LoadDialog::resetRequested, this, &Host::resetCategory);
	utils::showWindow(dlg);
}

void Host::triggerSave()
{
	host::SaveDialog *dlg = new host::SaveDialog(
		m_sessionPage->save(), m_listingPage->save(), m_permissionsPage->save(),
		m_rolesPage->save(), m_bansPage->save(), this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
}

void Host::triggerImport()
{
	FileWrangler(this).openSessionSettings(
		[this](const QString *path, const QByteArray *bytes) {
			host::SessionSettingsImporter importer;
			connect(
				&importer, &host::SessionSettingsImporter::importFinished, this,
				&Host::onImportFinished);
			connect(
				&importer, &host::SessionSettingsImporter::importFailed, this,
				&Host::onImportFailed);

			if(bytes) {
				importer.importSessionSettings(
					path ? *path : QString(), *bytes);
			} else if(path) {
				QByteArray content;
				QString error;
				if(utils::paths::slurp(*path, content, error)) {
					importer.importSessionSettings(*path, content);
				} else {
					onImportFailed(tr("Failed to read file: %1").arg(error));
				}
			} else {
				onImportFailed(tr("No file or content received."));
			}
		});
}

void Host::triggerExport()
{
	host::ExportDialog *dlg = new host::ExportDialog(
		m_sessionPage->save(), m_listingPage->save(), m_permissionsPage->save(),
		m_rolesPage->save(), m_bansPage->save(), this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
}

void Host::startEditingTitle()
{
	m_tabs->setCurrentIndex(1);
	m_listingPage->startEditingTitle();
}

void Host::loadCategory(
	int category, const QJsonObject &json, bool replaceAnnouncements,
	bool replaceAuth, bool replaceBans)
{
	switch(category) {
	case int(host::Categories::Session):
		m_sessionPage->load(json);
		break;
	case int(host::Categories::Listing):
		m_listingPage->load(json, replaceAnnouncements);
		break;
	case int(host::Categories::Permissions):
		m_permissionsPage->load(json);
		break;
	case int(host::Categories::Roles):
		m_rolesPage->load(json, replaceAuth);
		break;
	case int(host::Categories::Bans):
		m_bansPage->load(json, replaceBans);
		break;
	default:
		qWarning("Host::loadCategory: unknown category %d", category);
		break;
	}
}

void Host::resetCategory(
	int category, bool replaceAnnouncements, bool replaceAuth, bool replaceBans)
{
	switch(category) {
	case int(host::Categories::Session):
		m_sessionPage->reset();
		break;
	case int(host::Categories::Listing):
		m_listingPage->reset(replaceAnnouncements);
		break;
	case int(host::Categories::Permissions):
		m_permissionsPage->reset();
		break;
	case int(host::Categories::Roles):
		m_rolesPage->reset(replaceAuth);
		break;
	case int(host::Categories::Bans):
		m_bansPage->reset(replaceBans);
		break;
	default:
		qWarning("Host::resetCategory: unknown category %d", category);
		break;
	}
}

void Host::onImportFinished(const QJsonObject &json)
{
	host::ImportDialog *dlg = new host::ImportDialog(json, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &host::ImportDialog::loadRequested, this, &Host::loadCategory);
	utils::showWindow(dlg);
}

void Host::onImportFailed(const QString &error)
{
	utils::showCritical(
		this, tr("Import Failed"), tr("Could not import session settings."),
		error);
}

}
}

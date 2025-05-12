// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/listing.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/net/announcementlist.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionidvalidator.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <functional>

namespace dialogs {
namespace startdialog {
namespace host {

Listing::Listing(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_automaticBox = new QCheckBox(tr("Set title automatically"));
	layout->addWidget(m_automaticBox);
	connect(
		m_automaticBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&Listing::updateEditSectionVisibility);

	m_editSection = new QWidget;
	m_editSection->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_editSection);

	QFormLayout *manualLayout = new QFormLayout(m_editSection);
	manualLayout->setContentsMargins(0, 0, 0, 0);

	m_titleEdit = new QLineEdit;
	m_titleEdit->setMaxLength(50);
	m_titleEdit->setPlaceholderText(tr("Enter a publicly visible title"));
	manualLayout->addRow(tr("Title:"), m_titleEdit);

	utils::addFormSeparator(layout);

	QFormLayout *listingLayout = new QFormLayout;
	listingLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(listingLayout);

	m_idAliasEdit = new QLineEdit;
	m_idAliasEdit->setValidator(new SessionIdAliasValidator(this));
	m_idAliasEdit->setPlaceholderText(
		tr("Publicly visible ID for pretty invite links"));
	listingLayout->addRow(tr("ID alias:"), m_idAliasEdit);

	desktop::settings::Settings &settings = dpApp().settings();
	m_announcementModel = new net::AnnouncementListModel(this);

	QSortFilterProxyModel *announcementSortModel =
		new QSortFilterProxyModel(this);
	announcementSortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	announcementSortModel->setSourceModel(m_announcementModel);

	m_announcements = new QListView;
	m_announcements->setModel(announcementSortModel);
	m_announcements->setSelectionMode(QAbstractItemView::ExtendedSelection);
	listingLayout->addRow(tr("Announcements:"), m_announcements);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	listingLayout->addRow(buttonsLayout);

	buttonsLayout->addStretch();

	m_addAnnouncementButton =
		new QPushButton(QIcon::fromTheme("list-add"), tr("Add"));
	m_addAnnouncementButton->setMenu(new QMenu(m_addAnnouncementButton));
	buttonsLayout->addWidget(m_addAnnouncementButton);

	m_removeAnnouncementButton =
		new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove selected"));
	m_removeAnnouncementButton->setEnabled(false);
	buttonsLayout->addWidget(m_removeAnnouncementButton);
	connect(
		m_removeAnnouncementButton, &QPushButton::clicked, this,
		&Listing::removeSelectedAnnouncements);
	connect(
		m_announcements->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&Listing::updateRemoveAnnouncementsButton);
	updateRemoveAnnouncementsButton();

	updateEditSectionVisibility();

	settings.bindLastSessionAutomatic(m_automaticBox);
	settings.bindLastSessionTitle(m_titleEdit);
	settings.bindLastIdAlias(m_idAliasEdit);
	settings.bindListServers(this, &Listing::reloadAnnouncementMenu);
	m_announcementModel->setAnnouncements(settings.lastListingUrls());

	connect(
		m_titleEdit, &QLineEdit::textChanged, this, &Listing::checkNsfmTitle);
	connect(
		m_idAliasEdit, &QLineEdit::textChanged, this, &Listing::checkNsfmAlias);
}

void Listing::reset(bool replaceAnnouncements)
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setLastSessionAutomatic(true);
	settings.setLastSessionTitle(QString());
	settings.setLastIdAlias(QString());
	if(replaceAnnouncements) {
		settings.setLastListingUrls(QStringList());
		m_announcementModel->clear();
	}
}

void Listing::load(const QJsonObject &json, bool replaceAnnouncements)
{
	if(json.contains(QStringLiteral("autotitle"))) {
		m_automaticBox->setChecked(json[QStringLiteral("autotitle")].toBool());
	}

	QString title = json[QStringLiteral("title")].toString();
	if(!title.isEmpty()) {
		m_titleEdit->setText(title);
	}

	if(json.contains(QStringLiteral("alias"))) {
		m_idAliasEdit->setText(json[QStringLiteral("alias")].toString());
	}

	if(json.contains(QStringLiteral("announcements"))) {
		QStringList urls;
		if(!replaceAnnouncements) {
			urls = m_announcementModel->announcements();
		}

		QJsonArray announcements =
			json[QStringLiteral("announcements")].toArray();
		for(const QJsonValue &value : announcements) {
			QString url = value.toString().trimmed();
			if(!url.isEmpty() && !urls.contains(url)) {
				urls.append(url);
			}
		}

		m_announcementModel->setAnnouncements(urls);
	}
}

QJsonObject Listing::save() const
{
	QJsonObject json;

	bool autoTitle = m_personal && m_automaticBox->isChecked();
	if(m_personal) {
		json[QStringLiteral("autotitle")] = autoTitle;
	}

	if(!autoTitle) {
		QString title = m_titleEdit->text().trimmed();
		if(!title.isEmpty()) {
			json[QStringLiteral("title")] = title;
		}
	}

	json[QStringLiteral("alias")] = m_idAliasEdit->text().trimmed();

	const QStringList &urls = m_announcementModel->announcements();
	QJsonArray announcements;
	for(const QString &url : urls) {
		announcements.append(url.trimmed());
	}
	json[QStringLiteral("announcements")] = announcements;

	return json;
}

void Listing::setPersonal(bool personal)
{
	if(m_personal != personal) {
		m_personal = personal;
		m_automaticBox->setEnabled(m_personal);
		m_automaticBox->setVisible(m_personal);
		updateEditSectionVisibility();
	}
}

void Listing::startEditingTitle()
{
	if(isEditSectionVisible()) {
		m_titleEdit->selectAll();
		m_titleEdit->setFocus();
	} else {
		qWarning("Title editing requested, but section not visible");
	}
}

void Listing::host(
	bool nsfmAllowed, QStringList &outErrors, QString &outTitle,
	QString &outAlias, QStringList &outAnnouncementUrls)
{
	if(m_personal) {
		if(m_automaticBox->isChecked()) {
			outTitle.clear();
		} else {
			outTitle = m_titleEdit->text().trimmed();
			if(outTitle.isEmpty()) {
				// The user unchecked the automatic title, but then didn't
				// provide a title anyway. We'll just recheck the box then.
				m_automaticBox->setChecked(true);
			}
		}
	} else {
		outTitle = m_titleEdit->text().trimmed();
		if(outTitle.isEmpty()) {
			outErrors.append(
				tr("Listing: a title is required for public sessions"));
		}
	}
	outAlias = m_idAliasEdit->text().trimmed();
	outAnnouncementUrls = m_announcementModel->announcements();
	if(!nsfmAllowed) {
		if(parentalcontrols::isNsfmTitle(outTitle)) {
			outErrors.append(tr("Listing: the title is inappropriate."));
		}
		if(parentalcontrols::isNsfmAlias(outAlias)) {
			outErrors.append(tr("Listing: the ID alias is inappropriate."));
		}
	}
}

void Listing::updateEditSectionVisibility()
{
	bool visible = isEditSectionVisible();
	m_editSection->setVisible(visible);
}

bool Listing::isEditSectionVisible() const
{
	return !m_personal || !m_automaticBox->isChecked();
}

void Listing::reloadAnnouncementMenu()
{
	QVector<QVariantMap> listServers = dpApp().settings().listServers();
	m_announcementModel->setKnownServers(listServers);

	QMenu *addAnnouncementMenu = m_addAnnouncementButton->menu();
	addAnnouncementMenu->clear();

	for(const sessionlisting::ListServer &listServer :
		sessionlisting::ListServerModel::listServers(listServers, false)) {
		if(listServer.publicListings) {
			QAction *action = addAnnouncementMenu->addAction(
				listServer.icon, listServer.name);
			connect(
				action, &QAction::triggered, this,
				std::bind(&Listing::addAnnouncement, this, listServer.url));
		}
	}

	m_addAnnouncementButton->setEnabled(!addAnnouncementMenu->isEmpty());
}

void Listing::updateRemoveAnnouncementsButton()
{
	m_removeAnnouncementButton->setEnabled(
		!m_announcements->selectionModel()->selectedIndexes().isEmpty());
}

void Listing::addAnnouncement(const QString &url)
{
	m_announcementModel->addAnnouncement(url);
	persistAnnouncements();
}

void Listing::removeSelectedAnnouncements()
{
	QModelIndexList selected =
		m_announcements->selectionModel()->selectedIndexes();

	QVector<QString> urls;
	urls.reserve(selected.size());
	for(const QModelIndex &idx : selected) {
		urls.append(idx.data(Qt::UserRole).toString());
	}

	for(const QString &url : urls) {
		m_announcementModel->removeAnnouncement(url);
	}

	persistAnnouncements();
}

void Listing::persistAnnouncements()
{
	dpApp().settings().setLastListingUrls(m_announcementModel->announcements());
}

void Listing::checkNsfmTitle(const QString &title)
{
	if((!m_personal || !m_automaticBox->isChecked()) &&
	   parentalcontrols::isNsfmTitle(title)) {
		emit requestNsfmBasedOnListing();
	}
}

void Listing::checkNsfmAlias(const QString &alias)
{
	if(parentalcontrols::isNsfmAlias(alias)) {
		emit requestNsfmBasedOnListing();
	}
}

}
}
}

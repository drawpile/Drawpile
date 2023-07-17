// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/browse.h"
#include "desktop/main.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/spanawaretreeview.h"
#include "libclient/net/sessionlistingmodel.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/sessionfilterproxymodel.h"
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>

#ifdef HAVE_DNSSD
#	include "libshared/listings/zeroconfdiscovery.h"
#endif

namespace dialogs {
namespace startdialog {

Browse::Browse(QWidget *parent)
	: Page{parent}
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	m_noListServers = new QWidget;
	m_noListServers->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(m_noListServers);

	QVBoxLayout *noListServersLayout = new QVBoxLayout;
	noListServersLayout->setContentsMargins(0, 0, 0, 0);
	m_noListServers->setLayout(noListServersLayout);

	QHBoxLayout *iconLayout = new QHBoxLayout;
	iconLayout->setContentsMargins(0, 0, 0, 0);
	noListServersLayout->addLayout(iconLayout);

	iconLayout->addWidget(utils::makeIconLabel(
		QStyle::SP_MessageBoxInformation, QStyle::PM_LargeIconSize,
		m_noListServers));

	QVBoxLayout *labelsLayout = new QVBoxLayout;
	labelsLayout->setContentsMargins(0, 0, 0, 0);
	iconLayout->addLayout(labelsLayout);

	QLabel *communitiesLabel = new QLabel;
	communitiesLabel->setOpenExternalLinks(true);
	communitiesLabel->setWordWrap(true);
	communitiesLabel->setTextFormat(Qt::RichText);
	communitiesLabel->setText(
		tr("You haven't added any servers yet. You can find some at <a "
		   "href=\"https://drawpile.net/communities/\">drawpile.net/"
		   "communities</a>."));
	labelsLayout->addWidget(communitiesLabel);

	QLabel *addPubLabel = new QLabel;
	addPubLabel->setWordWrap(true);
	addPubLabel->setTextFormat(Qt::RichText);
	addPubLabel->setText(tr("To add the public Drawpile listing server, "
							"<a href=\"#\">click here</a>."));
	labelsLayout->addWidget(addPubLabel);
	connect(addPubLabel, &QLabel::linkActivated, this, [this] {
		emit addListServerUrlRequested(
			QUrl{"https://pub.drawpile.net/listing/"});
	});

	noListServersLayout->addWidget(utils::makeSeparator());

	QHBoxLayout *filterLayout = new QHBoxLayout;
	layout->addLayout(filterLayout);

	m_filterEdit = new QLineEdit;
	m_filterEdit->setPlaceholderText(tr("Filter"));
	m_filterEdit->setClearButtonEnabled(true);
	filterLayout->addWidget(m_filterEdit);

	m_closedBox = new QCheckBox{tr("Closed")};
	m_closedBox->setToolTip(tr("Show sessions that don't let new users join"));
	filterLayout->addWidget(m_closedBox);

	m_passwordBox = new QCheckBox{tr("Password")};
	m_passwordBox->setToolTip(
		tr("Show sessions that require a password to join"));
	filterLayout->addWidget(m_passwordBox);

	m_nsfmBox = new QCheckBox{tr("NSFM")};
	m_nsfmBox->setToolTip(tr("Show sessions not suitable for minors (NSFM)"));
	filterLayout->addWidget(m_nsfmBox);
	if(parentalcontrols::level() != parentalcontrols::Level::Unrestricted) {
		m_nsfmBox->setEnabled(false);
	}

	m_listing = new widgets::SpanAwareTreeView;
	m_listing->setAlternatingRowColors(true);
	m_listing->setRootIsDecorated(false);
	m_listing->setSortingEnabled(true);
	m_listing->header()->setStretchLastSection(false);
	layout->addWidget(m_listing);

	m_sessions = new SessionListingModel{this};

#ifdef HAVE_DNSSD
	if(ZeroconfDiscovery::isAvailable()) {
		ZeroconfDiscovery *zeroconfDiscovery = new ZeroconfDiscovery{this};
		m_sessions->setMessage(tr("Nearby"), tr("Loading..."));
		connect(
			zeroconfDiscovery, &ZeroconfDiscovery::serverListUpdated, this,
			[this](const QVector<sessionlisting::Session> &servers) {
				m_sessions->setList(tr("Nearby"), servers);
			});
		zeroconfDiscovery->discover();
	}
#endif

	m_filteredSessions = new SessionFilterProxyModel{this};
	m_filteredSessions->setSourceModel(m_sessions);
	m_filteredSessions->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_filteredSessions->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_filteredSessions->setFilterKeyColumn(-1);
	m_filteredSessions->setSortRole(Qt::UserRole);

	m_listing->setModel(m_filteredSessions);
	m_listing->expandAll();

	QHeaderView *header = m_listing->header();
	header->setSectionResizeMode(
		SessionListingModel::Version, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(
		SessionListingModel::Title, QHeaderView::Stretch);
	header->setSectionResizeMode(
		SessionListingModel::Server, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(
		SessionListingModel::UserCount, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(
		SessionListingModel::Owner, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(
		SessionListingModel::Uptime, QHeaderView::ResizeToContents);
	// Don't sort by default. Otherwise it sorts by the first available column.
	header->setSortIndicator(-1, Qt::AscendingOrder);
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	header->setSortIndicatorClearable(true);
#endif

	connect(
		m_listing->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &Browse::updateJoinButton);

	connect(m_listing, &QTreeView::doubleClicked, this, &Browse::joinIndex);

	m_listingContextMenu = new QMenu{this};
	m_listing->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(
		m_listing, &QWidget::customContextMenuRequested, this,
		&Browse::showListingContextMenu);

	m_joinAction = m_listingContextMenu->addAction(tr("Join"));
	connect(m_joinAction, &QAction::triggered, this, [this]() {
		joinIndex(m_listing->selectionModel()->currentIndex());
	});

	makeCopySessionDataAction(
		tr("Copy session URL"), SessionListingModel::UrlStringRole);
	makeCopySessionDataAction(tr("Copy title"), SessionListingModel::TitleRole);
	makeCopySessionDataAction(
		tr("Copy server"), SessionListingModel::ServerRole);
	makeCopySessionDataAction(tr("Copy owner"), SessionListingModel::OwnerRole);

	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	m_refreshTimer->setInterval(1000 * (REFRESH_INTERVAL_SECS + 1));
	connect(m_refreshTimer, &QTimer::timeout, this, &Browse::periodicRefresh);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindFilterClosed(m_closedBox);
	settings.bindFilterLocked(m_passwordBox);
	settings.bindFilterNsfm(m_nsfmBox);
	settings.bindListServers(this, &Browse::updateListServers);

	m_filteredSessions->setShowClosed(m_closedBox->isChecked());
	m_filteredSessions->setShowPassworded(m_passwordBox->isChecked());
	m_filteredSessions->setShowNsfw(m_nsfmBox->isChecked());

	connect(
		m_filterEdit, &QLineEdit::textChanged, m_filteredSessions,
		&SessionFilterProxyModel::setFilterFixedString);
	connect(
		m_passwordBox, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowPassworded);
	connect(
		m_nsfmBox, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowNsfw);
	connect(
		m_closedBox, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowClosed);
}

void Browse::activate()
{
	emit hideLinks();
	emit showButtons();
	updateJoinButton();
}

void Browse::accept()
{
	joinIndex(m_listing->selectionModel()->currentIndex());
}

void Browse::updateListServers(const QVector<QVariantMap> &settingsListServers)
{
	for(sessionlisting::AnnouncementApiResponse *response :
		m_refreshesInProgress.values()) {
		delete response;
	}
	m_refreshesInProgress.clear();
	m_refreshTimer->stop();
	m_sessions->clear();

	const QVector<sessionlisting::ListServer> servers =
		sessionlisting::ListServerModel::listServers(settingsListServers, true);
	for(const sessionlisting::ListServer &ls : servers) {
		if(ls.publicListings) {
			m_sessions->setIcon(ls.name, ls.icon);
			m_sessions->setMessage(ls.name, tr("Loading..."));
		}
	}

	if(servers.isEmpty()) {
		m_noListServers->show();
	} else {
		m_noListServers->hide();
		m_listing->expandAll();
		refresh();
	}
}

void Browse::periodicRefresh()
{
	bool shouldRefresh = m_listing->isVisible() ||
						 QDateTime::currentSecsSinceEpoch() - m_lastRefresh >=
							 REFRESH_INTERVAL_SECS;
	if(shouldRefresh) {
		refresh();
	}
}

void Browse::showListingContextMenu(const QPoint &pos)
{
	QModelIndex index = m_listing->selectionModel()->currentIndex();
	if(isListingIndex(index)) {
		m_joinAction->setEnabled(canJoinIndex(index));
		m_listingContextMenu->popup(mapToGlobal(pos));
	}
}

void Browse::joinIndex(const QModelIndex &index)
{
	if(canJoinIndex(index)) {
		emit join(index.data(SessionListingModel::UrlRole).toUrl());
	}
}

void Browse::refresh()
{
	m_lastRefresh = QDateTime::currentSecsSinceEpoch();

	const QVector<sessionlisting::ListServer> &listservers =
		sessionlisting::ListServerModel::listServers(
			dpApp().settings().listServers(), true);
	for(const sessionlisting::ListServer &ls : listservers) {
		if(ls.publicListings && !m_refreshesInProgress.contains(ls.name)) {
			QUrl url{ls.url};
			if(url.isValid()) {
				refreshServer(ls, url);
			} else {
				qWarning("Invalid list server URL: %s", qUtf8Printable(ls.url));
			}
		}
	}

	m_refreshTimer->start();
}

void Browse::updateJoinButton()
{
	QModelIndex index = m_listing->selectionModel()->currentIndex();
	emit enableJoin(canJoinIndex(index));
}

void Browse::refreshServer(
	const sessionlisting::ListServer &ls, const QUrl &url)
{
	sessionlisting::AnnouncementApiResponse *response =
		sessionlisting::getSessionList(url);
	response->setParent(this);
	m_refreshesInProgress.insert(ls.name, response);

	connect(
		response, &sessionlisting::AnnouncementApiResponse::serverGone,
		[urlString = ls.url]() {
			qInfo(
				"List server at %s is gone. Removing.",
				qUtf8Printable(urlString));
			sessionlisting::ListServerModel servers(dpApp().settings(), true);
			if(servers.removeServer(urlString)) {
				servers.submit();
			}
		});

	connect(
		response, &sessionlisting::AnnouncementApiResponse::finished, this,
		[this, name = ls.name](
			const QVariant &result, const QString &, const QString &error) {
			if(error.isEmpty()) {
				m_sessions->setList(
					name, result.value<QVector<sessionlisting::Session>>());
			} else {
				m_sessions->setMessage(name, error);
			}
		});

	connect(
		response, &sessionlisting::AnnouncementApiResponse::finished, this,
		[this, response, name = ls.name] {
			m_refreshesInProgress.remove(name);
			response->deleteLater();
		});
}

QAction *Browse::makeCopySessionDataAction(const QString &text, int role)
{
	QAction *action = m_listingContextMenu->addAction(text);
	connect(action, &QAction::triggered, this, [this, role] {
		QModelIndex index = m_listing->selectionModel()->currentIndex();
		if(isListingIndex(index)) {
			QGuiApplication::clipboard()->setText(index.data(role).toString());
		}
	});
	return action;
}

bool Browse::canJoinIndex(const QModelIndex &index)
{
	return isListingIndex(index) && index.flags().testFlag(Qt::ItemIsEnabled);
}

bool Browse::isListingIndex(const QModelIndex &index)
{
	return index.isValid() &&
		   index.data(SessionListingModel::IsListingRole).toBool();
}

}
}

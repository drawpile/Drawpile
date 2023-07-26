// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/browse.h"
#include "desktop/main.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/widgetutils.h"
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
#include <QScopedValueRollback>
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
		QIcon::fromTheme("dialog-information"), QStyle::PM_LargeIconSize,
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
	addPubLabel->setText(tr("To add the public Drawpile server, "
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

	m_duplicatesBox = new QCheckBox{tr("Duplicates")};
	m_duplicatesBox->setToolTip(
		tr("Show sessions that are listed on multiple servers"));
	filterLayout->addWidget(m_duplicatesBox);

	m_listing = new widgets::SpanAwareTreeView;
	m_listing->setAlternatingRowColors(true);
	m_listing->setRootIsDecorated(false);
	m_listing->setSortingEnabled(true);
	m_listing->header()->setStretchLastSection(false);
	m_listing->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_listing->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
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
				m_filteredSessions->refreshDuplicates();
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
		SessionListingModel::Server, QHeaderView::Interactive);
	header->setSectionResizeMode(
		SessionListingModel::UserCount, QHeaderView::Interactive);
	header->setSectionResizeMode(
		SessionListingModel::Owner, QHeaderView::Interactive);
	header->setSectionResizeMode(
		SessionListingModel::Uptime, QHeaderView::Interactive);
	// Don't sort by default. Otherwise it sorts by the first available column.
	header->setSortIndicator(-1, Qt::AscendingOrder);
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
	header->setSortIndicatorClearable(true);
#endif
	header->resizeSections(QHeaderView::ResizeToContents);
	header->resizeSection(SessionListingModel::Uptime, 80);
	header->resizeSection(SessionListingModel::Server, 125);

	connect(
		header, &QHeaderView::sectionResized, this,
		&Browse::cascadeSectionResize);
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
	settings.bindFilterDuplicates(m_duplicatesBox);
	settings.bindListServers(this, &Browse::updateListServers);

	m_filteredSessions->setShowClosed(m_closedBox->isChecked());
	m_filteredSessions->setShowPassworded(m_passwordBox->isChecked());
	m_filteredSessions->setShowNsfw(m_nsfmBox->isChecked());
	m_filteredSessions->setShowDuplicates(m_duplicatesBox->isChecked());

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
	connect(
		m_duplicatesBox, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowDuplicates);
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

void Browse::resizeEvent(QResizeEvent *event)
{
	Page::resizeEvent(event);
	m_listing->header()->setSectionResizeMode(
		SessionListingModel::Title, QHeaderView::Interactive);
	cascadeSectionResize(SessionListingModel::ColumnCount, 0, 0);
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
			m_sessions->setMessage(
				ls.name, QUrl{ls.url}.host(), tr("Loading..."));
		}
	}

	if(servers.isEmpty()) {
		m_noListServers->show();
		m_sessions->setMessage(
			tr("Nothing here yet!"), QString{},
			tr("Read the message at the top on how to add a server."));
	} else {
		m_noListServers->hide();
		refresh();
	}
	m_listing->expandAll();
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

void Browse::cascadeSectionResize(int logicalIndex, int oldSize, int newSize)
{
	Q_UNUSED(oldSize);
	if(!m_sectionFitInProgress) {
		QScopedValueRollback<bool> guard{m_sectionFitInProgress, true};
		utils::ScopedUpdateDisabler disabler{m_listing};
		QHeaderView *header = m_listing->header();

		int resizableWidths = 0;
		int fixedWidths = 0;
		for(int i = 0; i < SessionListingModel::ColumnCount; ++i) {
			if(i != logicalIndex) {
				int w = m_listing->columnWidth(i);
				if(header->sectionResizeMode(i) == QHeaderView::Interactive) {
					resizableWidths += w;
				} else {
					fixedWidths += w;
				}
			}
		}

		double ratios[SessionListingModel::ColumnCount];
		int largestIndex = -1;
		int largestWidth = -1;
		for(int i = 0; i < SessionListingModel::ColumnCount; ++i) {
			if(i != logicalIndex &&
			   header->sectionResizeMode(i) == QHeaderView::Interactive) {
				int w = m_listing->columnWidth(i);
				ratios[i] = double(w) / double(resizableWidths);
				if(w > largestWidth) {
					largestWidth = w;
					largestIndex = i;
				}
			}
		}

		int availableWidth = header->width();
		int widthToFill = availableWidth - newSize - fixedWidths;
		int widthFilled = 0;
		for(int i = 0; i < SessionListingModel::ColumnCount; ++i) {
			if(i != logicalIndex && i != largestIndex &&
			   header->sectionResizeMode(i) == QHeaderView::Interactive) {
				int w = qRound(double(widthToFill) * ratios[i]);
				m_listing->setColumnWidth(i, w);
				widthFilled += w;
			}
		}
		m_listing->setColumnWidth(largestIndex, widthToFill - widthFilled);

		if(logicalIndex != SessionListingModel::ColumnCount) {
			int actualWidth = 0;
			for(int i = 0; i < SessionListingModel::ColumnCount; ++i) {
				if(i != logicalIndex) {
					actualWidth += m_listing->columnWidth(i);
				}
			}
			m_listing->setColumnWidth(
				logicalIndex, availableWidth - actualWidth);
		}
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
		[this, name = ls.name, host = url.host()](
			const QVariant &result, const QString &, const QString &error) {
			if(error.isEmpty()) {
				m_sessions->setList(
					name, host,
					result.value<QVector<sessionlisting::Session>>());
			} else {
				m_sessions->setMessage(name, host, error);
			}
			m_filteredSessions->refreshDuplicates();
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

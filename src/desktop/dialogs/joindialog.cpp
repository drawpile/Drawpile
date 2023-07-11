// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/joindialog.h"
#include "desktop/dialogs/addserverdialog.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/main.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/utils/images.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/newversion.h"
#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/utils/usernamevalidator.h"
#include "libshared/listings/announcementapi.h"

#ifdef HAVE_DNSSD
#	include "libshared/listings/zeroconfdiscovery.h"
#endif

#include "ui_joindialog.h"

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QMenu>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace dialogs {

// How often the listing view should be refreshed (in seconds)
static const int REFRESH_INTERVAL = 60;

JoinDialog::JoinDialog(const QUrl &url, bool browse, QWidget *parent)
	: QDialog(parent)
	, m_lastRefresh(0)
	, m_autoJoin(false)
{
	m_ui = new Ui_JoinDialog;
	m_ui->setupUi(this);
	resetUrlPlaceholderText();

	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);
	connect(
		m_ui->buttons, &QDialogButtonBox::accepted, this, &JoinDialog::join);
	connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(
		m_ui->addServer, &QAbstractButton::clicked, this,
		&JoinDialog::addListServer);

	connect(
		m_ui->url, &QLineEdit::textChanged, this, &JoinDialog::addressChanged);
	connect(
		m_ui->autoRecord, &QAbstractButton::clicked, this,
		&JoinDialog::recordingToggled);

	// New version notification (cached)
	m_ui->newVersionNotification->setVisible(
		NewVersionCheck::isThereANewSeries(dpApp().settings()));

	// Session listing
	if(parentalcontrols::level() != parentalcontrols::Level::Unrestricted) {
		m_ui->showNsfw->setEnabled(false);
	}

	m_sessions = new SessionListingModel(this);

#ifdef HAVE_DNSSD
	if(ZeroconfDiscovery::isAvailable()) {
		auto zeroconfDiscovery = new ZeroconfDiscovery(this);
		m_sessions->setMessage(tr("Nearby"), tr("Loading..."));
		connect(
			zeroconfDiscovery, &ZeroconfDiscovery::serverListUpdated, this,
			[this](const QVector<sessionlisting::Session> &servers) {
				m_sessions->setList(tr("Nearby"), servers);
			});
		zeroconfDiscovery->discover();
	}
#endif

	const auto servers = sessionlisting::ListServerModel::listServers(
		dpApp().settings().listServers(), true);
	for(const sessionlisting::ListServer &ls : servers) {
		if(ls.publicListings) {
			m_sessions->setIcon(ls.name, ls.icon);
			m_sessions->setMessage(ls.name, tr("Loading..."));
		}
	}
	m_ui->noListServersNotification->setVisible(servers.isEmpty());

	m_filteredSessions = new SessionFilterProxyModel(this);
	m_filteredSessions->setSourceModel(m_sessions);
	m_filteredSessions->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_filteredSessions->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_filteredSessions->setFilterKeyColumn(-1);
	m_filteredSessions->setSortRole(Qt::UserRole);

	m_filteredSessions->setShowNsfw(false);
	m_filteredSessions->setShowPassworded(false);

	connect(
		m_ui->showPassworded, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowPassworded);
	connect(
		m_ui->showNsfw, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowNsfw);
	connect(
		m_ui->showClosed, &QAbstractButton::toggled, m_filteredSessions,
		&SessionFilterProxyModel::setShowClosed);
	connect(
		m_ui->filter, &QLineEdit::textChanged, m_filteredSessions,
		&SessionFilterProxyModel::setFilterFixedString);

	m_ui->listing->setModel(m_filteredSessions);
	m_ui->listing->expandAll();

	QHeaderView *header = m_ui->listing->header();
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
		m_ui->listing->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &JoinDialog::updateJoinButton);

	connect(
		m_ui->listing, &QTreeView::doubleClicked, this, &JoinDialog::joinIndex);

	m_listingContextMenu = new QMenu{this};
	m_ui->listing->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(
		m_ui->listing, &QWidget::customContextMenuRequested, this,
		&JoinDialog::showListingContextMenu);

	m_joinAction = m_listingContextMenu->addAction(tr("Join"));
	connect(m_joinAction, &QAction::triggered, this, [this]() {
		joinIndex(m_ui->listing->selectionModel()->currentIndex());
	});

	makeCopySessionDataAction(
		tr("Copy session URL"), SessionListingModel::UrlStringRole);
	makeCopySessionDataAction(tr("Copy title"), SessionListingModel::TitleRole);
	makeCopySessionDataAction(
		tr("Copy server"), SessionListingModel::ServerRole);
	makeCopySessionDataAction(tr("Copy owner"), SessionListingModel::OwnerRole);

	connect(
		m_ui->browseLinkLabel, &QLabel::linkActivated, this,
		&JoinDialog::activateBrowseTab);
	connect(
		m_ui->tabs, &QTabWidget::currentChanged, this, &JoinDialog::updateTab);
	if(browse) {
		activateBrowseTab();
	} else {
		updateTab(JOIN_TAB_INDEX);
	}

	restoreSettings();

	if(url.isEmpty()) {
		QString clipboardText = QApplication::clipboard()->text();
		bool clipboardHasValidUrl =
			clipboardText.startsWith("drawpile://", Qt::CaseInsensitive) &&
			QUrl::fromUserInput(clipboardText).isValid();
		if(clipboardHasValidUrl) {
			m_ui->url->setText(clipboardText);
		}
	} else {
		QUrl addListServer = QUrlQuery{url}.queryItemValue("list-server");
		if(addListServer.isValid() && !addListServer.isEmpty()) {
			activateBrowseTab();
			addListServerUrl(addListServer);
		} else {
			m_ui->url->setText(url.toString());
			m_autoJoin = !browse;
		}
	}

	// Periodically refresh the session listing
	auto refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &JoinDialog::refreshListing);
	refreshTimer->setSingleShot(false);
	refreshTimer->start(1000 * (REFRESH_INTERVAL + 1));
}

JoinDialog::~JoinDialog()
{
	delete m_ui;
}

void JoinDialog::resizeEvent(QResizeEvent *event)
{
	QDialog::resizeEvent(event);
	dpApp().settings().setLastJoinDialogSize(size());
}

bool JoinDialog::eventFilter(QObject *object, QEvent *event)
{
	if(event->type() == QEvent::MouseButtonDblClick) {
		QString recentHost = object->property("recenthost").toString();
		if(!recentHost.isEmpty()) {
			m_ui->url->setText(recentHost);
			m_address = recentHost.trimmed();
			accept();
			return true;
		}
	}
	return QDialog::eventFilter(object, event);
}


QString JoinDialog::cleanAddress(const QString &addr)
{
	if(addr.startsWith("drawpile://")) {
		QUrl url(addr);
		if(url.isValid()) {
			QString host = url.host();
			int port = url.port();
			if(port > 0 && port != cmake_config::proto::port()) {
				return QStringLiteral("%1:%2").arg(host).arg(port);
			} else {
				return host;
			}
		}
	}
	return addr;
}

void JoinDialog::activateBrowseTab()
{
	m_ui->tabs->setCurrentIndex(BROWSE_TAB_INDEX);
}

void JoinDialog::updateTab(int tabIndex)
{
	bool browse = tabIndex == BROWSE_TAB_INDEX;
	m_ui->addServer->setVisible(browse);
	if(browse) {
		m_ui->filter->setFocus();
		refreshListing();
	} else {
		m_ui->url->setFocus();
	}
	updateJoinButton();
}

void JoinDialog::resetUrlPlaceholderText()
{
	m_ui->url->setPlaceholderText(tr("Paste your drawpile://… link here!"));
}

void JoinDialog::updateJoinButton()
{
	QAbstractButton *joinButton = m_ui->buttons->button(QDialogButtonBox::Ok);
	if(m_ui->tabs->currentIndex() == BROWSE_TAB_INDEX) {
		QModelIndex index = m_ui->listing->selectionModel()->currentIndex();
		joinButton->setEnabled(canJoinIndex(index));
	} else {
		joinButton->setDisabled(
			m_ui->url->isReadOnly() || m_ui->url->text().trimmed().isEmpty());
	}
}

void JoinDialog::addressChanged(const QString &addr)
{
	// A roomcode is consists of exactly five uppercase ASCII letters.
	static QRegularExpression roomcodeRe{"\\A[A-Z]{5}\\z"};
	if(roomcodeRe.match(addr).hasMatch()) {
		// A room code was just entered. Trigger session URL query
		m_ui->url->setText(QString());
		m_ui->url->setPlaceholderText(tr("Searching..."));
		m_ui->url->setReadOnly(true);

		QStringList servers;
		for(const auto &s : sessionlisting::ListServerModel::listServers(
				dpApp().settings().listServers(), true)) {
			if(s.privateListings) {
				servers << s.url;
			}
		}
		resolveRoomcode(addr, servers);
	}
	updateJoinButton();
}

void JoinDialog::recordingToggled(bool checked)
{
	if(checked) {
		m_recordingFilename = QFileDialog::getSaveFileName(
			this, tr("Record"), m_recordingFilename,
			utils::fileFormatFilter(utils::FileFormatOption::SaveRecordings));
		if(m_recordingFilename.isEmpty()) {
			m_ui->autoRecord->setChecked(false);
		}
	}
}

QString JoinDialog::autoRecordFilename() const
{
	return m_ui->autoRecord->isChecked() ? m_recordingFilename : QString();
}

void JoinDialog::refreshListing()
{
	bool skipRefresh =
		m_ui->tabs->currentIndex() != BROWSE_TAB_INDEX ||
		m_ui->listing->isHidden() ||
		QDateTime::currentSecsSinceEpoch() - m_lastRefresh < REFRESH_INTERVAL;
	if(skipRefresh) {
		return;
	}
	m_lastRefresh = QDateTime::currentSecsSinceEpoch();

	const auto listservers = sessionlisting::ListServerModel::listServers(
		dpApp().settings().listServers(), true);
	for(const sessionlisting::ListServer &ls : listservers) {
		if(!ls.publicListings) {
			continue;
		}

		const QUrl url = ls.url;
		if(!url.isValid()) {
			qWarning("Invalid list server URL: %s", qPrintable(ls.url));
			continue;
		}

		auto response = sessionlisting::getSessionList(url);

		connect(
			response, &sessionlisting::AnnouncementApiResponse::serverGone,
			[ls]() {
				qInfo() << "List server at" << ls.url << "is gone. Removing.";
				sessionlisting::ListServerModel servers(
					dpApp().settings(), true);
				if(servers.removeServer(ls.url)) {
					servers.submit();
				}
			});
		connect(
			response, &sessionlisting::AnnouncementApiResponse::finished, this,
			[this, ls](
				const QVariant &result, const QString &message,
				const QString &error) {
				Q_UNUSED(message)
				if(error.isEmpty()) {
					m_sessions->setList(
						ls.name,
						result.value<QVector<sessionlisting::Session>>());
				} else {
					m_sessions->setMessage(ls.name, error);
				}
			});
		connect(
			response, &sessionlisting::AnnouncementApiResponse::finished,
			response, &QObject::deleteLater);
	}
}

void JoinDialog::resolveRoomcode(
	const QString &roomcode, const QStringList &servers)
{
	if(servers.isEmpty()) {
		// Tried all the servers and didn't find the code
		m_ui->url->setPlaceholderText(tr("Room code not found!"));
		QTimer::singleShot(1500, this, [this]() {
			m_ui->url->setText(QString());
			m_ui->url->setReadOnly(false);
			resetUrlPlaceholderText();
			m_ui->url->setFocus();
		});

		return;
	}

	const QUrl listServer = servers.first();
	qDebug() << "Querying join code" << roomcode << "at server:" << listServer;
	auto response = sessionlisting::queryRoomcode(listServer, roomcode);
	connect(
		response, &sessionlisting::AnnouncementApiResponse::finished, this,
		[this, roomcode, servers](
			const QVariant &result, const QString &message,
			const QString &error) {
			Q_UNUSED(message)
			if(!error.isEmpty()) {
				// Not found. Try the next server.
				resolveRoomcode(roomcode, servers.mid(1));
				return;
			}

			auto session = result.value<sessionlisting::Session>();

			QString url = "drawpile://" + session.host;
			if(session.port != 27750) {
				url += QStringLiteral(":%1").arg(session.port);
			}
			url += '/';
			url += session.id;
			m_ui->url->setReadOnly(false);
			resetUrlPlaceholderText();
			m_ui->url->setText(url);
			m_ui->url->setEnabled(true);
		});
	connect(
		response, &sessionlisting::AnnouncementApiResponse::finished, response,
		&QObject::deleteLater);
}

void JoinDialog::restoreSettings()
{
	auto &settings = dpApp().settings();

	const QSize oldSize = settings.lastJoinDialogSize();
	if(oldSize.isValid()) {
		resize(oldSize);
	}

	for(const QString &recentHost : settings.recentHosts()) {
		QLabel *label = new QLabel;
		label->setTextFormat(Qt::RichText);
		label->setWordWrap(true);
		label->setText(QStringLiteral("<a href=\"%1\">%1</a>")
						   .arg(recentHost.toHtmlEscaped()));
		QGraphicsOpacityEffect *opacity = new QGraphicsOpacityEffect;
		opacity->setOpacity(0.8);
		label->setGraphicsEffect(opacity);
		connect(label, &QLabel::linkActivated, m_ui->url, &QLineEdit::setText);
		label->installEventFilter(this);
		label->setProperty("recenthost", recentHost.trimmed());
		m_ui->recentScrollAreaLayout->addWidget(label);
	}
	m_ui->recentScrollAreaLayout->addStretch();

	settings.bindFilterLocked(m_ui->showPassworded);
	settings.bindFilterClosed(m_ui->showClosed);
	settings.bindFilterNsfm(m_ui->showNsfw);
}

void JoinDialog::rememberSettings() const
{
	desktop::settings::Settings &settings = dpApp().settings();

	QStringList hosts;
	int max = qMax(0, settings.maxRecentFiles());

	if(max > 0) {
		QString current = cleanAddress(m_address);
		if(!current.isEmpty()) {
			hosts.append(current);
		}

		for(const QString &host : settings.recentHosts()) {
			bool shouldAdd = !host.isEmpty() &&
							 !hosts.contains(host, Qt::CaseInsensitive) &&
							 hosts.size() < max;
			if(shouldAdd) {
				hosts.append(host);
			}
		}
	}

	settings.setRecentHosts(hosts);
}

QString JoinDialog::getAddress() const
{
	return m_address.trimmed();
}

QUrl JoinDialog::getUrl() const
{
	const QString address = getAddress();

	QString scheme;
	if(!address.startsWith("drawpile://")) {
		scheme = "drawpile://";
	}

	const QUrl url = QUrl(scheme + address, QUrl::TolerantMode);
	if(!url.isValid() || url.host().isEmpty()) {
		return QUrl();
	}

	return url;
}

void JoinDialog::autoJoin()
{
	if(m_autoJoin && m_ui->tabs->currentIndex() == JOIN_TAB_INDEX) {
		join();
	}
}

void JoinDialog::addListServer()
{
	addListServerUrl(QString{});
}

void JoinDialog::showListingContextMenu(const QPoint &pos)
{
	QModelIndex index = m_ui->listing->selectionModel()->currentIndex();
	if(isListingIndex(index)) {
		m_joinAction->setEnabled(canJoinIndex(index));
		m_listingContextMenu->popup(mapToGlobal(pos));
	}
}

void JoinDialog::join()
{
	if(m_ui->tabs->currentIndex() == BROWSE_TAB_INDEX) {
		QModelIndex index = m_ui->listing->selectionModel()->currentIndex();
		joinIndex(index);
	} else {
		m_address = m_ui->url->text().trimmed();
		if(!m_address.isEmpty()) {
			accept();
		}
	}
}

void JoinDialog::joinIndex(const QModelIndex &index)
{
	if(canJoinIndex(index)) {
		m_address = index.data(SessionListingModel::UrlStringRole).toString();
		accept();
	}
}

void JoinDialog::addListServerUrl(const QUrl &url)
{
	m_ui->addServer->setEnabled(false);

	auto *dlg = new AddServerDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	connect(dlg, &QObject::destroyed, [this]() {
		m_ui->addServer->setEnabled(true);
	});

	connect(
		dlg, &AddServerDialog::serverAdded, this,
		[this](const QString &name, const QIcon &icon) {
			m_sessions->setIcon(name, icon);
			m_sessions->setMessage(name, tr("Loading..."));
			m_ui->noListServersNotification->hide();

			const auto index = m_sessions->index(m_sessions->rowCount() - 1, 0);
			m_ui->listing->expandAll();
			m_ui->listing->scrollTo(index);

			m_lastRefresh = 0;
			refreshListing();
		});

	if(!url.isEmpty()) {
		dlg->query(url);
	}

	dlg->show();
}

QAction *JoinDialog::makeCopySessionDataAction(const QString &text, int role)
{
	QAction *action = m_listingContextMenu->addAction(text);
	connect(action, &QAction::triggered, this, [this, role] {
		QModelIndex index = m_ui->listing->selectionModel()->currentIndex();
		if(isListingIndex(index)) {
			QGuiApplication::clipboard()->setText(index.data(role).toString());
		}
	});
	return action;
}

bool JoinDialog::canJoinIndex(const QModelIndex &index)
{
	return isListingIndex(index) && index.flags().testFlag(Qt::ItemIsEnabled);
}

bool JoinDialog::isListingIndex(const QModelIndex &index)
{
	return index.isValid() &&
		   index.data(SessionListingModel::IsListingRole).toBool();
}

}

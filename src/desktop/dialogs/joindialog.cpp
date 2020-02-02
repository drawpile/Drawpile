/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "joindialog.h"

#include "net/sessionlistingmodel.h"
#include "utils/mandatoryfields.h"
#include "utils/usernamevalidator.h"
#include "utils/listservermodel.h"
#include "utils/sessionfilterproxymodel.h"
#include "utils/images.h"
#include "utils/newversion.h"
#include "utils/icon.h"
#include "../libshared/listings/announcementapi.h"
#include "../libshared/util/networkaccess.h"
#include "parentalcontrols/parentalcontrols.h"

#ifdef HAVE_DNSSD
#include "../libshared/listings/zeroconfdiscovery.h"
#endif

#include "ui_joindialog.h"

#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

namespace dialogs {

// Height below which the session listing widgets are hidden.
static const int COMPACT_MODE_THRESHOLD = 300;

// How often the listing view should be refreshed (in seconds)
static const int REFRESH_INTERVAL = 60;

JoinDialog::JoinDialog(const QUrl &url, QWidget *parent)
	: QDialog(parent), m_lastRefresh(0)
{
	m_ui = new Ui_JoinDialog;
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);

	m_addServerButton = m_ui->buttons->addButton(tr("Add"), QDialogButtonBox::ActionRole);
	m_addServerButton->setIcon(icon::fromTheme("list-add"));
	connect(m_addServerButton, &QPushButton::clicked, this, &JoinDialog::addListServer);

	if(!url.isEmpty())
		m_ui->address->setCurrentText(url.toString());

	connect(m_ui->address, &QComboBox::editTextChanged, this, &JoinDialog::addressChanged);
	connect(m_ui->autoRecord, &QAbstractButton::clicked, this, &JoinDialog::recordingToggled);

	// New version notification (cached)
	m_ui->newVersionNotification->setVisible(NewVersionCheck::isThereANewSeries());

	// Session listing
	if(parentalcontrols::level() != parentalcontrols::Level::Unrestricted)
		m_ui->showNsfw->setEnabled(false);

	m_sessions = new SessionListingModel(this);

#ifdef HAVE_DNSSD
	if(ZeroconfDiscovery::isAvailable()) {
		auto zeroconfDiscovery = new ZeroconfDiscovery(this);
		m_sessions->setMessage(tr("Nearby"), tr("Loading..."));
		connect(zeroconfDiscovery, &ZeroconfDiscovery::serverListUpdated, this, [this](const QVector<sessionlisting::Session> &servers) {
			m_sessions->setList(tr("Nearby"), servers);
		});
		zeroconfDiscovery->discover();
	}
#endif

	const auto servers = sessionlisting::ListServerModel::listServers(true);
	for(const sessionlisting::ListServer &ls : servers) {
		if(ls.publicListings)
			m_sessions->setMessage(ls.name, tr("Loading..."));
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

	connect(m_ui->showPassworded, &QAbstractButton::toggled,
			m_filteredSessions, &SessionFilterProxyModel::setShowPassworded);
	connect(m_ui->showNsfw, &QAbstractButton::toggled,
			m_filteredSessions, &SessionFilterProxyModel::setShowNsfw);
	connect(m_ui->showClosed, &QAbstractButton::toggled,
			m_filteredSessions, &SessionFilterProxyModel::setShowClosed);
	connect(m_ui->filter, &QLineEdit::textChanged,
			m_filteredSessions, &SessionFilterProxyModel::setFilterFixedString);

	m_ui->listing->setModel(m_filteredSessions);
	m_ui->listing->expandAll();

	QHeaderView *header = m_ui->listing->header();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(4, QHeaderView::ResizeToContents);

	connect(m_ui->listing, &QTreeView::clicked, this, [this](const QModelIndex &index) {
		// Set the server URL when clicking on an item
		if((index.flags() & Qt::ItemIsEnabled))
			m_ui->address->setCurrentText(index.data(SessionListingModel::UrlRole).value<QUrl>().toString());
	});

	connect(m_ui->listing, &QTreeView::doubleClicked, [this](const QModelIndex &index) {
		// Shortcut: double click to OK
		if((index.flags() & Qt::ItemIsEnabled) && m_ui->buttons->button(QDialogButtonBox::Ok)->isEnabled())
			accept();
	});

	new MandatoryFields(this, m_ui->buttons->button(QDialogButtonBox::Ok));

	restoreSettings();

	// Periodically refresh the session listing
	auto refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout,
			this, &JoinDialog::refreshListing);
	refreshTimer->setSingleShot(false);
	refreshTimer->start(1000 * (REFRESH_INTERVAL + 1));

	refreshListing();
}

JoinDialog::~JoinDialog()
{
	// Always remember these settings
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("filterlocked", m_ui->showPassworded->isChecked());
	cfg.setValue("filternsfw", m_ui->showNsfw->isChecked());
	cfg.setValue("filterclosed", m_ui->showClosed->isChecked());

	delete m_ui;
}

void JoinDialog::resizeEvent(QResizeEvent *event)
{
	QDialog::resizeEvent(event);
	bool show = false;
	bool change = false;
	if(height() < COMPACT_MODE_THRESHOLD && !m_ui->filter->isHidden()) {
		show = false;
		change = true;
	} else if(height() > COMPACT_MODE_THRESHOLD && m_ui->filter->isHidden()) {
		show = true;
		change = true;
	}

	if(change)
		setListingVisible(show);
}

void JoinDialog::setListingVisible(bool show)
{
	m_ui->filter->setVisible(show);
	m_ui->filterLabel->setVisible(show);
	m_ui->showPassworded->setVisible(show);
	m_ui->showNsfw->setVisible(show);
	m_ui->showClosed->setVisible(show);
	m_ui->listing->setVisible(show);
	m_ui->line->setVisible(show);

	if(show)
		refreshListing();
}

static QString cleanAddress(const QString &addr)
{
	if(addr.startsWith("drawpile://")) {
		QUrl url(addr);
		if(url.isValid()) {
			QString a = url.host();
			if(url.port()!=-1)
				a += ":" + QString::number(url.port());
			return a;
		}
	}
	return addr;
}

static bool isRoomcode(const QString &str) {
	// Roomcodes are always exactly 5 letters long
	if(str.length() != 5)
		return false;

	// And consist of characters in range A-Z
	for(int i=0;i<str.length();++i)
		if(str.at(i) < 'A' || str.at(i) > 'Z')
			return false;

	return true;
}

void JoinDialog::addressChanged(const QString &addr)
{
	m_addServerButton->setEnabled(!addr.isEmpty());

	if(isRoomcode(addr)) {
		// A room code was just entered. Trigger session URL query
		m_ui->address->setEditText(QString());
		m_ui->address->lineEdit()->setPlaceholderText(tr("Searching..."));
		m_ui->address->lineEdit()->setReadOnly(true);

		QStringList servers;
		for(const auto &s : sessionlisting::ListServerModel::listServers(true)) {
			if(s.privateListings)
				servers << s.url;
		}
		resolveRoomcode(addr, servers);
	}
}

void JoinDialog::recordingToggled(bool checked)
{
	if(checked) {
		m_recordingFilename = QFileDialog::getSaveFileName(
			this,
			tr("Record"),
			m_recordingFilename,
			utils::fileFormatFilter(utils::FileFormatOption::SaveRecordings)
		);
		if(m_recordingFilename.isEmpty())
			m_ui->autoRecord->setChecked(false);
	}
}

QString JoinDialog::autoRecordFilename() const
{
	return m_ui->autoRecord->isChecked() ? m_recordingFilename : QString();
}

void JoinDialog::refreshListing()
{
	if(m_ui->listing->isHidden() || QDateTime::currentSecsSinceEpoch() - m_lastRefresh < REFRESH_INTERVAL)
		return;
	m_lastRefresh = QDateTime::currentSecsSinceEpoch();

	auto listservers = sessionlisting::ListServerModel::listServers(true);
	for(const sessionlisting::ListServer &ls : listservers) {
		if(!ls.publicListings)
			continue;

		const QUrl url = ls.url;
		if(!url.isValid()) {
			qWarning("Invalid list server URL: %s", qPrintable(ls.url));
			continue;
		}

		auto response = sessionlisting::getSessionList(url);

		connect(response, &sessionlisting::AnnouncementApiResponse::serverGone,
			[ls]() {
				qInfo() << "List server at" << ls.url << "is gone. Removing.";
				sessionlisting::ListServerModel servers(true);
				if(servers.removeServer(ls.url))
					servers.saveServers();
			});
		connect(response, &sessionlisting::AnnouncementApiResponse::finished,
			this, [this, ls](const QVariant &result, const QString &message, const QString &error)
			{
				Q_UNUSED(message)
				if(error.isEmpty())
					m_sessions->setList(ls.name, result.value<QVector<sessionlisting::Session>>());
				else
					m_sessions->setMessage(ls.name, error);
			});
		connect(response, &sessionlisting::AnnouncementApiResponse::finished, response, &QObject::deleteLater);
	}
}

void JoinDialog::resolveRoomcode(const QString &roomcode, const QStringList &servers)
{
	if(servers.isEmpty()) {
		// Tried all the servers and didn't find the code
		m_ui->address->lineEdit()->setPlaceholderText(tr("Room code not found!"));
		QTimer::singleShot(1500, this, [this]() {
			m_ui->address->setEditText(QString());
			m_ui->address->lineEdit()->setReadOnly(false);
			m_ui->address->lineEdit()->setPlaceholderText(QString());
			m_ui->address->setFocus();
		});

		return;
	}

	const QUrl listServer = servers.first();
	qDebug() << "Querying join code" << roomcode << "at server:" << listServer;
	auto response = sessionlisting::queryRoomcode(listServer, roomcode);
	connect(response, &sessionlisting::AnnouncementApiResponse::finished,
		this, [this, roomcode, servers](const QVariant &result, const QString &message, const QString &error)
		{
			Q_UNUSED(message)
			if(!error.isEmpty()) {
				// Not found. Try the next server.
				resolveRoomcode(roomcode, servers.mid(1));
				return;
			}

			auto session = result.value<sessionlisting::Session>();

			QString url = "drawpile://" + session.host;
			if(session.port != 27750)
				url += QStringLiteral(":%1").arg(session.port);
			url += '/';
			url += session.id;
			m_ui->address->lineEdit()->setReadOnly(false);
			m_ui->address->lineEdit()->setPlaceholderText(QString());
			m_ui->address->setEditText(url);
			m_ui->address->setEnabled(true);
		}
	);
	connect(response, &sessionlisting::AnnouncementApiResponse::finished, response, &QObject::deleteLater);
}

void JoinDialog::restoreSettings()
{
	QSettings cfg;
	cfg.beginGroup("history");

	const QSize oldSize = cfg.value("joindlgsize").toSize();
	if(oldSize.isValid()) {
		if(oldSize.height() < COMPACT_MODE_THRESHOLD)
			setListingVisible(false);
		resize(oldSize);
	}

	m_ui->address->insertItems(0, cfg.value("recenthosts").toStringList());

	m_ui->showPassworded->setChecked(cfg.value("filterlocked", true).toBool());
	m_ui->showClosed->setChecked(cfg.value("filterclosed", true).toBool());

	if(m_ui->showNsfw->isEnabled())
		m_ui->showNsfw->setChecked(cfg.value("filternsfw", true).toBool());
	else
		m_ui->showNsfw->setChecked(false);

	const int sortColumn = cfg.value("listsortcol", 1).toInt();
	m_ui->listing->sortByColumn(
		qAbs(sortColumn)-1,
		sortColumn > 0 ? Qt::AscendingOrder : Qt::DescendingOrder
	);
}

void JoinDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");

	cfg.setValue("joindlgsize", size());

	QStringList hosts;
	// Move current item to the top of the list
	const QString current = cleanAddress(m_ui->address->currentText());
	int curindex = m_ui->address->findText(current);
	if(curindex>=0)
		m_ui->address->removeItem(curindex);
	hosts << current;
	for(int i=0;i<qMin(8, m_ui->address->count());++i) {
		if(!m_ui->address->itemText(i).isEmpty())
			hosts << m_ui->address->itemText(i);
	}
	cfg.setValue("recenthosts", hosts);

	const auto *listingHeader = m_ui->listing->header();

	cfg.setValue("listsortcol",
		(listingHeader->sortIndicatorSection() + 1) *
		(listingHeader->sortIndicatorOrder() == Qt::AscendingOrder ? 1 : -1)
	);
}

QString JoinDialog::getAddress() const {
	return m_ui->address->currentText().trimmed();
}

QUrl JoinDialog::getUrl() const
{
	const QString address = getAddress();

	QString scheme;
	if(!address.startsWith("drawpile://"))
		scheme = "drawpile://";

	const QUrl url = QUrl(scheme + address, QUrl::TolerantMode);
	if(!url.isValid() || url.host().isEmpty())
		return QUrl();

	return url;
}

void JoinDialog::addListServer()
{
	// This is the "simplified" way of adding list servers:
	// The application will fetch the server's root page (http://DOMAIN/)
	// and see if there is a <meta name="drawpile:list-server"> tag.
	// If there is, it will follow it and add the list server.
	QString urlStr = m_ui->address->currentText();
	QUrl url;
	if(!urlStr.contains('/')) {
		url = QUrl { "http://" + urlStr };
	} else {
		url = QUrl { "http://" + QUrl{urlStr}.host() };
	}

	m_addServerButton->setEnabled(false);

	qInfo() << "Querying" << url << "for list server info...";

	auto *response = sessionlisting::getApiInfo(url);
	connect(response, &sessionlisting::AnnouncementApiResponse::finished, this, [this, response](const QVariant &result, const QString&, const QString &error) {
		m_addServerButton->setEnabled(true);
		if(!error.isEmpty()) {
			QMessageBox::warning(this, tr("Add listing server"), error);
			return;
		}

		const auto info = result.value<sessionlisting::ListServerInfo>();
		const QString apiUrl = response->apiUrl().toString();

		sessionlisting::ListServerModel listservers(true, this);

		// Don't overwrite any existing server here.
		// That can be done in the preferences dialog.
		if(!listservers.addServer(
			info.name,
			apiUrl,
			info.description,
			info.readOnly,
			info.publicListings,
			info.privateListings
		)) {
			qInfo() << apiUrl << "List server already added.";
			return;
		}

		if(info.faviconUrl == "drawpile") {
			listservers.setFavicon(
				apiUrl,
				QIcon(":/icons/drawpile.png").pixmap(128, 128).toImage()
				);

		} else {
			const QUrl favicon(info.faviconUrl);
			if(favicon.isValid()) {
				auto *filedownload = new networkaccess::FileDownload(this);

				filedownload->setExpectedType("image/");

				connect(filedownload, &networkaccess::FileDownload::finished, this, [filedownload, apiUrl](const QString &errorMessage) {
					if(!errorMessage.isEmpty()) {
						qWarning("Couldnt' fetch favicon: %s", qPrintable(errorMessage));
						return;
					}

					QImage image;
					if(!image.load(filedownload->file(), nullptr)) {
						qWarning("Couldn't load favicon.");
						return;
					}

					sessionlisting::ListServerModel listservers(true);
					listservers.setFavicon(apiUrl, image);
					listservers.saveServers();
				});
				connect(filedownload, &networkaccess::FileDownload::finished, filedownload, &QObject::deleteLater);

				filedownload->start(favicon);
			}
		}

		listservers.saveServers();
		m_sessions->setMessage(info.name, tr("Loading..."));
		m_ui->noListServersNotification->hide();

		if(height() < COMPACT_MODE_THRESHOLD)
			resize(width(), COMPACT_MODE_THRESHOLD + 10);

		QTimer::singleShot(1, [this]() {
			const auto index = m_sessions->index(m_sessions->rowCount()-1, 0);
			m_ui->listing->expandAll();
			m_ui->listing->scrollTo(index);
		});

		m_lastRefresh = 0;
		refreshListing();
	});
	connect(response, &sessionlisting::AnnouncementApiResponse::finished, response, &QObject::deleteLater);
}

}


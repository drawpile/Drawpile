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
#include "../shared/util/announcementapi.h"
#include "parentalcontrols/parentalcontrols.h"

#ifdef HAVE_DNSSD
#include "net/serverdiscoverymodel.h"
#endif

#include "widgets/spinner.h"
using widgets::Spinner;

#include "ui_joindialog.h"

#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QDebug>
#include <QFileDialog>

namespace dialogs {

enum {
	LIST_PAGE_LOADING = 0,
	LIST_PAGE_LISTING,
	LIST_PAGE_ERROR
};

JoinDialog::JoinDialog(const QUrl &url, QWidget *parent)
	: QDialog(parent),
	m_localServers(nullptr)
{
	m_ui = new Ui_JoinDialog;
	m_ui->setupUi(this);
	m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Join"));
	m_ui->buttons->button(QDialogButtonBox::Ok)->setDefault(true);

	if(!url.isEmpty())
		m_ui->address->setCurrentText(url.toString());

	connect(m_ui->address, &QComboBox::editTextChanged, this, &JoinDialog::addressChanged);
	connect(m_ui->autoRecord, &QAbstractButton::clicked, this, &JoinDialog::recordingToggled);

	// Session listing
	if(parentalcontrols::level() != parentalcontrols::Level::Unrestricted)
		m_ui->filterNsfw->setEnabled(false);

	m_ui->listserver->setModel(new sessionlisting::ListServerModel(sessionlisting::ListServerModel::ShowLocal | sessionlisting::ListServerModel::ShowBlank, this));

	m_sessions = new SessionListingModel(this);
#ifdef HAVE_DNSSD
	m_localServers = new ServerDiscoveryModel(this);
#endif

	m_filteredSessions = new SessionFilterProxyModel(this);
	m_filteredSessions->setSourceModel(m_sessions);
	m_filteredSessions->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_filteredSessions->setFilterKeyColumn(-1);
	m_filteredSessions->setSortRole(Qt::UserRole);

	m_filteredSessions->setShowNsfw(false);
	m_filteredSessions->setShowPassworded(false);

	connect(m_ui->filterLocked, &QAbstractButton::toggled,
			m_filteredSessions, &SessionFilterProxyModel::setShowPassworded);
	connect(m_ui->filterNsfw, &QAbstractButton::toggled,
			m_filteredSessions, &SessionFilterProxyModel::setShowNsfw);
	connect(m_ui->filter, &QLineEdit::textChanged,
			m_filteredSessions, &SessionFilterProxyModel::setFilterFixedString);

	m_ui->listing->setModel(m_filteredSessions);
	QHeaderView *header = m_ui->listing->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(4, QHeaderView::ResizeToContents);

	// Periodically refresh the session listing
	auto refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout,
			this, &JoinDialog::refreshListing);
	refreshTimer->setSingleShot(false);
	refreshTimer->start(1000 * 60);

	connect(m_ui->listing, &QTableView::clicked, this, [this](const QModelIndex &index) {
		// Set the server URL when clicking on an item
		if((index.flags() & Qt::ItemIsEnabled))
			m_ui->address->setCurrentText(index.data(SessionListingModel::UrlRole).value<QUrl>().toString());
	});

	connect(m_ui->listing, &QTableView::doubleClicked, [this](const QModelIndex &index) {
		// Shortcut: double click to OK
		if((index.flags() & Qt::ItemIsEnabled) && m_ui->buttons->button(QDialogButtonBox::Ok)->isEnabled())
			accept();
	});

	new MandatoryFields(this, m_ui->buttons->button(QDialogButtonBox::Ok));

	restoreSettings();

	connect(m_ui->listserver, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &JoinDialog::refreshListing);
	refreshListing();
}

JoinDialog::~JoinDialog()
{
	// Always remember these settings
	QSettings cfg;
	cfg.beginGroup("history");
	cfg.setValue("listingserverlast", m_ui->listserver->currentIndex());
	cfg.setValue("filterlocked", m_ui->filterLocked->isChecked());
	cfg.setValue("filternsfw", m_ui->filterNsfw->isChecked());

	delete m_ui;
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
	if(isRoomcode(addr)) {
		// A room code was just entered. Trigger session URL query
		m_ui->address->setEditText(QString());
		m_ui->address->lineEdit()->setPlaceholderText(tr("Searching..."));
		m_ui->address->lineEdit()->setReadOnly(true);

		QStringList servers;
		for(const auto &s : sessionlisting::ListServerModel::listServers()) {
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
	const QString urlstr = m_ui->listserver->currentData().toString();
	qDebug() << "Fetching session list from" << urlstr;

	const bool showList = !urlstr.isEmpty();
	const bool showNsfw = parentalcontrols::level() == parentalcontrols::Level::Unrestricted;

	m_ui->filter->setEnabled(showList);
	m_ui->filterLocked->setEnabled(showList);
	m_ui->filterNsfw->setEnabled(showList && showNsfw);

	if(!showList) {
		// Hide listing and collapse dialog
		m_ui->liststack->hide();
		if(isVisible())
			QTimer::singleShot(0, this, [this]() {
				resize(width(), 0);
				});
		else
			resize(width(), 0);

		return;
	}

	m_ui->liststack->setCurrentIndex(LIST_PAGE_LOADING);
	m_ui->liststack->show();

	if(urlstr == "local") {
#ifdef HAVE_DNSSD
		// Local server discovery mode (DNS-SD)
		m_filteredSessions->setSourceModel(m_localServers);
		m_ui->liststack->setCurrentIndex(LIST_PAGE_LISTING);
		m_localServers->discover();
#else
		// Shouldn't happen
		setListingError("DNS-SD support not compiled in");
#endif

	} else {
		// Listing server mode
		const QUrl url = urlstr;
		if(!url.isValid()) {
			setListingError("Invalid list server URL");
			return;
		}
		m_filteredSessions->setSourceModel(m_sessions);

		auto response = sessionlisting::getSessionList(
			url,
			protocol::ProtocolVersion::current().asString(),
			QString(),
			showNsfw
			);

		connect(response, &sessionlisting::AnnouncementApiResponse::finished,
			this, [this](const QVariant &result, const QString &message, const QString &error)
			{
				Q_UNUSED(message);
				if(error.isEmpty()) {
					m_sessions->setList(result.value<QList<sessionlisting::Session>>());
					m_ui->liststack->setCurrentIndex(LIST_PAGE_LISTING);
				} else {
					setListingError(error);
				}
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
			Q_UNUSED(message);
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
	m_ui->address->insertItems(0, cfg.value("recenthosts").toStringList());

	m_ui->listserver->setCurrentIndex(cfg.value("listingserverlast", 0).toInt());

	m_ui->filterLocked->setChecked(cfg.value("filterlocked").toBool());

	if(m_ui->filterNsfw->isEnabled())
		m_ui->filterNsfw->setChecked(cfg.value("filternsfw").toBool());
	else
		m_ui->filterNsfw->setChecked(false);
}

void JoinDialog::rememberSettings() const
{
	QSettings cfg;
	cfg.beginGroup("history");

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

void JoinDialog::setListingError(const QString &message)
{
	m_ui->listingError->setText(message);
	m_ui->liststack->setCurrentIndex(LIST_PAGE_ERROR);
}

}


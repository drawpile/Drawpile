/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "sessionlistingdialog.h"
#include "net/sessionlistingmodel.h"
#include "ui_sessionlisting.h"
#include "utils/listservermodel.h"
#include "parentalcontrols/parentalcontrols.h"

#ifdef HAVE_DNSSD
#include "net/serverdiscoverymodel.h"
#endif

#include <QTimer>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QMessageBox>

#include "config.h"

namespace dialogs {

SessionListingDialog::SessionListingDialog(QWidget *parent)
	: QDialog(parent)
{
	m_ui = new Ui_SessionListingDialog;
	m_ui->setupUi(this);

	m_ui->nsfmSessionsLabel->setVisible(false);

	connect(m_ui->nsfmSessionsLabel, &QLabel::linkActivated, this, &SessionListingDialog::stopNsfmFiltering);

	QPushButton *ok = m_ui->buttonBox->button(QDialogButtonBox::Ok);
	ok->setEnabled(false);

	m_ui->listserver->setModel(new sessionlisting::ListServerModel(true, this));
	m_ui->listserver->setCurrentIndex(QSettings().value("history/listingserverlast", 0).toInt());
	connect(m_ui->listserver, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SessionListingDialog::refreshListing);

	m_sessions = new sessionlisting::SessionListingModel(this);
	m_sessions->setShowNsfm(parentalcontrols::level() == parentalcontrols::Level::Unrestricted);

#ifdef HAVE_DNSSD
	m_localServers = new ServerDiscoveryModel(this);
#endif
	m_apiClient = new sessionlisting::AnnouncementApi(this);

	connect(m_apiClient, &sessionlisting::AnnouncementApi::sessionListReceived, [this](QList<sessionlisting::Session> list) {
		m_ui->liststack->setCurrentIndex(0);
		m_sessions->setList(list);
		int filtered = m_sessions->filteredCount();
		if(filtered>0) {
			QString label = tr("%n age restricted session(s) hidden.", "", filtered);
			if(!parentalcontrols::isLocked())
				label = "<a href=\"#\">" + label  + "</a>";
			m_ui->nsfmSessionsLabel->setText(label);
		}
		m_ui->nsfmSessionsLabel->setVisible(filtered>0);
	});
	connect(m_apiClient, &sessionlisting::AnnouncementApi::error, [this](const QString &message) {
		m_ui->liststack->setCurrentIndex(1);
		m_ui->errormessage->setText(message);
	});

	m_model = new QSortFilterProxyModel(this);
	m_model->setSourceModel(m_sessions);
	m_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_model->setFilterKeyColumn(-1);
	m_model->setSortRole(Qt::UserRole);

	connect(m_ui->filter, &QLineEdit::textChanged, m_model, &QSortFilterProxyModel::setFilterFixedString);

	m_ui->listing->setModel(m_model);
	QHeaderView *header = m_ui->listing->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(4, QHeaderView::ResizeToContents);

	QTimer *refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &SessionListingDialog::refreshListing);
	refreshTimer->setSingleShot(false);
	refreshTimer->start(1000 * 60);
	refreshListing();

	connect(this, &QDialog::accepted, [this]() {
		// Emit selection when OK is clicked		
		QModelIndex sel = m_ui->listing->selectionModel()->selectedIndexes().at(0);
		QUrl url = sel.data(Qt::UserRole+1).value<QUrl>();

		emit selected(url);
	});

	connect(m_ui->listing->selectionModel(), &QItemSelectionModel::selectionChanged, [ok](const QItemSelection &sel) {
		ok->setEnabled(!sel.indexes().isEmpty());
	});

	connect(m_ui->listing, &QTableView::doubleClicked, [this, ok](const QModelIndex &) {
		// Shortcut: double click to OK
		if(ok->isEnabled())
			accept();
	});
}

SessionListingDialog::~SessionListingDialog()
{
	QSettings cfg;
	cfg.setValue("history/listingserverlast", m_ui->listserver->currentIndex());
	delete m_ui;
}

void SessionListingDialog::refreshListing()
{
	QString urlstr = m_ui->listserver->itemData(m_ui->listserver->currentIndex()).toString();
	if(urlstr == "local") {
#ifdef HAVE_DNSSD
		// Local server discovery mode (DNS-SD)
		m_model->setSourceModel(m_localServers);
		m_ui->nsfmSessionsLabel->setVisible(false);
		m_localServers->discover();
#endif

	} else {
		// Session listing server mode
		QUrl url = urlstr;
		if(url.isValid()) {
			m_model->setSourceModel(m_sessions);
			m_apiClient->getSessionList(url, protocol::ProtocolVersion::current().asString(), QString(), true);
		}
	}
}

void SessionListingDialog::stopNsfmFiltering()
{
	QMessageBox::StandardButton btn = QMessageBox::question(this, QString(), tr("Show age restricted sessions?"));

	if(btn == QMessageBox::Yes) {
		m_sessions->setShowNsfm(true);
		m_ui->nsfmSessionsLabel->setVisible(false);
		QSettings().setValue("pc/level", int(parentalcontrols::Level::Unrestricted));
	}
}

}


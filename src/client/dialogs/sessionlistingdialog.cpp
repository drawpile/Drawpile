/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
	_ui = new Ui_SessionListingDialog;
	_ui->setupUi(this);

	_ui->nsfmSessionsLabel->setVisible(false);

	connect(_ui->nsfmSessionsLabel, &QLabel::linkActivated, this, &SessionListingDialog::stopNsfmFiltering);

	QPushButton *ok = _ui->buttonBox->button(QDialogButtonBox::Ok);
	ok->setEnabled(false);

	_ui->listserver->setModel(new sessionlisting::ListServerModel(true, this));
	_ui->listserver->setCurrentIndex(QSettings().value("history/listingserverlast", 0).toInt());
	connect(_ui->listserver, SIGNAL(currentIndexChanged(int)), this, SLOT(refreshListing()));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	// move this to the .ui file when we no longer support Qt <5.2
	_ui->filter->setClearButtonEnabled(true);
#endif

	_sessions = new sessionlisting::SessionListingModel(this);
	_sessions->setShowNsfm(QSettings().value("listservers/nsfm", false).toBool());

#ifdef HAVE_DNSSD
	_localservers = new ServerDiscoveryModel(this);
#endif
	_apiClient = new sessionlisting::AnnouncementApi(this);

	connect(_apiClient, &sessionlisting::AnnouncementApi::sessionListReceived, [this](QList<sessionlisting::Session> list) {
		_ui->liststack->setCurrentIndex(0);
		_sessions->setList(list);
		int filtered = _sessions->filteredCount();
		if(filtered>0)
			_ui->nsfmSessionsLabel->setText("<a href=\"#\">" + tr("%n age restricted session(s) hidden.", "", filtered) + "</a>");
		_ui->nsfmSessionsLabel->setVisible(filtered>0);
	});
	connect(_apiClient, &sessionlisting::AnnouncementApi::error, [this](const QString &message) {
		_ui->liststack->setCurrentIndex(1);
		_ui->errormessage->setText(message);
	});

	_model = new QSortFilterProxyModel(this);
	_model->setSourceModel(_sessions);
	_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
	_model->setFilterKeyColumn(-1);

	connect(_ui->filter, &QLineEdit::textChanged, _model, &QSortFilterProxyModel::setFilterFixedString);

	_ui->listing->setModel(_model);
	QHeaderView *header = _ui->listing->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::Stretch);
	header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(3, QHeaderView::ResizeToContents);

	QTimer *refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &SessionListingDialog::refreshListing);
	refreshTimer->setSingleShot(false);
	refreshTimer->start(1000 * 60);
	refreshListing();

	connect(this, &QDialog::accepted, [this]() {
		// Emit selection when OK is clicked		
		QModelIndex sel = _ui->listing->selectionModel()->selectedIndexes().at(0);
		QUrl url = sel.data(Qt::UserRole+1).value<QUrl>();

		emit selected(url);
	});

	connect(_ui->listing->selectionModel(), &QItemSelectionModel::selectionChanged, [ok](const QItemSelection &sel) {
		ok->setEnabled(!sel.indexes().isEmpty());
	});

	connect(_ui->listing, &QTableView::doubleClicked, [this, ok](const QModelIndex &) {
		// Shortcut: double click to OK
		if(ok->isEnabled())
			accept();
	});
}

SessionListingDialog::~SessionListingDialog()
{
	QSettings cfg;
	cfg.setValue("history/listingserverlast", _ui->listserver->currentIndex());
	delete _ui;
}

void SessionListingDialog::refreshListing()
{
	QString urlstr = _ui->listserver->itemData(_ui->listserver->currentIndex()).toString();
	if(urlstr == "local") {
#ifdef HAVE_DNSSD
		// Local server discovery mode (DNS-SD)
		_model->setSourceModel(_localservers);
		_ui->nsfmSessionsLabel->setVisible(false);
		_localservers->discover();
#endif

	} else {
		// Session listing server mode
		QUrl url = urlstr;
		if(url.isValid()) {
			_model->setSourceModel(_sessions);
			_apiClient->getSessionList(url, DRAWPILE_PROTO_STR, QString(), true);
		}
	}
}

void SessionListingDialog::stopNsfmFiltering()
{
	QMessageBox::StandardButton btn = QMessageBox::question(this, QString(), tr("Show age restricted sessions?"));

	if(btn == QMessageBox::Yes) {
		_sessions->setShowNsfm(true);
		_ui->nsfmSessionsLabel->setVisible(false);
		QSettings cfg;
		cfg.setValue("listservers/nsfm", true);
	}
}

}


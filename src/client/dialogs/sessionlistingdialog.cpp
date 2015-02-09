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

#include <QTimer>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "config.h"

namespace dialogs {

SessionListingDialog::SessionListingDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_SessionListingDialog;
	_ui->setupUi(this);

	QPushButton *ok = _ui->buttonBox->button(QDialogButtonBox::Ok);
	ok->setEnabled(false);

	_ui->listserver->setModel(new sessionlisting::ListServerModel(this));
	_ui->listserver->setCurrentIndex(QSettings().value("history/listingserver", 0).toInt());
	connect(_ui->listserver, SIGNAL(currentIndexChanged(int)), this, SLOT(refreshListing()));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	// move this to the .ui file when we no longer support Qt <5.2
	_ui->filter->setClearButtonEnabled(true);
#endif

	_model = new sessionlisting::SessionListingModel(this);
	_apiClient = new sessionlisting::AnnouncementApi(this);

	//connect(_apiClient, &sessionlisting::AnnouncementApi::sessionListReceived, _model, &sessionlisting::SessionListingModel::setList);
	connect(_apiClient, &sessionlisting::AnnouncementApi::sessionListReceived, [this](QList<sessionlisting::Session> list) {
		_ui->liststack->setCurrentIndex(0);
		_model->setList(list);
	});
	connect(_apiClient, &sessionlisting::AnnouncementApi::error, [this](const QString &message) {
		_ui->liststack->setCurrentIndex(1);
		_ui->errormessage->setText(message);
	});

	QSortFilterProxyModel *filteredModel = new QSortFilterProxyModel(this);
	filteredModel->setSourceModel(_model);
	filteredModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	filteredModel->setFilterKeyColumn(-1);

	connect(_ui->filter, &QLineEdit::textChanged, filteredModel, &QSortFilterProxyModel::setFilterFixedString);

	_ui->listing->setModel(filteredModel);
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
		int row = _ui->listing->selectionModel()->selectedIndexes().at(0).row();
		QUrl url = _model->sessionUrl(row);
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
	cfg.setValue("history/listingserver", _ui->listserver->currentIndex());
	delete _ui;
}

#define PROTO_STR(x) #x
#define XPROTO_STR(x) PROTO_STR(x)

void SessionListingDialog::refreshListing()
{
	QUrl url = _ui->listserver->currentData().toString();
	if(url.isValid()) {
		_apiClient->setApiUrl(url);
		const QString version = QStringLiteral(XPROTO_STR(DRAWPILE_PROTO_MAJOR_VERSION) "." XPROTO_STR(DRAWPILE_PROTO_MINOR_VERSION));
		_apiClient->getSessionList(version);
	}
}

}

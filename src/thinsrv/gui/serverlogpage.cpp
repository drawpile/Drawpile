/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "thinsrv/gui/serverlogpage.h"
#include "thinsrv/gui/serverlogmodel.h"
#include "thinsrv/gui/subheaderwidget.h"
#include "thinsrv/gui/server.h"

#include "ui_ipbandialog.h"

#include <QDebug>
#include <QBoxLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QTableView>
#include <QHeaderView>
#include <QTimer>

namespace server {
namespace gui {

static const QString REQ_ID = "serverlog";

struct ServerLogPage::Private {
	Server *server;
	ServerLogModel *model;
	QTableView *view;
	QTimer *refreshTimer;
};

ServerLogPage::ServerLogPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;
	d->model = new ServerLogModel(this);

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("Server log"), 1));

	d->view = new QTableView;
	d->view->setModel(d->model);
	d->view->horizontalHeader()->setStretchLastSection(true);
	d->view->setSelectionMode(QTableView::SingleSelection);
	d->view->setSelectionBehavior(QTableView::SelectRows);

	layout->addWidget(d->view);

	connect(server, &Server::apiResponse, this, &ServerLogPage::handleResponse);

	d->refreshTimer = new QTimer(this);
	d->refreshTimer->setSingleShot(true);
	connect(d->refreshTimer, &QTimer::timeout, this, &ServerLogPage::refreshPage);

	refreshPage();
}

ServerLogPage::~ServerLogPage()
{
	delete d;
}

void ServerLogPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId == REQ_ID) {
		if(result.status == JsonApiResult::Ok) {
			d->model->addLogEntries(result.body.array());
			d->refreshTimer->start(5000);
		}
	}
}

void ServerLogPage::refreshPage()
{
	QJsonObject q;
	QString after = d->model->lastTimestamp();
	if(!after.isEmpty())
		q["after"] = after;
	d->server->makeApiRequest(REQ_ID, JsonApiMethod::Get, QStringList() << "log", q);
}

}
}


// SPDX-License-Identifier: GPL-3.0-or-later

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


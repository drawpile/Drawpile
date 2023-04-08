// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/userlistpage.h"
#include "thinsrv/gui/subheaderwidget.h"
#include "thinsrv/gui/userlistmodel.h"
#include "thinsrv/gui/server.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QBoxLayout>
#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QTimer>

namespace server {
namespace gui {

static const QString REQ_ID = "userlist";

struct UserListPage::Private {
	Server *server;
	QTimer *refreshTimer;

	UserListModel *userlist;
};

UserListPage::UserListPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;
	d->userlist = new UserListModel(this);

	d->refreshTimer = new QTimer(this);
	d->refreshTimer->setSingleShot(false);
	d->refreshTimer->setInterval(15 * 1000);
	connect(d->refreshTimer, &QTimer::timeout, this, &UserListPage::refreshPage);
	d->refreshTimer->start(15 * 1000);

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("Users"), 1));

	QTableView *userview = new QTableView;
	userview->setModel(d->userlist);
	userview->horizontalHeader()->setStretchLastSection(true);
	userview->setSelectionMode(QTableView::SingleSelection);
	userview->setSelectionBehavior(QTableView::SelectRows);

	layout->addWidget(userview);

	connect(server, &Server::apiResponse, this, &UserListPage::handleResponse);
	refreshPage();
}

UserListPage::~UserListPage()
{
	delete d;
}

void UserListPage::refreshPage()
{

	d->server->makeApiRequest(REQ_ID, JsonApiMethod::Get, QStringList() << "users", QJsonObject());
}

void UserListPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId != REQ_ID)
		return;

	d->userlist->setList(result.body.array());
}

}
}

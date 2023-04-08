// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/banlistpage.h"
#include "thinsrv/gui/banlistmodel.h"
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

static const QString REQ_ID = "banlist";
static const QString ADD_REQ_ID = "banlistAdd";
static const QString DEL_REQ_ID = "banlistDel";

struct BanListPage::Private {
	Server *server;
	BanListModel *model;
	QTableView *view;
};

BanListPage::BanListPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;
	d->model = new BanListModel(this);

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("IP bans"), 1));

	d->view = new QTableView;
	d->view->setModel(d->model);
	d->view->horizontalHeader()->setStretchLastSection(true);
	d->view->setSelectionMode(QTableView::SingleSelection);
	d->view->setSelectionBehavior(QTableView::SelectRows);

	layout->addWidget(d->view);

	{
		auto *buttons = new QHBoxLayout;

		auto *addButton = new QPushButton(tr("Add"), this);
		connect(addButton, &QPushButton::clicked, this, &BanListPage::addNewBan);
		buttons->addWidget(addButton);

		auto *rmButton = new QPushButton(tr("Remove"), this);
		connect(rmButton, &QPushButton::clicked, this, &BanListPage::removeSelectedBan);
		buttons->addWidget(rmButton);

		buttons->addStretch(1);
		layout->addLayout(buttons);
	}

	connect(server, &Server::apiResponse, this, &BanListPage::handleResponse);
	refreshPage();
}

BanListPage::~BanListPage()
{
	delete d;
}

void BanListPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId == REQ_ID) {
		if(result.status == JsonApiResult::Ok)
			d->model->setList(result.body.array());

	} else if(requestId == ADD_REQ_ID) {
		if(result.status == JsonApiResult::BadRequest)
			QMessageBox::warning(this, tr("Error"), result.body.object()["message"].toString());
		else if(result.status == JsonApiResult::Ok)
			d->model->addBanEntry(result.body.object());

	} else if(requestId == DEL_REQ_ID && result.status == JsonApiResult::Ok) {
		d->model->removeBanEntry(result.body.object()["deleted"].toInt());
	}
}

void BanListPage::refreshPage()
{
	d->server->makeApiRequest(REQ_ID, JsonApiMethod::Get, QStringList() << "banlist", QJsonObject());
}

void BanListPage::addNewBan()
{
	QDialog *dlg = new QDialog(this);
	Ui_ipBanDialog ui;
	ui.setupUi(dlg);

	ui.expiration->setMinimumDateTime(QDateTime::currentDateTime());

	connect(dlg, &QDialog::accepted, this, [this, ui]() {
		QJsonObject body;
		body["ip"] = ui.address->text();
		body["subnet"] = ui.subnetmask->text().toInt();
		body["expires"] = ui.expiration->dateTime().toString("yyyy-MM-dd HH:mm:ss");
		body["comment"] = ui.comment->toPlainText();
		d->server->makeApiRequest(ADD_REQ_ID, JsonApiMethod::Create, QStringList() << "banlist", body);
	});
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void BanListPage::removeSelectedBan()
{
	const QModelIndexList sel = d->view->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return;

	const QString id = sel.at(0).data(Qt::UserRole).toString();

	d->server->makeApiRequest(DEL_REQ_ID, JsonApiMethod::Delete, QStringList() << "banlist" << id, QJsonObject());
}

}
}

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

#include "accountlistpage.h"
#include "accountlistmodel.h"
#include "subheaderwidget.h"
#include "server.h"
#include "qtshims.h"

#include "ui_accountdialog.h"

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

static const QString REQ_ID = "accountlist";
static const QString ADD_REQ_ID = "accountlistAdd";
static const QString UPDATE_REQ_ID = "accountlistUpdate";
static const QString DEL_REQ_ID = "accountlistDel";

struct AccountListPage::Private {
	Server *server;
	AccountListModel *model;
	QTableView *view;
};

AccountListPage::AccountListPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;
	d->model = new AccountListModel(this);

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("User Accounts"), 1));

	d->view = new QTableView;
	d->view->setModel(d->model);
	d->view->horizontalHeader()->setStretchLastSection(true);
	d->view->setSelectionMode(QTableView::SingleSelection);
	d->view->setSelectionBehavior(QTableView::SelectRows);
	connect(d->view, &QTableView::doubleClicked, this, &AccountListPage::editSelectedAccount);

	layout->addWidget(d->view);

	{
		auto *buttons = new QHBoxLayout;

		auto *addButton = new QPushButton(tr("Add"), this);
		connect(addButton, &QPushButton::clicked, this, &AccountListPage::addNewAccount);
		buttons->addWidget(addButton);

		auto *editButton = new QPushButton(tr("Edit"), this);
		connect(editButton, &QPushButton::clicked, this, &AccountListPage::editSelectedAccount);
		buttons->addWidget(editButton);

		auto *rmButton = new QPushButton(tr("Remove"), this);
		connect(rmButton, &QPushButton::clicked, this, &AccountListPage::removeSelectedAccount);
		buttons->addWidget(rmButton);

		buttons->addStretch(1);
		layout->addLayout(buttons);
	}

	connect(server, &Server::apiResponse, this, &AccountListPage::handleResponse);
	refreshPage();
}

AccountListPage::~AccountListPage()
{
	delete d;
}

void AccountListPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId == REQ_ID) {
		if(result.status == JsonApiResult::Ok)
			d->model->setList(result.body.array());

	} else if(requestId == ADD_REQ_ID) {
		if(result.status == JsonApiResult::BadRequest)
			QMessageBox::warning(this, tr("Error"), result.body.object()["message"].toString());
		else if(result.status == JsonApiResult::Ok)
			d->model->addAccount(result.body.object());

	} else if(requestId == UPDATE_REQ_ID) {
		if(result.status == JsonApiResult::BadRequest)
			QMessageBox::warning(this, tr("Error"), result.body.object()["message"].toString());
		else if(result.status == JsonApiResult::Ok)
			d->model->updateAccount(result.body.object());

	} else if(requestId == DEL_REQ_ID && result.status == JsonApiResult::Ok) {
		d->model->removeAccount(result.body.object()["deleted"].toInt());
	}
}

void AccountListPage::refreshPage()
{
	d->server->makeApiRequest(REQ_ID, JsonApiMethod::Get, QStringList() << "accounts", QJsonObject());
}

void AccountListPage::addNewAccount()
{
	QDialog *dlg = new QDialog(this);
	Ui_AccountDialog ui;
	ui.setupUi(dlg);

	connect(dlg, &QDialog::accepted, this, [this, ui]() {
		QJsonObject body;
		body["username"] = ui.username->text();
		body["password"] = ui.password->text();
		body["locked"] = ui.locked->isChecked();
		QStringList flags;
		if(ui.flagHost->isChecked())
			flags << "HOST";
		if(ui.flagMod->isChecked())
			flags << "MOD";
		body["flags"] = flags.join(',');
		d->server->makeApiRequest(ADD_REQ_ID, JsonApiMethod::Create, QStringList() << "accounts", body);
	});
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void AccountListPage::editSelectedAccount()
{
	const QModelIndexList sel = d->view->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return;

	const QString id = sel.at(0).data(Qt::UserRole).toString();

	QDialog *dlg = new QDialog(this);
	Ui_AccountDialog ui;
	ui.setupUi(dlg);

	const QJsonObject account = static_cast<const AccountListModel*>(d->view->model())->objectAt(sel.at(0).row());

	ui.username->setText(account["username"].toString());
	ui.locked->setChecked(account["locked"].toBool());
	const QStringList flags = account["flags"].toString().split(',', shim::SKIP_EMPTY_PARTS);
	ui.flagHost->setChecked(flags.contains("HOST"));
	ui.flagMod->setChecked(flags.contains("MOD"));

	connect(dlg, &QDialog::accepted, this, [this, ui, id]() {
		QJsonObject body;
		body["username"] = ui.username->text();
		if(!ui.password->text().isEmpty())
			body["password"] = ui.password->text();
		body["locked"] = ui.locked->isChecked();
		QStringList flags;
		if(ui.flagHost->isChecked())
			flags << "HOST";
		if(ui.flagMod->isChecked())
			flags << "MOD";
		body["flags"] = flags.join(',');
		d->server->makeApiRequest(UPDATE_REQ_ID, JsonApiMethod::Update, QStringList() << "accounts" << id, body);
	});
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void AccountListPage::removeSelectedAccount()
{
	const QModelIndexList sel = d->view->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return;

	const QString id = sel.at(0).data(Qt::UserRole).toString();

	d->server->makeApiRequest(DEL_REQ_ID, JsonApiMethod::Delete, QStringList() << "accounts" << id, QJsonObject());
}

}
}

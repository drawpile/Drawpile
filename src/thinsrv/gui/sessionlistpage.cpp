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

#include "thinsrv/gui/sessionlistpage.h"
#include "thinsrv/gui/subheaderwidget.h"
#include "thinsrv/gui/sessionlistmodel.h"
#include "thinsrv/gui/server.h"

#include <QDebug>
#include <QBoxLayout>
#include <QGridLayout>
#include <QJsonObject>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QTableView>
#include <QHeaderView>
#include <QInputDialog>
#include <QTimer>

namespace server {
namespace gui {

struct SessionListPage::Private {
	Server *server;
};

SessionListPage::SessionListPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("Sessions"), 1));

	QTableView *view = new QTableView;
	view->setModel(server->sessionList());
	view->horizontalHeader()->setStretchLastSection(true);
	view->setSelectionMode(QTableView::SingleSelection);
	view->setSelectionBehavior(QTableView::SelectRows);

	layout->addWidget(view);

	auto *btnLayout = new QHBoxLayout;
	layout->addLayout(btnLayout);

	auto *msgAllBtn = new QPushButton(tr("Message all"));
	btnLayout->addWidget(msgAllBtn);
	btnLayout->addStretch(1);

	connect(msgAllBtn, &QPushButton::clicked, this, &SessionListPage::sendMessageToAll);
}

SessionListPage::~SessionListPage()
{
	delete d;
}

void SessionListPage::sendMessageToAll()
{
	QInputDialog *dlg = new QInputDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setLabelText(tr("Send message"));
	connect(dlg, &QInputDialog::accepted, this, [dlg, this]() {
			QJsonObject o;
			o["message"] = dlg->textValue();
			d->server->makeApiRequest(QString(), JsonApiMethod::Update, QStringList() << "sessions", o);
	});
	dlg->show();
}

}
}


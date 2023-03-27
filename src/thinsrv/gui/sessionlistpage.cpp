// SPDX-License-Identifier: GPL-3.0-or-later

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
			o["alert"] = dlg->textValue();
			d->server->makeApiRequest(QString(), JsonApiMethod::Update, QStringList() << "sessions", o);
	});
	dlg->show();
}

}
}


// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/sessionpage.h"
#include "thinsrv/gui/subheaderwidget.h"
#include "thinsrv/gui/userlistmodel.h"
#include "thinsrv/gui/server.h"
#include "libserver/jsonapi.h"

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
#include <QTimer>

namespace server {
namespace gui {

struct SessionPage::Private {
	Server *server;
	QString id;
	QString refreshReqId;

	QLabel *alias, *founder, *password, *size, *protocol, *title, *started;
	QSpinBox *maxUsers;
	QCheckBox *closed, *nsfm, *persistent;

	UserListModel *userlist;
	QTableView *userview;

	JsonListModel *announcementlist;
	QTableView *announcementview;

	QTimer *saveTimer, *refreshTimer;
	QJsonObject lastUpdate;

	Private() :
		alias(new QLabel),
		founder(new QLabel),
		password(new QLabel),
		size(new QLabel),
		protocol(new QLabel),
		title(new QLabel),
		started(new QLabel),
		maxUsers(new QSpinBox),
		closed(new QCheckBox),
		nsfm(new QCheckBox),
		persistent(new QCheckBox)
	{
		maxUsers->setMinimum(1);
		maxUsers->setMaximum(254);
		closed->setText(SessionPage::tr("Closed"));
		nsfm->setText(SessionPage::tr("NSFM"));
		persistent->setText(SessionPage::tr("Persistent"));
	}
};

namespace sessionpage {

static void addWidgets(struct SessionPage::Private *d, QGridLayout *layout, int row, const QString labelText, QWidget *value, bool addSpacer=false)
{
	if(!labelText.isEmpty())
		layout->addWidget(new QLabel(labelText + ":"), row, 0, 1, 1, Qt::AlignRight);

	if(addSpacer) {
		auto *l2 = new QHBoxLayout;
		l2->addWidget(value);
		l2->addStretch(1);
		layout->addLayout(l2, row, 1);
	} else {
		layout->addWidget(value, row, 1);
	}

	if(value->inherits("QAbstractSpinBox"))
		value->connect(value, SIGNAL(valueChanged(QString)), d->saveTimer, SLOT(start()));
	else if(value->inherits("QAbstractButton"))
		value->connect(value, SIGNAL(clicked(bool)), d->saveTimer, SLOT(start()));
}

static void addLabels(QGridLayout *layout, int row, const QString labelText, QLabel *value)
{
	value->setTextFormat(Qt::PlainText);
	value->setTextInteractionFlags(Qt::TextBrowserInteraction);
	addWidgets(nullptr, layout, row, labelText, value);
}

} // namespace sessionpage

SessionPage::SessionPage(Server *server, const QString &id, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	using namespace sessionpage;

	d->server = server;
	d->id = id;
	d->refreshReqId = "refresh-" + id;
	d->userlist = new UserListModel(this);

	d->announcementlist = new JsonListModel({
			{"id", tr("ID")},
			{"url", tr("URL")},
		}, this
	);

	d->saveTimer = new QTimer(this);
	d->saveTimer->setSingleShot(true);
	d->saveTimer->setInterval(500);
	connect(d->saveTimer, &QTimer::timeout, this, &SessionPage::saveSettings);

	d->refreshTimer = new QTimer(this);
	d->refreshTimer->setSingleShot(false);
	d->refreshTimer->setInterval(15 * 1000);
	connect(d->refreshTimer, &QTimer::timeout, this, &SessionPage::refreshPage);
	d->refreshTimer->start(15 * 1000);

	auto *layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(new SubheaderWidget(tr("Session info"), 1));

	{
		int row=0;
		auto *grid = new QGridLayout;
		grid->setColumnStretch(0, 1);
		grid->setColumnStretch(1, 10);
		layout->addLayout(grid);
		addLabels(grid, row++, tr("ID"), new QLabel(d->id));
		addLabels(grid, row++, tr("Alias"), d->alias);
		addLabels(grid, row++, tr("Title"), d->title);
		addLabels(grid, row++, tr("Started"), d->started);
		addLabels(grid, row++, tr("Protocol version"), d->protocol);
		addLabels(grid, row++, tr("Started by"), d->founder);
		addLabels(grid, row++, tr("Password protected"), d->password);
		addLabels(grid, row++, tr("Size"), d->size);
		addWidgets(d, grid, row++, tr("Maximum users"), d->maxUsers, true);
		addWidgets(d, grid, row++, QString(), d->closed);
		addWidgets(d, grid, row++, QString(), d->persistent);
		addWidgets(d, grid, row++, QString(), d->nsfm);
	}

	{
		auto *buttons = new QHBoxLayout;

		auto *stopBtn = new QPushButton(tr("Terminate"));
		connect(stopBtn, &QPushButton::clicked, this, &SessionPage::terminateSession);
		buttons->addWidget(stopBtn);

		auto *passwdBtn = new QPushButton(tr("Change password"));
		connect(passwdBtn, &QPushButton::clicked, this, &SessionPage::changePassword);
		buttons->addWidget(passwdBtn);

		auto *titleBtn = new QPushButton(tr("Retitle"));
		connect(titleBtn, &QPushButton::clicked, this, &SessionPage::changeTitle);
		buttons->addWidget(titleBtn);

		auto *messageBtn = new QPushButton(tr("Send message"));
		connect(messageBtn, &QPushButton::clicked, this, &SessionPage::sendMessage);
		buttons->addWidget(messageBtn);

		buttons->addStretch(1);

		layout->addLayout(buttons);
	}

	{
		layout->addWidget(new SubheaderWidget(tr("Users"), 2));

		d->userview = new QTableView;
		d->userview->setModel(d->userlist);
		d->userview->setColumnHidden(0, true);
		d->userview->horizontalHeader()->setStretchLastSection(true);
		d->userview->setSelectionMode(QTableView::SingleSelection);
		d->userview->setSelectionBehavior(QTableView::SelectRows);

		layout->addWidget(d->userview);
	}

	{
		auto *buttons = new QHBoxLayout;

		auto *kickBtn = new QPushButton(tr("Kick"));
		connect(kickBtn, &QPushButton::clicked, this, &SessionPage::kickUser);
		buttons->addWidget(kickBtn);

		auto *messageBtn = new QPushButton(tr("Send message"));
		connect(messageBtn, &QPushButton::clicked, this, &SessionPage::sendUserMessage);
		buttons->addWidget(messageBtn);

		buttons->addStretch(1);
		layout->addLayout(buttons);
	}

	{
		layout->addWidget(new SubheaderWidget(tr("Listings"), 2));

		d->announcementview = new QTableView;
		d->announcementview->setModel(d->announcementlist);
		d->announcementview->setSelectionMode(QTableView::SingleSelection);
		d->announcementview->setSelectionBehavior(QTableView::SelectRows);

		layout->addWidget(d->announcementview);
	}

	{
		auto *buttons = new QHBoxLayout;

		auto *removeBtn = new QPushButton(tr("Remove"));
		connect(removeBtn, &QPushButton::clicked, this, &SessionPage::removeAnnouncement);
		buttons->addWidget(removeBtn);

		buttons->addStretch(1);
		layout->addLayout(buttons);
	}

	layout->addStretch(1);

	connect(server, &Server::apiResponse, this, &SessionPage::handleResponse);
	refreshPage();
}

SessionPage::~SessionPage()
{
	delete d;
}

void SessionPage::terminateSession()
{
	QMessageBox *msg = new QMessageBox(
		QMessageBox::Question,
		d->title->text(),
		tr("Terminate session?"),
		QMessageBox::Yes|QMessageBox::No,
		this
		);
	msg->setAttribute(Qt::WA_DeleteOnClose, true);

	connect(msg, &QMessageBox::buttonClicked, [msg, this](QAbstractButton *btn) {
		if(msg->standardButton(btn) == QMessageBox::Yes)
			d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Delete, QStringList() << "sessions" << d->id, QJsonObject());
	});

	msg->show();
}

void SessionPage::changePassword()
{
	QInputDialog *dlg = new QInputDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setTextEchoMode(QLineEdit::PasswordEchoOnEdit);
	dlg->setLabelText(tr("Change password"));
	connect(dlg, &QInputDialog::accepted, [dlg, this]() {
		QJsonObject o;
		o["password"] = dlg->textValue();
		d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Update, QStringList() << "sessions" << d->id, o);
		d->refreshTimer->start(); // reset refresh timer
	});
	dlg->show();

}

void SessionPage::changeTitle()
{
	QInputDialog *dlg = new QInputDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setLabelText(tr("Change title"));
	dlg->setTextValue(d->title->text());
	connect(dlg, &QInputDialog::accepted, [dlg, this]() {
		QJsonObject o;
		o["title"] = dlg->textValue();
		d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Update, QStringList() << "sessions" << d->id, o);
		d->refreshTimer->start(); // reset refresh timer
	});
	dlg->show();
}

void SessionPage::sendMessage()
{
	QInputDialog *dlg = new QInputDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setLabelText(tr("Send message"));
	connect(dlg, &QInputDialog::accepted, [dlg, this]() {
		QJsonObject o;
		o["alert"] = dlg->textValue();
		d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Update, QStringList() << "sessions" << d->id, o);
		d->refreshTimer->start(); // reset refresh timer
	});
	dlg->show();
}

void SessionPage::kickUser()
{
	const int id = selectedUser();
	if(id>0) {
		d->server->makeApiRequest("kickuser", JsonApiMethod::Delete, QStringList() << "sessions" << d->id << QString::number(id), QJsonObject());
		refreshPage();
	}
}

void SessionPage::removeAnnouncement()
{
	const int id = selectedAnnouncement();
	if(id>0) {
		d->server->makeApiRequest("removeannouncement", JsonApiMethod::Delete, QStringList() << "sessions" << d->id << "listing" << QString::number(id), QJsonObject());
		refreshPage();
	}
}

void SessionPage::sendUserMessage()
{
	const int id = selectedUser();
	if(id<1)
		return;

	QInputDialog *dlg = new QInputDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setLabelText(tr("Send message"));
	connect(dlg, &QInputDialog::accepted, [dlg, id, this]() {
		QJsonObject o;
		o["alert"] = dlg->textValue();
		d->server->makeApiRequest("msguser", JsonApiMethod::Update, QStringList() << "sessions" << d->id << QString::number(id), o);
	});
	dlg->show();

}

int SessionPage::selectedUser() const
{
	const QModelIndexList sel = d->userview->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return 0;

	return sel.at(0).data(Qt::UserRole).toInt();
}

int SessionPage::selectedAnnouncement() const
{
	const QModelIndexList sel = d->announcementview->selectionModel()->selectedIndexes();
	if(sel.isEmpty())
		return 0;

	return sel.at(0).data(Qt::UserRole).toInt();
}

void SessionPage::refreshPage()
{
	qDebug() << "Refreshing session" << d->id << "details";
	d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Get, QStringList() << "sessions" << d->id, QJsonObject());
}

void SessionPage::saveSettings()
{
	QJsonObject o {
		{"maxUserCount", d->maxUsers->value()},
		{"closed", d->closed->isChecked()},
		{"persistent", d->persistent->isChecked()},
		{"nsfm", d->nsfm->isChecked()}
	};

	QJsonObject update;
	for(auto i=o.constBegin();i!=o.constEnd();++i) {
		if(d->lastUpdate[i.key()] != i.value())
			update[i.key()] = i.value();
	}

	if(!update.isEmpty()) {
		d->lastUpdate = o;

		qDebug() << "update" << update;
		d->server->makeApiRequest(d->refreshReqId, JsonApiMethod::Update, QStringList() << "sessions" << d->id, update);
	}

	d->refreshTimer->start(); // reset refresh timer
}

void SessionPage::handleResponse(const QString &requestId, const JsonApiResult &result)
{
	if(requestId!=d->refreshReqId)
		return;

	QJsonObject o = result.body.object();

	d->alias->setText(o["alias"].toString());
	d->founder->setText(o["founder"].toString());
	d->password->setText(o["hasPassword"].toBool() ? tr("Yes") : tr("No"));

	double size = o["size"].toDouble();
	double maxSize = o["maxSize"].toDouble();
	if(maxSize>0) {
		d->size->setText(QString("%1% of %2 MB")
			.arg(size/maxSize * 100.0, 0, 'f', 1)
			.arg(maxSize / (1024*1024), 0, 'f', 2));
	} else {
		d->size->setText(QString("%1% MB").arg(maxSize / (1024*1024), 0, 'f', 2));
	}

	d->protocol->setText(o["protocol"].toString());
	d->title->setText(o["title"].toString());
	d->started->setText(o["started"].toString());

	d->maxUsers->setValue(o["maxUserCount"].toInt());
	d->closed->setChecked(o["closed"].toBool());
	if(o.contains("persistent")) {
		d->persistent->setEnabled(true);
		d->persistent->setChecked(o["persistent"].toBool());
	} else {
		d->persistent->setEnabled(false);
		d->persistent->setChecked(false);
	}

	d->nsfm->setChecked(o["nsfm"].toBool());

	d->userlist->setList(o["users"].toArray());
	d->announcementlist->setList(o["listings"].toArray());
}

}
}

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

#include "serversummarypage.h"
#include "localserver.h"

#include <QGridLayout>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace server {
namespace gui {

struct ServerSummaryPage::Private {
	Server *server;
	QLabel
		*status,
		*address,
		*port,
		*remoteAdmin,
		*recording;

	QPushButton *startStopButton;

	Private()
		:
		  status(new QLabel),
		  address(new QLabel),
		  port(new QLabel),
		  remoteAdmin(new QLabel),
		  recording(new QLabel)
	{}
};

static void addLabels(QGridLayout *layout, int row, QLabel *label, QLabel *value)
{
	label->setText(label->text() + ":");
	value->setTextFormat(Qt::PlainText);
	value->setTextInteractionFlags(Qt::TextBrowserInteraction);
	layout->addWidget(label, row, 0, 1, 1, Qt::AlignRight);
	layout->addWidget(value, row, 1);
}

ServerSummaryPage::ServerSummaryPage(Server *server, QWidget *parent)
	: QWidget(parent), d(new Private)
{
	d->server = server;

	auto *layout = new QGridLayout;
	layout->setColumnStretch(0, 1);
	layout->setColumnStretch(1, 10);
	setLayout(layout);

	int row=0;

	layout->addWidget(new QLabel("<h1>" + tr("Server") + "</h1>"), row++, 0, 1,	2);

	if(server->isLocal())
		addLabels(layout, row++, new QLabel(tr("Status")), d->status);

	addLabels(layout, row++, new QLabel(tr("Address")), d->address);
	addLabels(layout, row++, new QLabel(tr("Port")), d->port);
	addLabels(layout, row++, new QLabel(tr("Remote admin port")), d->remoteAdmin);
	addLabels(layout, row++, new QLabel(tr("Recording")), d->recording);

	if(server->isLocal()) {
		layout->addItem(new QSpacerItem(1,10), row++, 0);
		auto *buttons = new QHBoxLayout;

		d->startStopButton = new QPushButton;
		connect(d->startStopButton, &QPushButton::clicked, this, &ServerSummaryPage::startOrStopServer);
		buttons->addWidget(d->startStopButton);

		auto *settingsButton = new QPushButton(tr("Settings"));
		buttons->addWidget(settingsButton);

		buttons->addStretch(1);

		layout->addLayout(buttons, row++, 0, 1, 2);

	}

	layout->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);

	if(d->server->isLocal()) {
		connect(static_cast<LocalServer*>(d->server), &LocalServer::serverStateChanged, this, &ServerSummaryPage::refreshPage);
	}
	refreshPage();
}

ServerSummaryPage::~ServerSummaryPage()
{
	delete d;
}

void ServerSummaryPage::startOrStopServer()
{
	Q_ASSERT(d->server->isLocal());
	auto server = static_cast<LocalServer*>(d->server);

	if(server->isRunning()) {
		server->stopServer();
	} else {
		server->startServer();
	}
	d->startStopButton->setEnabled(false);
}

void ServerSummaryPage::refreshPage()
{
	d->address->setText(d->server->address());
	d->port->setText(QString::number(d->server->port()));
	if(d->server->isLocal()) {
		const bool isRunning = static_cast<LocalServer*>(d->server)->isRunning();
		d->status->setText(isRunning ? tr("Started") : tr("Stopped"));
		d->remoteAdmin->setText("-");
		d->recording->setText("???");

		d->startStopButton->setText(isRunning ? tr("Stop") : tr("Start"));
		d->startStopButton->setEnabled(true);

	} else {
		d->remoteAdmin->setText("port goes here");
		d->recording->setText("unknown");
	}
}

}
}

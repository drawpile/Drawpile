/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

******************************************************************************/

#include "statusdialog.h"

#include <QtGui>
#include <QHostAddress>

#include "../server/statistics.h"
#include "../server/server.h"
#include "../shared/qt.h"
#include "../shared/templates.h"

StatusDialog::StatusDialog(const Server *_srv, QWidget *parent)
	: QDialog(parent),
	srv(_srv)
{
	setWindowTitle(QString(tr("%1 Status").arg(QCoreApplication::applicationName())));
	
	setAttribute(Qt::WA_DeleteOnClose);
	
	QGroupBox *status_group = new QGroupBox(tr("Status"));
	
	QVBoxLayout *status_superbox = new QVBoxLayout;
	status_superbox->setContentsMargins(5,3,5,3);
	
	// server state
	QHBoxLayout *host_box = new QHBoxLayout;
	host_box->addWidget(new QLabel(tr("Hostname")), 1);
	hostname = new QLineEdit;
	hostname->setReadOnly(true);
	hostname->setAlignment(Qt::AlignCenter);
	host_box->addWidget(hostname);
	status_superbox->addLayout(host_box);
	
	serverStopped(); // updates hostname box
	
	status_group->setLayout(status_superbox);
	
	// Statistics
	QGroupBox *statistics_group = new QGroupBox(tr("Statistics"));
	QVBoxLayout *statistics_superbox = new QVBoxLayout;
	statistics_superbox->setContentsMargins(5,3,5,3);
	
	Statistics stats = srv->getStats(); // mutex lock?
	
	QHBoxLayout *sentdata_box = new QHBoxLayout;
	sentdata_box->addWidget(new QLabel(tr("Sent data")), 1);
	sentdata_box->addWidget(new QLabel(QString("%1").arg(stats.dataSent)));
	statistics_superbox->addLayout(sentdata_box);
	
	QHBoxLayout *recvdata_box = new QHBoxLayout;
	recvdata_box->addWidget(new QLabel(tr("Received data")), 1);
	recvdata_box->addWidget(new QLabel(QString("%1").arg(stats.dataRecv)));
	statistics_superbox->addLayout(recvdata_box);
	
	statistics_group->setLayout(statistics_superbox);
	
	// Sessions
	session_group = new QGroupBox(tr("Sessions"));
	
	QVBoxLayout *session_box = new QVBoxLayout;
	session_box->setContentsMargins(5,3,5,3);
	
	// * session list
	
	session_group->setLayout(session_box);
	
	// Users
	QGroupBox *user_group = new QGroupBox(tr("Users"));
	
	QVBoxLayout *user_box = new QVBoxLayout;
	user_box->setContentsMargins(5,3,5,3);
	
	// * user list
	
	user_group->setLayout(user_box);
	
	// root layout
	
	QVBoxLayout *root = new QVBoxLayout;
	root->setContentsMargins(5,3,5,3);
	
	root->addWidget(status_group);
	root->addWidget(session_group);
	root->addWidget(user_group);
	root->addWidget(statistics_group);
	
	setLayout(root);
	
	setMinimumWidth(120);
}

void StatusDialog::serverStarted()
{
	//srvmutex->lock();
	QString host = QString("%1:%2")
		.arg(Network::getExternalAddress().toString())
		.arg(srv->getPort());
	//srvmutex->unlock();
	hostname->setText(host);
}

void StatusDialog::serverStopped()
{
	hostname->setText(tr("n/a"));
}

void StatusDialog::update()
{
	mutex.lock();
	//srv->getStatus();
	mutex.unlock();
}

void StatusDialog::updateUsers()
{
	mutex.lock();
	mutex.unlock();
}

void StatusDialog::updateSessions()
{
	mutex.lock();
	mutex.unlock();
}

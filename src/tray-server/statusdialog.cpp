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

#include "../server/server.h"
#include "network.h"
#include "../shared/templates.h"

StatusDialog::StatusDialog(const Server *_srv, QWidget *parent)
	: QDialog(parent),
	srv(_srv)
{
	setWindowTitle(tr("DrawPile Server Status"));
	
	//setAttribute(Qt::WA_DeleteOnClose);
	
	QGroupBox *status_group = new QGroupBox(tr("Status"));
	
	QVBoxLayout *status_superbox = new QVBoxLayout;
	
	status_group->setContentsMargins(3,12,3,0);
	
	// server state
	QHBoxLayout *state_box = new QHBoxLayout;
	state_box->addWidget(new QLabel(tr("State:")), 1);
	state_text = new QLabel;
	state_text->setAlignment(Qt::AlignCenter);
	serverStopped();
	state_box->addWidget(state_text, 0);
	status_superbox->addLayout(state_box);
	
	QHBoxLayout *host_box = new QHBoxLayout;
	host_box->addWidget(new QLabel(tr("Hostname")), 1);
	hostname = new QLineEdit;
	hostname->setReadOnly(true);
	hostname->setAlignment(Qt::AlignCenter);
	host_box->addWidget(hostname);
	status_superbox->addLayout(host_box);
	
	
	status_group->setLayout(status_superbox);
	
	// Sessions
	session_group = new QGroupBox(tr("Sessions"));
	session_group->setContentsMargins(3,12,3,0);
	
	QVBoxLayout *session_box = new QVBoxLayout;
	
	// * session list
	
	session_group->setLayout(session_box);
	
	// Users
	QGroupBox *user_group = new QGroupBox(tr("Users"));
	user_group->setContentsMargins(3,12,3,0);
	
	QVBoxLayout *user_box = new QVBoxLayout;
	
	// * user list
	
	user_group->setLayout(user_box);
	
	// root layout
	
	QVBoxLayout *root = new QVBoxLayout;
	root->setContentsMargins(3,3,3,3);
	root->addStretch(1);
	root->addSpacing(3);
	root->addStrut(120); // ?
	
	root->addWidget(status_group);
	root->addWidget(session_group);
	root->addWidget(user_group);
	
	setLayout(root);
}

void StatusDialog::serverStarted()
{
	state_text->setText(tr("Ready"));
	//srvmutex->lock();
	QString host = QString("%1:%2")
		.arg(Network::Qt::getExternalAddress().toString())
		.arg(srv->getPort());
	//srvmutex->unlock();
	hostname->setText(host);
}

void StatusDialog::serverStopped()
{
	state_text->setText(tr("Stopped"));
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

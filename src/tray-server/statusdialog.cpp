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

StatusDialog::StatusDialog()
	: unknown_value(tr("Unknown"))
{
	setWindowTitle(tr("DrawPile Server Status"));
	
	//setAttribute(Qt::WA_DeleteOnClose);
	
	QGroupBox *status_group = new QGroupBox(tr("Status"));
	status_group->setContentsMargins(3,12,3,0);
	
	// server state
	QHBoxLayout *state_box = new QHBoxLayout;
	state_box->addWidget(new QLabel(tr("State:")), 1);
	state_text = new QLabel;
	state_text->setText(unknown_value);
	state_text->setAlignment(Qt::AlignCenter);
	state_box->addWidget(state_text, 0);
	
	status_group->setLayout(state_box);
	
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

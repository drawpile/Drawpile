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

#include "configdialog.h"
#include "../shared/protocol.defaults.h"
#include "../server/sockets.h"

#include <QDebug>
#include <QtGui>

ConfigDialog::ConfigDialog()
{
	#define REQUIRES_RESTART "<br>This option can't be changed while the server is running."
	setWindowTitle(tr("DrawPile Server Configuration"));
	
	//setAttribute(Qt::WA_DeleteOnClose);
	
	QGroupBox *limit_group = new QGroupBox("Limits");
	QVBoxLayout *limit_superbox = new QVBoxLayout;
	
	// user limit
	QHBoxLayout *ulimit_box = new QHBoxLayout;
	ulimit_box->addWidget(new QLabel(tr("User limit")), 1);
	
	QSpinBox *ulimit_spinner = new QSpinBox;
	ulimit_spinner->setRange(1, 255);
	ulimit_spinner->setValue(10);
	ulimit_spinner->setToolTip(tr("Maximum number of users allowed on server."));
	
	ulimit_box->addWidget(ulimit_spinner, 0);
	limit_superbox->addLayout(ulimit_box);
	
	// session limit
	QHBoxLayout *slimit_box = new QHBoxLayout;
	slimit_box->addWidget(new QLabel(tr("Session limit")), 1);
	
	QSpinBox *slimit_spinner = new QSpinBox;
	slimit_spinner->setRange(1, 255);
	slimit_spinner->setValue(1);
	slimit_spinner->setToolTip(tr("Maximum number of sessions allowed on server."));
	
	slimit_box->addWidget(slimit_spinner, 0);
	limit_superbox->addLayout(slimit_box);
	
	// minimum canvas dimensions
	QHBoxLayout *mindim_box = new QHBoxLayout;
	mindim_box->addWidget(new QLabel(tr("Min. allowed canvas size")), 1);
	
	QSpinBox *mindim_spinner = new QSpinBox;
	mindim_spinner->setRange(400, protocol::max_dimension);
	mindim_spinner->setValue(400);
	mindim_spinner->setToolTip(tr("Minimum size of canvas (for both width and height) in any session."));
	
	mindim_box->addWidget(mindim_spinner, 0);
	limit_superbox->addLayout(mindim_box);
	
	// name length limit
	QHBoxLayout *namelen_box = new QHBoxLayout;
	namelen_box->addWidget(new QLabel(tr("Maximum name length")), 1);
	
	namelen_spinner = new QSpinBox;
	namelen_spinner->setRange(8, 255);
	namelen_spinner->setValue(16);
	
	/** @todo needs better description! */
	namelen_spinner->setToolTip(tr("For UTF-16 the limit is halved because each character takes two bytes by default."));
	
	namelen_box->addWidget(namelen_spinner, 0);
	limit_superbox->addLayout(namelen_box);
	
	// subscription limit
	QHBoxLayout *sublimit_box = new QHBoxLayout;
	sublimit_box->addWidget(new QLabel(tr("Subscription limit")), 1);
	
	QSpinBox *sublimit_spinner = new QSpinBox;
	sublimit_spinner->setRange(1, 255);
	sublimit_spinner->setValue(5);
	sublimit_spinner->setToolTip(tr("How many sessions single user can be subscribed to.<br>"
		"Has little or no impact on server load if multiple connections are allowed from same address."));
	
	sublimit_box->addWidget(sublimit_spinner, 0);
	limit_superbox->addLayout(sublimit_box);
	
	limit_group->setContentsMargins(3,12,3,0);
	limit_group->setLayout(limit_superbox);
	
	// initial user permissions
	QGroupBox *umode_group = new QGroupBox(tr("Initial user permissions"));
	umode_group->setContentsMargins(3,12,3,0);
	
	QHBoxLayout *usermode_box = new QHBoxLayout;
	usermode_box->addSpacing(3);
	
	QCheckBox *can_draw = new QCheckBox;
	can_draw->setChecked(true);
	can_draw->setToolTip(tr("If unchecked, session owner or server admin must unlock the user before they can draw."));
	usermode_box->addWidget(can_draw, 0);
	
	usermode_box->addWidget(new QLabel(tr("Allow drawing")), 1);
	
	QCheckBox *can_chat = new QCheckBox;
	can_chat->setChecked(true);
	can_chat->setToolTip(tr("If unchecked, session owner or server admin must unmute the user before thay can chat."))
	usermode_box->addWidget(can_chat, 0);
	usermode_box->addWidget(new QLabel(tr("Allow chat")), 1);
	
	umode_group->setLayout(usermode_box);
	
	// listening port
	QHBoxLayout *port_box = new QHBoxLayout;
	port_box->addWidget(new QLabel(tr("Listening port")), 1);
	
	port_spinner = new QSpinBox;
	port_spinner->setRange(Network::SuperUser_Port+1, Network::PortUpperBound);
	port_spinner->setValue(protocol::default_port);
	port_spinner->setToolTip(tr("TCP port on which the server will try to listen when started."
		REQUIRES_RESTART));
	
	port_box->addWidget(port_spinner, 0);
	
	QGroupBox *req_group = new QGroupBox(tr("Restrictions"));
	QVBoxLayout *req_superbox = new QVBoxLayout;
	
	// require unique names
	QHBoxLayout *unique_box = new QHBoxLayout;
	unique_box->addWidget(new QLabel(tr("Unique names")), 1);
	unique_names = new QCheckBox;
	unique_names->setToolTip(tr("Require that all users and sessions have an unique name."
		REQUIRES_RESTART));
	unique_box->addWidget(unique_names, 0);
	req_superbox->addLayout(unique_box);
	
	// allow duplicate connections from same IP
	QHBoxLayout *dupe_box = new QHBoxLayout;
	dupe_box->addWidget(new QLabel(tr("Allow duplicate connections")), 1);
	QCheckBox *allow_duplicate = new QCheckBox;
	dupe_box->addWidget(allow_duplicate, 0);
	req_superbox->addLayout(dupe_box);
	
	// Require the use of UTF-16 strings
	QHBoxLayout *widestr_box = new QHBoxLayout;
	widestr_box->addWidget(new QLabel(tr("UTF-16 strings")), 1);
	wide_strings = new QCheckBox;
	wide_strings->setToolTip(tr("Require clients communicate to with UTF-16 instead of UTF-8.<br>Useful only if the users mostly use non-ASCII (a-z) characters (e.g. Kanji, Cyrillics, etc.)." REQUIRES_RESTART));
	
	connect(wide_strings, SIGNAL(stateChanged(int)), this, SLOT(wideStrChanged(int)));
	
	widestr_box->addWidget(wide_strings, 0);
	req_superbox->addLayout(widestr_box);
	
	req_group->setContentsMargins(3,12,3,0);
	req_group->setLayout(req_superbox);
	
	// server password
	QHBoxLayout *srvpass_box = new QHBoxLayout;
	srvpass_box->addWidget(new QLabel(tr("Server password")), 1);
	QLineEdit *srvpass_edit = new QLineEdit;
	srvpass_edit->setMaxLength(255);
	srvpass_edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	srvpass_edit->setToolTip(tr("Password for connecting to the server."));
	srvpass_box->addWidget(srvpass_edit);
	
	// admin password
	QHBoxLayout *admpass_box = new QHBoxLayout;
	admpass_box->addWidget(new QLabel(tr("Administrator password")), 1);
	QLineEdit *admpass_edit = new QLineEdit;
	admpass_edit->setMaxLength(255);
	admpass_edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	admpass_edit->setToolTip(tr("Password for gaining administrator rights.<br>If not set, users can't become admin by any means."));
	admpass_box->addWidget(admpass_edit);
	
	// command box
	QHBoxLayout *command_box = new QHBoxLayout;
	
	QPushButton *apply_butt = new QPushButton(tr("Apply"));
	apply_butt->setEnabled(false);
	
	QPushButton *save_butt = new QPushButton(tr("Save"));
	save_butt->setEnabled(false);
	
	QPushButton *reset_butt = new QPushButton(tr("Reset"));
	reset_butt->setEnabled(false);
	
	command_box->addWidget(apply_butt);
	command_box->addWidget(save_butt);
	command_box->addWidget(reset_butt);
	
	// root layout
	QVBoxLayout *root = new QVBoxLayout;
	root->setContentsMargins(3,3,3,3);
	root->addStretch(1);
	root->addSpacing(3);
	root->addStrut(120); // ?
	
	root->addWidget(limit_group);
	root->addWidget(umode_group);
	root->addWidget(req_group);
	
	root->addLayout(port_box);
	
	root->addLayout(srvpass_box);
	root->addLayout(admpass_box);
	
	root->addLayout(command_box);
	
	setLayout(root);
	
	#undef REQUIRES_RESTART
}

void ConfigDialog::serverState(bool running)
{
	wide_strings->setDisabled(running);
	unique_names->setDisabled(running);
	port_spinner->setReadOnly(running);
}

void ConfigDialog::wideStrChanged(int state)
{
	if (state == Qt::Unchecked)
		namelen_spinner->setMaximum(255);
	else
		namelen_spinner->setMaximum(127);
}

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
#include "../server/network.h"
#include "../server/server.h"
#include "../shared/templates.h"

#include "strings.h"

#include <QSettings>
#include <QSystemTrayIcon>
#include <QtGui>

ConfigDialog::ConfigDialog(Server *_srv, QWidget *parent)
	: QDialog(parent),
	srv(_srv)
{
	QString RequireRestart(tr("<br>This option can't be changed while the server is running."));
	setWindowTitle(QString(tr("%1 Configuration").arg(QCoreApplication::applicationName())));
	
	//setAttribute(Qt::WA_DeleteOnClose);
	
	QGroupBox *limit_group = new QGroupBox("Limits");
	QVBoxLayout *limit_superbox = new QVBoxLayout;
	
	// user limit
	QHBoxLayout *ulimit_box = new QHBoxLayout;
	ulimit_box->addWidget(new QLabel(tr("User limit")), 1);
	
	ulimit_spinner = new QSpinBox;
	ulimit_spinner->setRange(1, 255);
	ulimit_spinner->setToolTip(tr("Maximum number of users allowed on server."));
	connect(ulimit_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	ulimit_box->addWidget(ulimit_spinner, 0);
	limit_superbox->addLayout(ulimit_box);
	
	// session limit
	QHBoxLayout *slimit_box = new QHBoxLayout;
	slimit_box->addWidget(new QLabel(tr("Session limit")), 1);
	
	slimit_spinner = new QSpinBox;
	slimit_spinner->setRange(1, 255);
	slimit_spinner->setToolTip(tr("Maximum number of sessions allowed on server."));
	connect(slimit_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	slimit_box->addWidget(slimit_spinner, 0);
	limit_superbox->addLayout(slimit_box);
	
	// minimum canvas dimensions
	QHBoxLayout *mindim_box = new QHBoxLayout;
	mindim_box->addWidget(new QLabel(tr("Min. allowed canvas size")), 1);
	
	mindim_spinner = new QSpinBox;
	mindim_spinner->setRange(400, protocol::max_dimension);
	mindim_spinner->setToolTip(tr("Minimum size of canvas (for both width and height) in any session."));
	connect(mindim_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	mindim_box->addWidget(mindim_spinner, 0);
	limit_superbox->addLayout(mindim_box);
	
	// name length limit
	QHBoxLayout *namelen_box = new QHBoxLayout;
	namelen_box->addWidget(new QLabel(tr("Maximum name length")), 1);
	
	namelen_spinner = new QSpinBox;
	namelen_spinner->setRange(8, 255);
	
	/** @todo needs better description! */
	namelen_spinner->setToolTip(tr("For UTF-16 the limit is halved because each character takes two bytes by default."));
	connect(namelen_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	namelen_box->addWidget(namelen_spinner, 0);
	limit_superbox->addLayout(namelen_box);
	
	// subscription limit
	QHBoxLayout *sublimit_box = new QHBoxLayout;
	sublimit_box->addWidget(new QLabel(tr("Subscription limit")), 1);
	
	sublimit_spinner = new QSpinBox;
	sublimit_spinner->setRange(1, 255);
	sublimit_spinner->setToolTip(tr("How many sessions single user can be subscribed to.<br>"
		"Has little or no impact on server load if multiple connections are allowed from same address."));
	connect(sublimit_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	sublimit_box->addWidget(sublimit_spinner, 0);
	limit_superbox->addLayout(sublimit_box);
	
	limit_group->setContentsMargins(3,12,3,0);
	limit_group->setLayout(limit_superbox);
	
	// initial user permissions
	QGroupBox *umode_group = new QGroupBox(tr("Initial user permissions"));
	umode_group->setContentsMargins(3,12,3,0);
	
	QHBoxLayout *usermode_box = new QHBoxLayout;
	usermode_box->addSpacing(3);
	
	can_draw = new QCheckBox;
	can_draw->setToolTip(tr("If unchecked, session owner or server admin must unlock the user before they can draw."));
	can_draw->setChecked(true);
	connect(can_draw, SIGNAL(stateChanged(int)), this, SLOT(enableButtons()));
	
	usermode_box->addWidget(can_draw, 0);
	
	usermode_box->addWidget(new QLabel(tr("Allow drawing")), 1);
	
	can_chat = new QCheckBox;
	can_chat->setToolTip(tr("If unchecked, session owner or server admin must unmute the user before thay can chat."));
	can_chat->setChecked(true);
	connect(can_chat, SIGNAL(stateChanged(int)), this, SLOT(enableButtons()));
	
	usermode_box->addWidget(can_chat, 0);
	usermode_box->addWidget(new QLabel(tr("Allow chat")), 1);
	
	umode_group->setLayout(usermode_box);
	
	// listening port
	QHBoxLayout *port_box = new QHBoxLayout;
	port_box->addWidget(new QLabel(tr("Listening port")), 1);
	
	port_spinner = new QSpinBox;
	port_spinner->setRange(Network::SuperUser_Port+1, Network::PortUpperBound);
	port_spinner->setToolTip(tr("TCP port on which the server will try to listen when started.").append(RequireRestart));
	connect(port_spinner, SIGNAL(editingFinished()), this, SLOT(enableButtons()));
	
	port_box->addWidget(port_spinner, 0);
	
	QGroupBox *req_group = new QGroupBox(tr("Restrictions"));
	QVBoxLayout *req_superbox = new QVBoxLayout;
	
	// require unique names
	QHBoxLayout *unique_box = new QHBoxLayout;
	unique_box->addWidget(new QLabel(tr("Unique names")), 1);
	unique_names = new QCheckBox;
	unique_names->setToolTip(tr("Require that all users and sessions have an unique name.").append(RequireRestart));
	connect(unique_names, SIGNAL(stateChanged(int)), this, SLOT(enableButtons()));
	
	unique_box->addWidget(unique_names, 0);
	req_superbox->addLayout(unique_box);
	
	// allow duplicate connections from same IP
	QHBoxLayout *dupe_box = new QHBoxLayout;
	dupe_box->addWidget(new QLabel(tr("Allow duplicate connections")), 1);
	
	allow_duplicate = new QCheckBox;
	allow_duplicate->setToolTip(tr("Allows multiple connections from same IP address."));
	connect(allow_duplicate, SIGNAL(stateChanged(int)), this, SLOT(enableButtons()));
	dupe_box->addWidget(allow_duplicate, 0);
	req_superbox->addLayout(dupe_box);
	
	// Require the use of UTF-16 strings
	QHBoxLayout *widestr_box = new QHBoxLayout;
	widestr_box->addWidget(new QLabel(tr("UTF-16 strings")), 1);
	wide_strings = new QCheckBox;
	wide_strings->setToolTip(tr("Require clients communicate to with UTF-16 instead of UTF-8.<br>Useful only if the users mostly use non-ASCII (a-z) characters (e.g. Kanji, Cyrillics, etc.).").append(RequireRestart));
	
	connect(wide_strings, SIGNAL(stateChanged(int)), this, SLOT(wideStrChanged(int)));
	connect(wide_strings, SIGNAL(stateChanged(int)), this, SLOT(enableButtons()));
	
	widestr_box->addWidget(wide_strings, 0);
	req_superbox->addLayout(widestr_box);
	
	req_group->setContentsMargins(3,12,3,0);
	req_group->setLayout(req_superbox);
	
	// server password
	QHBoxLayout *srvpass_box = new QHBoxLayout;
	srvpass_box->addWidget(new QLabel(tr("Server password")), 1);
	srvpass_edit = new QLineEdit;
	srvpass_edit->setMaxLength(255);
	srvpass_edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	srvpass_edit->setToolTip(tr("Password for connecting to the server."));
	connect(srvpass_edit, SIGNAL(textChanged(const QString&)), this, SLOT(enableButtons()));
	srvpass_box->addWidget(srvpass_edit);
	
	// admin password
	QHBoxLayout *admpass_box = new QHBoxLayout;
	admpass_box->addWidget(new QLabel(tr("Administrator password")), 1);
	admpass_edit = new QLineEdit;
	admpass_edit->setMaxLength(255);
	admpass_edit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
	admpass_edit->setToolTip(tr("Password for gaining administrator rights.<br>If not set, users can't become admin by any means."));
	connect(admpass_edit, SIGNAL(textChanged(const QString&)), this, SLOT(enableButtons()));
	admpass_box->addWidget(admpass_edit);
	
	// command box
	QHBoxLayout *command_box = new QHBoxLayout;
	
	apply_butt = new QPushButton(tr("Apply"));
	apply_butt->setToolTip(tr("Commit settings to server."));
	connect(apply_butt, SIGNAL(clicked(bool)), this, SLOT(applySettings()));
	
	save_butt = new QPushButton(tr("Save"));
	save_butt->setToolTip(tr("Save settings in O/S specific storage. The settings will be automatically loaded in on any subsequent restarts of the server application."));
	connect(save_butt, SIGNAL(clicked(bool)), this, SLOT(saveSettings()));
	
	reset_butt = new QPushButton(tr("Reset"));
	reset_butt->setToolTip(tr("Restore currently active settigs to config dialog."));
	connect(reset_butt, SIGNAL(clicked(bool)), this, SLOT(resetSettings()));
	
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
	
	// get settings from server
	//resetSettings();
	//enableButtons();
	
	loadSettings();
	applySettings();
	
	apply_butt->setDisabled(true);
	save_butt->setDisabled(true);
	reset_butt->setDisabled(true);
}

void ConfigDialog::serverStopped()
{
	serverRunning(false);
}

void ConfigDialog::serverStarted()
{
	serverRunning(true);
}

void ConfigDialog::serverRunning(bool _running)
{
	wide_strings->setDisabled(_running);
	unique_names->setDisabled(_running);
	port_spinner->setReadOnly(_running);
}

void ConfigDialog::wideStrChanged(int state)
{
	if (state == Qt::Unchecked)
		namelen_spinner->setMaximum(255);
	else
		namelen_spinner->setMaximum(127);
}

void ConfigDialog::enableButtons()
{
	apply_butt->setEnabled(true);
	save_butt->setEnabled(true);
	reset_butt->setEnabled(true);
}

void ConfigDialog::applySettings()
{
	apply_butt->setDisabled(true);
	reset_butt->setDisabled(true);
	
	quint8 user_mode = 0;
	if (!can_draw->isChecked())
		fSet(user_mode, quint8(protocol::user::Locked));
	if (!can_chat->isChecked())
		fSet(user_mode, quint8(protocol::user::Mute));
	
	srv->setUserMode(user_mode);
	
	//srvmutex->lock();
	
	srv->setDuplicateConnectionBlocking( !allow_duplicate->isChecked() );
	bool req_widestrings = wide_strings->isChecked();
	srv->setUTF16Requirement( req_widestrings );
	srv->setUniqueNameEnforcing( unique_names->isChecked() );
	srv->setPort(port_spinner->value());
	uint namelength_limit = namelen_spinner->value();
	if (req_widestrings)
		namelength_limit *= 2;
	srv->setNameLengthLimit(namelength_limit);
	srv->setSubscriptionLimit(sublimit_spinner->value());
	srv->setMinDimension(mindim_spinner->value());
	srv->setSessionLimit(slimit_spinner->value());
	srv->setUserLimit(ulimit_spinner->value());
	
	const QString configError(tr("Configuration error!"));
	
	char *str;
	uint len;
	QString pass = admpass_edit->text();
	if (pass.length() != 0)
	{
		if (req_widestrings)
			str = convert::toUTF16(pass, len);
		else
			str = convert::toUTF8(pass, len);
		
		if (len <= namelength_limit)
			srv->setAdminPassword(str, len);
		else
			emit message(configError, tr("Administrator password length exceeded allowed size!"), QSystemTrayIcon::Warning);
	}
	else
		srv->setAdminPassword(0,0);
	admpass_backup = pass;
	
	pass = srvpass_edit->text();
	if (pass.length() != 0)
	{
		if (req_widestrings)
			str = convert::toUTF16(pass, len);
		else
			str = convert::toUTF8(pass, len);
		
		if (len <= namelength_limit)
			srv->setPassword(str, len);
		else
			emit message(configError, tr("Server password length exceeded allowed size!"), QSystemTrayIcon::Warning);
	}
	else
		srv->setPassword(0,0);
	srvpass_backup = pass;
	
	//srvmutex->unlock();
}

void ConfigDialog::loadSettings()
{
	// todo: use QSettings here
	
	QSettings cfg;
	
	port_spinner->setValue( cfg.value("Port", srv->getPort()).toInt() );
	
	quint8 user_mode = srv->getUserMode();
	
	cfg.beginGroup("Users");
	can_draw->setChecked( cfg.value("Can draw", !fIsSet(user_mode, quint8(protocol::user::Locked))).toBool() );
	can_chat->setChecked( cfg.value("Can chat", !fIsSet(user_mode, quint8(protocol::user::Mute))).toBool() );
	allow_duplicate->setChecked( cfg.value("Allow duplicate connections", !srv->getDuplicateConnectionBlocking()).toBool() );
	cfg.endGroup();
	
	cfg.beginGroup("Requirements");
	wide_strings->setChecked( cfg.value("UTF-16", srv->getUTF16Requirement()).toBool() );
	unique_names->setChecked( cfg.value("Unique names", srv->getUniqueNameEnforcing()).toBool() );
	cfg.endGroup();
	
	int div = (wide_strings->isChecked() ? 2 : 1);
	
	cfg.beginGroup("Limits");
	namelen_spinner->setValue( cfg.value("Name length", srv->getNameLengthLimit() ).toInt() / div );
	sublimit_spinner->setValue( cfg.value("Subscriptions", srv->getSubscriptionLimit()).toInt() );
	ulimit_spinner->setValue( cfg.value("Users", srv->getUserLimit()).toInt() );
	slimit_spinner->setValue( cfg.value("Sessions", srv->getSessionLimit()).toInt() );
	mindim_spinner->setValue( cfg.value("Dimension", srv->getMinDimension()).toInt() );
	cfg.endGroup();
	
	/*
	// These perhaps shouldn't be stored
	cfg.beginGroup("Secret");
	admpass_edit->setText( cfg.value("Root") );
	srvpass_edit->setText( cfg.value("Host") );
	cfg.endGroup();
	*/
}

void ConfigDialog::saveSettings()
{
	save_butt->setDisabled(true);
	
	QSettings cfg;
	
	cfg.setValue("Port", port_spinner->value());
	
	cfg.beginGroup("Users");
	cfg.setValue("Can draw", can_draw->isChecked());
	cfg.setValue("Can chat", can_chat->isChecked());
	cfg.setValue("Allow duplicate connections", allow_duplicate->isChecked());
	cfg.endGroup();
	
	cfg.beginGroup("Requirements");
	cfg.setValue("UTF-16", wide_strings->isChecked());
	cfg.setValue("Unique names", unique_names->isChecked());
	cfg.endGroup();
	
	int mult = (wide_strings->isChecked() ? 2 : 1);
	
	cfg.beginGroup("Limits");
	cfg.setValue("Name length", namelen_spinner->value() * mult);
	cfg.setValue("Subscriptions", sublimit_spinner->value());
	cfg.setValue("Users", ulimit_spinner->value());
	cfg.setValue("Sessions", slimit_spinner->value());
	cfg.setValue("Dimension", mindim_spinner->value());
	cfg.endGroup();
	
	/*
	// These perhaps shouldn't be stored
	cfg.beginGroup("Secret");
	cfg.setValue("Root", admpass_edit->text());
	cfg.setValue("Host", srvpass_edit->text());
	cfg.endGroup();
	*/
}

void ConfigDialog::resetSettings()
{
	apply_butt->setDisabled(true);
	reset_butt->setDisabled(true);
	
	//srvmutex->lock();
	
	quint8 user_mode = srv->getUserMode();
	can_draw->setChecked( !fIsSet(user_mode, quint8(protocol::user::Locked)) );
	can_chat->setChecked( !fIsSet(user_mode, quint8(protocol::user::Mute)) );
	
	allow_duplicate->setChecked( !srv->getDuplicateConnectionBlocking() );
	
	wide_strings->setChecked( srv->getUTF16Requirement() );
	unique_names->setChecked( srv->getUniqueNameEnforcing() );
	port_spinner->setValue( srv->getPort() );
	namelen_spinner->setValue( srv->getNameLengthLimit() );
	sublimit_spinner->setValue( srv->getSubscriptionLimit() );
	mindim_spinner->setValue( srv->getMinDimension() );
	slimit_spinner->setValue( srv->getSessionLimit() );
	ulimit_spinner->setValue( srv->getUserLimit() );
	
	admpass_edit->setText(admpass_backup);
	srvpass_edit->setText(srvpass_backup);
	
	//srvmutex->unlock();
}

/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
*/

#include "logindialog.h"
#include "netstate.h"

#include "ui_logindialog.h"

namespace dialogs {

LoginDialog::LoginDialog(QWidget *parent)
	: QDialog(parent), appenddisconnect_(false)
{
	ui_ = new Ui_LoginDialog;
	ui_->setupUi(this);
	ui_->progress->setMaximum(107);

	connect(ui_->passwordbutton, SIGNAL(clicked()), this, SLOT(sendPassword()));
	connect(ui_->joinbutton, SIGNAL(clicked()), this, SLOT(sendSession()));
}

LoginDialog::~LoginDialog()
{
	delete ui_;
}

/**
 * Show the dialog and display the connecting message.
 * The address is displayed
 * @param address remote host address
 */
void LoginDialog::connecting(const QString& address)
{
	ui_->titlemessage->setText(tr("Connecting to %1...").arg(address));
	ui_->connectmessage->setText(tr("Connecting..."));
	ui_->progress->setValue(0);
	ui_->stackedWidget->setCurrentIndex(0);
	show();
}

/**
 * Move on to the login screen.
 */
void LoginDialog::connected()
{
	ui_->connectmessage->setText(tr("Logging in..."));
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->progress->setValue(1);
}

/**
 * User is now logged in
 */
void LoginDialog::loggedin()
{
	ui_->connectmessage->setText(tr("Logged in"));
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->progress->setValue(2);
}

/**
 * Disconnected, display no sessions message
 */
void LoginDialog::noSessions()
{
	ui_->connectmessage->setText(tr("No sessions were available on the host."));
	ui_->stackedWidget->setCurrentIndex(0);
	appenddisconnect_ = true;
}

/**
 * @param list list of sessions
 */
void LoginDialog::selectSession(const network::SessionList& list)
{
	ui_->sessionlist->clear();
	foreach(const network::Session& s, list) {
		QString title = s.title;
		if(title.isEmpty())
			title = tr("<untitled session>");
		QListWidgetItem *i = new QListWidgetItem(title, ui_->sessionlist);
		i->setData(Qt::UserRole, s.id);
	}
	ui_->stackedWidget->setCurrentIndex(2);
}

/**
 * If the dialog is still visible when this is shown, it means
 * the login/join/host sequence has failed.
 */
void LoginDialog::disconnected(const QString& message)
{
	if(appenddisconnect_) {
		ui_->connectmessage->setText(ui_->connectmessage->text() + "\n"
				+ message);
		appenddisconnect_ = false;
	} else {
		ui_->connectmessage->setText(message);
	}
	ui_->titlemessage->setText(ui_->titlemessage->text() + " " + tr("Failed."));
	ui_->progress->setValue(0);
	ui_->stackedWidget->setCurrentIndex(0);
	disconnect(SIGNAL(rejected()));
}

/**
 * A session was joined. Login sequence is now complete, so hide the dialog
 */
void LoginDialog::joined()
{
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->connectmessage->setText(tr("Joined session, downloading image..."));
}

/**
 * Raster data download progresses
 * @param p progress percentage. When 100, login sequence is complete
 */
void LoginDialog::raster(int p)
{
	ui_->progress->setValue(7+p);
	if(p>=100)
		hide();
}

/**
 * Shows password request page
 */
void LoginDialog::getPassword()
{
	ui_->password->setText(QString());
	ui_->stackedWidget->setCurrentIndex(1);
}

/**
 * Send the password
 */
void LoginDialog::sendPassword()
{
	ui_->connectmessage->setText(tr("Sending password..."));
	ui_->stackedWidget->setCurrentIndex(0);
	emit password(ui_->password->text());
}

/**
 * Send the selected session id
 */
void LoginDialog::sendSession()
{
	ui_->connectmessage->setText(tr("Joining session..."));
	ui_->stackedWidget->setCurrentIndex(0);
	emit session(ui_->sessionlist->currentItem()->data(Qt::UserRole).toInt());
}

}


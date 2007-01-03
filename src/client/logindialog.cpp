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

#include "ui_logindialog.h"

namespace dialogs {

LoginDialog::LoginDialog(QWidget *parent)
	: QDialog(parent), appenddisconnect_(false)
{
	ui_ = new Ui_LoginDialog;
	ui_->setupUi(this);
	ui_->progress->setMaximum(107);

	connect(ui_->passwordbutton, SIGNAL(clicked()), this, SLOT(sendPassword()));
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
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->connectmessage->setText(tr("Connecting..."));
	ui_->progress->setValue(0);
	show();
}

/**
 * Move on to the login screen.
 */
void LoginDialog::connected()
{
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->connectmessage->setText(tr("Logging in..."));
	ui_->progress->setValue(1);
}

/**
 * User is now logged in
 */
void LoginDialog::loggedin()
{
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->connectmessage->setText(tr("Logged in"));
	ui_->progress->setValue(2);
}

/**
 * Disconnected, display no sessions message
 */
void LoginDialog::noSessions()
{
	ui_->stackedWidget->setCurrentIndex(0);
	ui_->connectmessage->setText(tr("No sessions were available on the host."));
	appenddisconnect_ = true;
}

/**
 *
 */
void LoginDialog::disconnected()
{
	ui_->stackedWidget->setCurrentIndex(0);
	QString msg = tr("Disconnected");
	if(appenddisconnect_) {
		msg = ui_->connectmessage->text() + "\n" + msg;
		appenddisconnect_ = false;
	}
	ui_->connectmessage->setText(msg);
	ui_->progress->setValue(0);
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
 * @param session if true, get a session password
 */
void LoginDialog::getPassword(bool session)
{
	ui_->stackedWidget->setCurrentIndex(1);
	ui_->password->setText(QString());
}

/**
 * Send the password
 */
void LoginDialog::sendPassword()
{
	emit password(ui_->password->text());
}

}


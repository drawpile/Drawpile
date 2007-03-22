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

#include <QInputDialog>
#include "logindialog.h"

#include "ui_logindialog.h"
#include "ui_sessiondialog.h"
#include "sessionlistmodel.h"

namespace dialogs {

LoginDialog::LoginDialog(QWidget *parent)
	: QDialog(parent), appenddisconnect_(false)
{
	ui_ = new Ui_LoginDialog;
	ui_->setupUi(this);
	ui_->progress->setMaximum(107);
}

LoginDialog::~LoginDialog()
{
	delete ui_;
}

/**
 * The message is set to the title message label and the window title
 * @param message title message to set
 */
void LoginDialog::setTitleMessage(const QString& message)
{
	setWindowTitle(message);
	ui_->titlemessage->setText(message);
}

/**
 * Show the dialog and display the connecting message.
 * The address is displayed
 * @param address remote host address
 */
void LoginDialog::connecting(const QString& address)
{
	setTitleMessage(tr("Joining a drawing session"));
	ui_->statustext->setText(tr("Connecting to %1...").arg(address));
	ui_->progress->setValue(0);
	ui_->buttonBox->setStandardButtons(QDialogButtonBox::Abort);
	show();
}

/**
 * Connection established. Next step is the login handshake.
 */
void LoginDialog::connected()
{
	ui_->statustext->setText(tr("Logging in..."));
	ui_->progress->setValue(1);
}

/**
 * User is now logged in.
 */
void LoginDialog::loggedin()
{
	ui_->statustext->setText(tr("Logged in"));
	ui_->progress->setValue(2);
}

/**
 * Disconnected, display no sessions message.
 */
void LoginDialog::noSessions()
{
	error(tr("No sessions were available on the host."));
}

/**
 * Disconnected, display session not found message.
 */
void LoginDialog::sessionNotFound()
{
	error(tr("Selected session was not found on the host."));
}

void LoginDialog::error(const QString& message)
{
	ui_->statustext->setText(message);
	appenddisconnect_ = true;
}

/**
 * @param list list of sessions
 */
void LoginDialog::selectSession(const network::SessionList& list)
{
	QDialog sessiondialog(this);
	Ui_SessionSelectDialog sessionselect;
	sessionselect.setupUi(&sessiondialog);
	sessionselect.sessionlist->setModel(new SessionListModel(list));
	sessionselect.sessionlist->selectRow(0);
	if(sessiondialog.exec() == QDialog::Rejected) {
		reject();
	} else {
		ui_->statustext->setText(tr("Joining session..."));
		emit session(list.at(sessionselect.sessionlist->currentIndex().row()).id);
	}
}

/**
 * If the dialog is still visible when this is shown, it means
 * the login/join/host sequence has failed.
 */
void LoginDialog::disconnected(const QString& message)
{
	if(appenddisconnect_) {
		ui_->statustext->setText(ui_->statustext->text() + "\n" + message);
		appenddisconnect_ = false;
	} else {
		ui_->statustext->setText(message);
	}
	ui_->progress->setValue(0);
	setTitleMessage(tr("Couldn't join a session"));
	disconnect(SIGNAL(rejected()));
	ui_->buttonBox->setStandardButtons(QDialogButtonBox::Close);
}

/**
 * A session was joined. Board contents is now being downloaded.
 */
void LoginDialog::joined()
{
	ui_->statustext->setText(tr("Downloading board contents..."));
}

/**
 * Raster data download progresses. When progress hits 100, the download
 * sequence is complete and the dialog is hidden.
 * @param p progress percentage.
 */
void LoginDialog::raster(int p)
{
	ui_->progress->setValue(7+p);
	if(p>=100)
		hide();
}

/**
 * Request a password. TODO, session or host password?
 */
void LoginDialog::getPassword()
{
	bool ok;
	QString passwd = QInputDialog::getText(this,
			tr("Password required"),
			tr("Password:"),
			QLineEdit::Password,
			QString(),
			&ok);
	if(ok) {
		ui_->statustext->setText(tr("Sending password..."));
		emit password(passwd);
	} else {
		reject();
	}
}

}


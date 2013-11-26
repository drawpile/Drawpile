/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QApplication>
#include <QDebug>
#include <QStringList>
#include <QInputDialog>

#include "net/login.h"
#include "net/server.h"
#include "../shared/net/login.h"
#include "../shared/net/meta.h"
#include "../shared/net/constants.h"
#include "version.h"

namespace net {

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() != protocol::MSG_LOGIN) {
		_server->loginFailure(QApplication::tr("Invalid state"));
		return;
	}

	QString msg = message.cast<protocol::Login>().message();

	qDebug() << "LOGIN:" << msg;

	switch(_state) {
	case 0: expectHello(msg); break;
	case 1: expectPasswordResponse(msg); break;
	case 2: expectLoginOk(msg); break;
	default: _server->loginFailure(QApplication::tr("Unexpected response"));
	}
}

void LoginHandler::expectHello(const QString &msg)
{
	// Hello response should be in format "DRAWPILE <major.minor> [PASS]"
	QStringList tokens = msg.split(' ', QString::SkipEmptyParts);

	if(tokens.length() < 2 || tokens.length() > 3) {
		qWarning() << "Login error. Expected hello, got:" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	if(tokens[0] != "DRAWPILE") {
		qWarning() << "Login error. Expected \"DRAWPILE\", got:" << tokens[0];
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	QStringList versions = tokens[1].split('.', QString::SkipEmptyParts);
	if(versions.length()!=2) {
		qWarning() << "Login error. Expected <minor>.<major>, got:" << tokens[1];
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}
	// Major version must match ours
	bool ok;
	int majorVersion = versions[0].toInt(&ok);
	if(!ok || majorVersion != protocol::REVISION) {
		qWarning() << "Login error. Server major version mismatch.";
		_server->loginFailure(QApplication::tr("Server is for a different DrawPile version!"));
		return;
	}

	// Minor version (if set) must also match ours
	int minorVersion = versions[1].toInt(&ok);
	if(!ok || (minorVersion>0 && minorVersion != version::MINOR_REVISION)) {
		qWarning() << "Login error. Server minor version mismatch";
		_server->loginFailure(QApplication::tr("Session for different DrawPile version in progress!"));
		return;
	}

	// Is a password needed?
	bool needpassword = false;
	if(tokens.length()==3) {
		if(tokens[2] == "PASS") {
			needpassword = true;
		} else {
			_server->loginFailure(QApplication::tr("Incompatible server"));
			return;
		}
	}

	if(needpassword) {
		QString pass = QInputDialog::getText(
			0,
			QApplication::tr("Session is password protected"),
			QApplication::tr("Enter password"),
			QLineEdit::Password
		);

		_server->sendMessage(protocol::MessagePtr(new protocol::Login(pass)));
		_state = 1;
	} else {
		// Proceed to next state
		sendLogin();
		_state = 2;
	}
}

void LoginHandler::expectPasswordResponse(const QString &msg)
{
	if(msg == "OK") {
		sendLogin();
		_state = 2;
	} else if(msg=="BADPASS") {
		_server->loginFailure(QApplication::tr("Incorrect password"));
	} else {
		// This shouldn't happen
		_server->loginFailure("Unknown password error");
	}
}

void LoginHandler::sendLogin()
{
	QString msg;
	switch(_mode) {
	case HOST:
		msg = QString("HOST %1 %2 %3").arg(version::MINOR_REVISION).arg(_userid).arg(_address.userName());
		break;
	case JOIN:
		msg = QString("JOIN %1").arg(_address.userName());
		break;
	}

	_server->sendMessage(protocol::MessagePtr(new protocol::Login(msg)));
}

void LoginHandler::expectLoginOk(const QString &msg)
{
	// Check for valid error responses
	if(msg == "BADNAME") {
		_server->loginFailure(QApplication::tr("Username invalid or reserved"));
		return;
	} else if(msg == "NOSESSION") {
		_server->loginFailure(QApplication::tr("Session does not exist yet!"));
		return;
	} else if(msg == "CLOSED") {
		_server->loginFailure(QApplication::tr("Session is closed!"));
		return;
	}

	// OK response should be in format "OK <userid>"
	QStringList tokens = msg.split(' ', QString::SkipEmptyParts);
	if(tokens.length() != 2 || tokens[0] != "OK") {
		qWarning() << "Login error. Expected OK, got:" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	bool ok;
	int userid = tokens[1].toInt(&ok);
	if(!ok) {
		qWarning() << "Login error. Got invalid user ID:" << tokens[1];
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	_userid = userid;

	// Login complete!
	_server->loginSuccess();

	// If in host mode, send initial session settings
	if(_mode==HOST) {
		if(!_password.isEmpty())
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(0, QString("/password %1").arg(_password))));
		_server->sendMessage(protocol::MessagePtr(new protocol::SessionTitle(_title)));
		if(_maxusers>0)
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(0, QString("/maxusers %1").arg(_maxusers))));
		if(!_allowdrawing)
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(0, QString("/lockdefault").arg(_maxusers))));
	}
}
}

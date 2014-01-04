/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#include <QDebug>
#include <QApplication>
#include <QStringList>
#include <QInputDialog>

#include "config.h"
#include "net/login.h"
#include "net/server.h"
#include "../shared/net/login.h"
#include "../shared/net/meta.h"

namespace net {

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() != protocol::MSG_LOGIN) {
		qWarning() << "Login error: got message type" << message->type() << "when expected type 0";
		_server->loginFailure(QApplication::tr("Invalid state"));
		return;
	}

	QString msg = message.cast<protocol::Login>().message();

	qDebug() << "login" << msg;
	switch(_state) {
	case 0: expectHello(msg); break;
	case 1: expectSessionDescription(msg); break;
	case 2: expectLoginOk(msg); break;
	default: _server->loginFailure(QApplication::tr("Unexpected response"));
	}
}

void LoginHandler::expectHello(const QString &msg)
{
	// Hello response should be in format "DRAWPILE <major>"
	QStringList tokens = msg.split(' ', QString::SkipEmptyParts);

	if(tokens.length() != 2) {
		qWarning() << "Login error. Expected hello, got:" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	if(tokens[0] != "DRAWPILE") {
		qWarning() << "Login error. Expected \"DRAWPILE\", got:" << tokens[0];
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	// Major version must match ours
	bool ok;
	int majorVersion = tokens[1].toInt(&ok);
	if(!ok || majorVersion != DRAWPILE_PROTO_MAJOR_VERSION) {
		qWarning() << "Login error. Server major version mismatch.";
		_server->loginFailure(QApplication::tr("Server is for a different DrawPile version!"));
		return;
	}

	// Next, we expect the session description
	_state = 1;
}

void LoginHandler::expectSessionDescription(const QString &msg)
{
	if(msg == "NOSESSION") {
		if(_mode != HOST) {
			_server->loginFailure(QApplication::tr("Session does not exist yet!"));
			return;
		}

		// Ok, we're good to host
		QString hostmsg = QString("HOST %1 %2 %3").arg(DRAWPILE_PROTO_MINOR_VERSION).arg(_userid).arg(_address.userName());
		_server->sendMessage(protocol::MessagePtr(new protocol::Login(hostmsg)));
	} else {
		QStringList tokens = msg.split(' ', QString::SkipEmptyParts);
		// Expect session description in format:
		// SESSION <minorVersion> [PASS]
		if(tokens.size() < 2 || tokens.size() > 3) {
			qWarning() << "Login error. Expected session description, got:" << msg;
			_server->loginFailure(QApplication::tr("Incompatible server"));
			return;
		}

		bool ok;
		int minorVersion = tokens[1].toInt(&ok);
		if(!ok || minorVersion != DRAWPILE_PROTO_MINOR_VERSION) {
			qWarning() << "Login error. Server minor version mismatch";
			_server->loginFailure(QApplication::tr("Session for a different DrawPile version in progress!"));
			return;
		}

		bool needpass = false;
		if(tokens.size()==3) {
			if(tokens[2] == "PASS")
				needpass = true;
			else {
				qWarning() << "Login error. Unknown session flag:" << tokens[2];
				_server->loginFailure(QApplication::tr("Incompatible server"));
				return;
			}
		}

		QString password;
		if(needpass) {
			password = QInputDialog::getText(
				0,
				QApplication::tr("Session is password protected"),
				QApplication::tr("Enter password"),
				QLineEdit::Password
			);
		}

		// So far so good, send join message
		QString joinmsg = QString("JOIN %1 %2").arg(_address.userName()).arg(password);
		_server->sendMessage(protocol::MessagePtr(new protocol::Login(joinmsg)));
	}

	// Expect OK/Error response
	_state = 2;
}

void LoginHandler::expectLoginOk(const QString &msg)
{
	// Check for valid error responses
	if(msg == "BADNAME") {
		_server->loginFailure(QApplication::tr("Username invalid or reserved"));
		return;
	} else if(msg == "CLOSED") {
		_server->loginFailure(QApplication::tr("Session is closed!"));
		return;
	} else if(msg == "BADPASS") {
		_server->loginFailure(QApplication::tr("Incorrect password"));
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
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, QString("/password %1").arg(_password))));
		_server->sendMessage(protocol::MessagePtr(new protocol::SessionTitle(_userid, _title)));
		if(_maxusers>0)
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, QString("/maxusers %1").arg(_maxusers))));
		if(!_allowdrawing)
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, QString("/lockdefault"))));
		if(_layerctrllock)
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, QString("/locklayerctrl"))));
		else
			_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, QString("/unlocklayerctrl"))));
	}
}
}

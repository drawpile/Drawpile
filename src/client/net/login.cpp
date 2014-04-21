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
#include <QRegExp>

#include "config.h"

#include "dialogs/selectsessiondialog.h"
#include "net/login.h"
#include "net/loginsessions.h"
#include "net/server.h"

#include "../shared/net/login.h"
#include "../shared/net/meta.h"

namespace net {

LoginHandler::LoginHandler(Mode mode, const QUrl &url, QWidget *parent)
	: QObject(parent),
	  _mode(mode),
	  _address(url),
	  _widgetParent(parent),
	  _maxusers(0),
	  _allowdrawing(true),
	  _layerctrllock(true),
	  _state(EXPECT_HELLO),
	  _multisession(false)
{
	_sessions = new LoginSessionModel(this);
}

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() != protocol::MSG_LOGIN) {
		qWarning() << "Login error: got message type" << message->type() << "when expected type 0";
		_server->loginFailure(QApplication::tr("Invalid state"));
		return;
	}

	QString msg = message.cast<protocol::Login>().message();

	switch(_state) {
	case EXPECT_HELLO: expectHello(msg); break;
	case EXPECT_SESSIONLIST_TO_HOST: expectSessionDescriptionHost(msg); break;
	case EXPECT_SESSIONLIST_TO_JOIN: expectSessionDescriptionJoin(msg); break;
	case EXPECT_LOGIN_OK: expectLoginOk(msg); break;
	default: _server->loginFailure(QApplication::tr("Unexpected response"));
	}
}

void LoginHandler::expectHello(const QString &msg)
{
	// Greeting should be in format "DRAWPILE <majorVersion> <flags>"
	QRegExp re("DRAWPILE (\\d+) (-|[\\w,]+)");

	if(!re.exactMatch(msg)) {
		qWarning() << "Login error. Invalid greeting:" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	// Major version must match ours
	int majorVersion = re.cap(1).toInt();
	if(majorVersion != DRAWPILE_PROTO_MAJOR_VERSION) {
		qWarning() << "Login error. Server major version mismatch.";
		_server->loginFailure(QApplication::tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	QStringList flags = re.cap(2).split(",");
	bool hostPassword=false;
	for(const QString &flag : flags) {
		if(flag == "-") {
			// no flags
		} else if(flag == "MULTI") {
			_multisession = true;
		} else if(flag == "HOSTP") {
			hostPassword = true;
		} else if(flag == "SECURE") {
			// Encryption is not supported yet in this version. Ignore this flag
		} else if(flag == "SECNOW") {
			// Encryption not yet supported, but server demands it!
			qWarning() << "Login error. Server demands encryption!";
			_server->loginFailure(QApplication::tr("This version does not support encrypted connections!"));
			return;
		} else if(flag == "PERSIST") {
			// TODO indicate persistent session support
		} else {
			qWarning() << "Unknown server capability:" << flag;
		}
	}

	// Query host password if needed
	if(_mode == HOST && hostPassword) {
		_hostPassword = QInputDialog::getText(
			_widgetParent,
			QApplication::tr("Password is needed to host a session"),
			QApplication::tr("Enter password"),
			QLineEdit::Password
		);
	}

	// Show session selector if needed
	if(_mode == JOIN && _multisession) {
		auto *dialog = new dialogs::SelectSessionDialog(_sessions, _widgetParent);
		dialog->setModal(true);
		dialog->setAttribute(Qt::WA_DeleteOnClose);

		connect(dialog, SIGNAL(selected(int,bool)), this, SLOT(joinSelectedSession(int,bool)));
		connect(dialog, &QDialog::rejected, [this]() {
			_server->loginFailure(QApplication::tr("Cancelled"));
		});

		dialog->show();
	}

	// Next, we expect the session description
	_state = (_mode == HOST) ? EXPECT_SESSIONLIST_TO_HOST : EXPECT_SESSIONLIST_TO_JOIN;
}

void LoginHandler::expectSessionDescriptionHost(const QString &msg)
{
	Q_ASSERT(_mode == HOST);

	if(msg.startsWith("NOSESSION") || msg.startsWith("SESSION")) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead

		QString hostmsg = QString("HOST %1 %2 \"%3\"")
				.arg(DRAWPILE_PROTO_MINOR_VERSION)
				.arg(_userid)
				.arg(_address.userName());

		if(!_hostPassword.isEmpty())
			hostmsg += ";" + _hostPassword;

		send(hostmsg);
		_state = EXPECT_LOGIN_OK;

	} else {
		qWarning() << "Expected session list, got" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}
}

void LoginHandler::expectSessionDescriptionJoin(const QString &msg)
{
	Q_ASSERT(_mode == JOIN);

	if(msg.startsWith("NOSESSION")) {
		// No sessions
		if(!_multisession) {
			_server->loginFailure(QApplication::tr("Session does not exist yet!"));
		}

		int sep = msg.indexOf(' ');
		if(sep>0) {
			bool ok;
			int id = msg.mid(sep+1).toInt(&ok);
			if(ok)
				_sessions->removeSession(id);
			else
				qWarning() << "invalid NOSESSION message:" << msg;
		}
		return;
	}

	LoginSession session;

	// Expect session description in format:
	// SESSION <id> <minorVersion> <FLAGS> <user-count> "title"
	QRegExp re("SESSION (\\d+) (\\d+) (-|[\\w,]+) (\\d+) \"([^\"]*)\"");
	if(!re.exactMatch(msg)) {
		qWarning() << "Login error. Expected session description, got:" << msg;
		_server->loginFailure(QApplication::tr("Incompatible server"));
		return;
	}

	session.id = re.cap(1).toInt();

	const int minorVersion = re.cap(2).toInt();
	if(minorVersion != DRAWPILE_PROTO_MINOR_VERSION) {
		qWarning() << "Session" << session.id << "minor version" << minorVersion << "mismatch.";
		session.incompatible = true;
	}

	const QStringList flags = re.cap(3).split(",");
	for(const QString &flag : flags) {
		if(flag=="-") {
			// No flags marker
		} else if(flag=="PASS") {
			session.needPassword = true;
		} else if(flag=="CLOSED") {
			session.closed = true;
		} else if(flag=="PERSIST") {
			session.persistent = true;
		} else {
			// TODO multisession: show as disabled
			qWarning() << "Session" << session.id << "has unknown flag:" << flag;
			session.incompatible = true;
		}
	}

	session.userCount = re.cap(4).toInt();
	session.title = re.cap(5);

	if(_multisession) {
		// Multisesion mode: add session to list which is presented to the user
		_sessions->updateSession(session);

	} else {
		// Single session mode: automatically join the (only) session
		if(session.incompatible) {
			_server->loginFailure(QApplication::tr("Session for a different Drawpile version in progress!"));
			return;
		}

		joinSelectedSession(session.id, session.needPassword);
	}
}

void LoginHandler::expectLoginOk(const QString &msg)
{
	if(msg.startsWith("SESSION") || msg.startsWith("NOSESSION")) {
		// We can still get session list updates here. They are safe to ignore.
		return;
	}

	if(msg.startsWith("ERROR ")) {
		const QString ecode = msg.mid(msg.indexOf(' ') + 1);
		qWarning() << "Login error:" << ecode;
		QString error;
		if(ecode == "BADPASS")
			error = QApplication::tr("Incorrect password");
		else if(ecode == "BADNAME")
			error = QApplication::tr("Invalid username");
		else if(ecode == "CLOSED")
			error = QApplication::tr("Session is closed");
		else if(ecode == "SESLIMIT")
			error = QApplication::tr("Session limit reached");
		else
			error = QApplication::tr("Unknown error (%1)").arg(ecode);

		_server->loginFailure(error);
		return;

	}

	if(msg.startsWith("OK ")) {
		QRegExp re("OK (\\d+)");
		if(!re.exactMatch(msg)) {
			qWarning() << "Login error. Expected OK <id>, got:" << msg;
			_server->loginFailure(QApplication::tr("Incompatible server"));
			return;
		}

		_userid = re.cap(1).toInt();
		_server->loginSuccess();

		// If in host mode, send initial session settings
		if(_mode==HOST) {
			QStringList init;
			init << "/title " + _title;

			if(!_sessionPassword.isEmpty())
				init << "/password " + _sessionPassword;

			if(_maxusers>0)
				init << QString("/maxusers %1").arg(_maxusers);

			if(!_allowdrawing)
				init <<  "/lockdefault";

			if(_layerctrllock)
				init << "/locklayerctrl";
			else
				init << "/unlocklayerctrl";

			for(const QString msg : init)
				_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, msg)));
		}
		return;
	}

	// Unexpected command
	qWarning() << "Login error. Unexpected response:" << msg;
	_server->loginFailure(QApplication::tr("Incompatible server"));
}

void LoginHandler::joinSelectedSession(int id, bool needPassword)
{
	QString joinmsg = QString("JOIN %1 \"%2\"")
			.arg(id)
			.arg(_address.userName());

	if(needPassword) {
		QString password = QInputDialog::getText(
			_widgetParent,
			QApplication::tr("Session is password protected"),
			QApplication::tr("Enter password"),
			QLineEdit::Password
		);

		joinmsg += ";" + password;
	}

	send(joinmsg);
	_state = EXPECT_LOGIN_OK;
}

void LoginHandler::send(const QString &message)
{
	_server->sendMessage(protocol::MessagePtr(new protocol::Login(message)));
}

}

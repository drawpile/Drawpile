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
#include <QStringList>
#include <QInputDialog>
#include <QRegularExpression>

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

void LoginHandler::serverDisconnected()
{
	if(_selectorDialog)
		_selectorDialog->deleteLater();
	if(_passwordDialog)
		_passwordDialog->deleteLater();
}

void LoginHandler::receiveMessage(protocol::MessagePtr message)
{
	if(message->type() != protocol::MSG_LOGIN) {
		qWarning() << "Login error: got message type" << message->type() << "when expected type 0";
		_server->loginFailure(tr("Invalid state"));
		return;
	}

	QString msg = message.cast<protocol::Login>().message();

	// Overall, the login process is:
	// 1. wait for server greeting
	// 2. get hosting password from user if needed
	// 3. wait for session list
	// 4. wait for user to finish typing host/join password if needed
	// 5. send host/join command
	// 6. wait for OK

	switch(_state) {
	case EXPECT_HELLO: expectHello(msg); break;
	case EXPECT_SESSIONLIST_TO_HOST: expectSessionDescriptionHost(msg); break;
	case EXPECT_SESSIONLIST_TO_JOIN: expectSessionDescriptionJoin(msg); break;
	case WAIT_FOR_JOIN_PASSWORD:
	case WAIT_FOR_HOST_PASSWORD: expectNoErrors(msg); break;
	case EXPECT_LOGIN_OK: expectLoginOk(msg); break;
	}
}

void LoginHandler::expectHello(const QString &msg)
{
	// Greeting should be in format "DRAWPILE <majorVersion> <flags>"
	const QRegularExpression re("\\ADRAWPILE (\\d+) (-|[\\w,]+)\\z");
	auto m = re.match(msg);

	if(!m.hasMatch()) {
		qWarning() << "Login error. Invalid greeting:" << msg;
		_server->loginFailure(tr("Incompatible server"));
		return;
	}

	// Major version must match ours
	int majorVersion = m.captured(1).toInt();
	if(majorVersion != DRAWPILE_PROTO_MAJOR_VERSION) {
		qWarning() << "Login error. Server major version mismatch.";
		_server->loginFailure(tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	QStringList flags = m.captured(2).split(",");
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
			_server->loginFailure(tr("Secure connection support not available!"));
			return;
		} else if(flag == "PERSIST") {
			// TODO indicate persistent session support
		} else {
			qWarning() << "Unknown server capability:" << flag;
		}
	}

	// Query host password if needed
	if(_mode == HOST && hostPassword) {
		showPasswordDialog(tr("Password is needed to host a session"), tr("Enter hosting password"));
	}

	// Show session selector if in multisession mode
	if(_mode == JOIN && _multisession) {
		_selectorDialog = new dialogs::SelectSessionDialog(_sessions, _widgetParent);
		_selectorDialog->setWindowModality(Qt::WindowModal);
		_selectorDialog->setAttribute(Qt::WA_DeleteOnClose);

		connect(_selectorDialog, SIGNAL(selected(int,bool)), this, SLOT(joinSelectedSession(int,bool)));
		connect(_selectorDialog, SIGNAL(rejected()), this, SLOT(cancelLogin()));

		_selectorDialog->show();
	}

	// Next, we expect the session description
	_state = (_mode == HOST) ? EXPECT_SESSIONLIST_TO_HOST : EXPECT_SESSIONLIST_TO_JOIN;
}

void LoginHandler::showPasswordDialog(const QString &title, const QString &text)
{
	Q_ASSERT(_passwordDialog.isNull());

	_passwordDialog = new QInputDialog(_widgetParent);
	_passwordDialog->setWindowTitle(title);
	_passwordDialog->setLabelText(text);
	_passwordDialog->setTextEchoMode(QLineEdit::Password);
	_passwordDialog->setWindowModality(Qt::WindowModal);

	connect(_passwordDialog, SIGNAL(accepted()), this, SLOT(passwordSet()));
	connect(_passwordDialog, SIGNAL(rejected()), this, SLOT(cancelLogin()));

	// note. We use QueuedConnection above, to give the dialog time to get deleted before
	// this object gets deleted.

	_passwordDialog->show();
}

void LoginHandler::passwordSet()
{
	Q_ASSERT(!_passwordDialog.isNull());

	switch(_state) {
	case WAIT_FOR_HOST_PASSWORD:
		_hostPassword = _passwordDialog->textValue();
		sendHostCommand();
		break;
	case WAIT_FOR_JOIN_PASSWORD:
		_joinPassword = _passwordDialog->textValue();
		sendJoinCommand();
		break;
	default:
		// shouldn't happen...
		qFatal("invalid state");
		return;
	}
}

void LoginHandler::expectSessionDescriptionHost(const QString &msg)
{
	Q_ASSERT(_mode == HOST);

	if(msg.startsWith("NOSESSION") || msg.startsWith("SESSION")) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead

		// No password needed or password already set: go ahead
		if(_passwordDialog.isNull() || !_hostPassword.isEmpty())
			sendHostCommand();
		else
			_state = WAIT_FOR_HOST_PASSWORD;

	} else {
		qWarning() << "Expected session list, got" << msg;
		_server->loginFailure(tr("Incompatible server"));
		return;
	}
}

void LoginHandler::sendHostCommand()
{
	QString hostmsg = QString("HOST %1 %2 \"%3\"")
			.arg(DRAWPILE_PROTO_MINOR_VERSION)
			.arg(_userid)
			.arg(_address.userName());

	if(!_hostPassword.isEmpty())
		hostmsg += ";" + _hostPassword;

	send(hostmsg);
	_state = EXPECT_LOGIN_OK;
}

void LoginHandler::expectSessionDescriptionJoin(const QString &msg)
{
	Q_ASSERT(_mode == JOIN);

	if(msg.startsWith("NOSESSION")) {
		// No sessions
		if(!_multisession) {
			_server->loginFailure(tr("Session does not exist yet!"));
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
	const QRegularExpression re("\\ASESSION (\\d+) (\\d+) (-|[\\w,]+) (\\d+) \"([^\"]*)\"\\z");
	auto m = re.match(msg);

	if(!m.hasMatch()) {
		qWarning() << "Login error. Expected session description, got:" << msg;
		_server->loginFailure(tr("Incompatible server"));
		return;
	}

	session.id = m.captured(1).toInt();

	const int minorVersion = m.captured(2).toInt();
	if(minorVersion != DRAWPILE_PROTO_MINOR_VERSION) {
		qWarning() << "Session" << session.id << "minor version" << minorVersion << "mismatch.";
		session.incompatible = true;
	}

	const QStringList flags = m.captured(3).split(",");
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
			qWarning() << "Session" << session.id << "has unknown flag:" << flag;
			session.incompatible = true;
		}
	}

	session.userCount = m.captured(4).toInt();
	session.title = m.captured(5);

	if(_multisession) {
		// Multisesion mode: add session to list which is presented to the user
		_sessions->updateSession(session);

	} else {
		// Single session mode: automatically join the (only) session
		if(session.incompatible) {
			_server->loginFailure(tr("Session for a different Drawpile version in progress!"));
			return;
		}

		joinSelectedSession(session.id, session.needPassword);
		_state = WAIT_FOR_JOIN_PASSWORD;
	}
}

void LoginHandler::expectNoErrors(const QString &msg)
{
	// A "do nothing" handler while waiting for the user to enter a password
	if(msg.startsWith("SESSION") || msg.startsWith("NOSESSION"))
		return;

	if(msg.startsWith("ERROR")) {
		const QString ecode = msg.mid(msg.indexOf(' ') + 1);
		qWarning() << "Server error while waiting for password:" << ecode;

		_server->loginFailure(tr("An error occurred"));
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
			error = tr("Incorrect password");
		else if(ecode == "BADNAME")
			error = tr("Invalid username");
		else if(ecode == "CLOSED")
			error = tr("Session is closed");
		else if(ecode == "SESLIMIT")
			error = tr("Session limit reached");
		else
			error = tr("Unknown error (%1)").arg(ecode);

		_server->loginFailure(error);
		return;

	}

	if(msg.startsWith("OK ")) {
		const QRegularExpression re("\\AOK (\\d+)\\z");
		auto m = re.match(msg);

		if(!m.hasMatch()) {
			qWarning() << "Login error. Expected OK <id>, got:" << msg;
			_server->loginFailure(tr("Incompatible server"));
			return;
		}

		_userid = m.captured(1).toInt();
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

			if(_requestPersistent)
				init << "/persist";

			for(const QString msg : init)
				_server->sendMessage(protocol::MessagePtr(new protocol::Chat(_userid, msg)));
		}
		return;
	}

	// Unexpected command
	qWarning() << "Login error. Unexpected response:" << msg;
	_server->loginFailure(tr("Incompatible server"));
}

void LoginHandler::joinSelectedSession(int id, bool needPassword)
{
	_selectedId = id;
	if(needPassword) {
		showPasswordDialog(tr("Session is password protected"), tr("Enter session password"));
		_state = WAIT_FOR_JOIN_PASSWORD;

	} else {
		sendJoinCommand();
	}
}

void LoginHandler::sendJoinCommand()
{
	QString joinmsg = QString("JOIN %1 \"%2\"")
			.arg(_selectedId)
			.arg(_address.userName());

	if(!_joinPassword.isEmpty()) {
		joinmsg.append(';');
		joinmsg.append(_joinPassword);
	}

	send(joinmsg);
	_state = EXPECT_LOGIN_OK;
}

void LoginHandler::cancelLogin()
{
	_server->loginFailure(tr("Cancelled"), true);
}

void LoginHandler::send(const QString &message)
{
	_server->sendMessage(protocol::MessagePtr(new protocol::Login(message)));
}

}

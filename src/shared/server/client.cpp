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

#include <QTcpSocket>
#include <QStringList>

#include "server.h"
#include "client.h"
#include "exceptions.h"

#include "../net/messagequeue.h"
#include "../net/annotation.h"
#include "../net/image.h"
#include "../net/layer.h"
#include "../net/login.h"
#include "../net/meta.h"
#include "../net/pen.h"
#include "../net/snapshot.h"
#include "../net/constants.h"

namespace server {

using protocol::MessagePtr;

Client::Client(Server *server, QTcpSocket *socket)
	: QObject(server),
	  _server(server),
	  _socket(socket),
	  _state(LOGIN), _substate(0),
	  _awaiting_snapshot(false),
	  _uploading_snapshot(false),
	  _streampointer(0),
	  _substreampointer(-1),
	  _id(0),
	  _isOperator(false),
	  _userLock(false)
{
	_msgqueue = new protocol::MessageQueue(socket, this);

	connect(_socket, SIGNAL(disconnected()), this, SLOT(socketDisconnect()));
	connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError()));
	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(receiveMessages()));
	connect(_msgqueue, SIGNAL(snapshotAvailable()), this, SLOT(receiveSnapshot()));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(gotBadData(int,int)));

	// Client just connected, start by saying hello
	QString hello = QString("DRAWPILE %1.%2").arg(protocol::REVISION).arg(_server->session().minorVersion);
	_msgqueue->send(MessagePtr(new protocol::Login(hello)));
	// No password protection (TODO), so we are expecting a HOST/JOIN command
	_substate = 1;
}

Client::~Client()
{
	delete _socket;
}

QHostAddress Client::peerAddress() const
{
	return _socket->peerAddress();
}

void Client::sendAvailableCommands()
{
	if(_state != IN_SESSION)
		return;

	if(_substreampointer>=0) {
		// Are we downloading a substream?
		const protocol::MessagePtr sptr = _server->mainstream().at(_streampointer);
		Q_ASSERT(sptr->type() == protocol::MSG_SNAPSHOT);
		const protocol::SnapshotPoint &sp = sptr.cast<const protocol::SnapshotPoint>();

		// Enqueue substream
		while(_substreampointer < sp.substream().length())
			_msgqueue->send(sp.substream().at(_substreampointer++));

		if(sp.isComplete()) {
			_substreampointer = -1;
			++_streampointer;
			sendAvailableCommands();
		}
	} else {
		// No substream in progress, enqueue normal commands
		// Snapshot points (substreams) are skipped.
		while(_streampointer < _server->mainstream().end()) {
			MessagePtr msg = _server->mainstream().at(_streampointer++);
			if(msg->type() != protocol::MSG_SNAPSHOT)
				_msgqueue->send(msg);
		}
	}

}

void Client::receiveMessages()
{
	while(_msgqueue->isPending()) {
		MessagePtr msg = _msgqueue->getPending();

		switch(_state) {
		case LOGIN:
			handleLoginMessage(msg.cast<protocol::Login>());
			break;
		case WAIT_FOR_SYNC:
		case IN_SESSION:
			handleSessionMessage(msg);
			break;
		}
	}
}

void Client::receiveSnapshot()
{
	if(!_uploading_snapshot) {
		_server->printError(QString("Received snapshot data from client %1 when not expecting it!").arg(_id));
		_socket->disconnect();
		return;
	}

	while(_msgqueue->isPendingSnapshot()) {
		MessagePtr msg = _msgqueue->getPendingSnapshot();

		// Filter away blatantly unallowed messages
		switch(msg->type()) {
		using namespace protocol;
		case MSG_LOGIN:
		case MSG_SESSION_CONFIG:
		case MSG_STREAMPOS:
			continue;
		default: break;
		}

		// Add message
		if(_server->addToSnapshotStream(msg)) {
			_server->printDebug(QString("Finished getting snapshot from client %1").arg(_id));
			_uploading_snapshot = false;

			// Graduate to session
			if(_state == WAIT_FOR_SYNC) {
				_server->session().syncInitialState(_server->mainstream().snapshotPoint().cast<protocol::SnapshotPoint>().substream());
				_state = IN_SESSION;
				enqueueHeldCommands();
				sendAvailableCommands();
			}

			if(_msgqueue->isPendingSnapshot()) {
				_server->printError(QString("Client %1 sent too much snapshot data!").arg(_id));
				_socket->disconnect();
			}
			break;
		}
	}
}

void Client::gotBadData(int len, int type)
{
	_server->printError(QString("Received unknown message type #%1 of length %2 from %3").arg(type).arg(len).arg(peerAddress().toString()));
	_socket->close();
}

void Client::socketError()
{
	_server->printError(QString("Socket error %1 (from %2)").arg(_socket->errorString()).arg(peerAddress().toString()));
	_socket->close();
}

void Client::socketDisconnect()
{
	if(_id>0) {
		_server->session().userids.release(_id);
		_server->addToCommandStream(MessagePtr(new protocol::UserLeave(_id)));
	}
	emit disconnected(this);
}

void Client::requestSnapshot()
{
	Q_ASSERT(_state != LOGIN);
	_msgqueue->send(MessagePtr(new protocol::SnapshotMode(protocol::SnapshotMode::REQUEST)));
	_awaiting_snapshot = true;

	_server->addSnapshotPoint();
	_uploading_snapshot = true;

	_server->printDebug(QString("Created a new snapshot point and requested data from client %1").arg(_id));
}

/**
 * @brief Handle messages in normal session mode
 *
 * This one is pretty simple. The message is validated to make sure
 * the client is authorized to send it, etc. and it is added to the
 * main message stream, from which it is distributed to all connected clients.
 * @param msg the message received from the client
 */
void Client::handleSessionMessage(MessagePtr msg)
{
	// Filter away blatantly unallowed messages
	switch(msg->type()) {
	using namespace protocol;
	case MSG_LOGIN:
	case MSG_USER_JOIN:
	case MSG_USER_ATTR:
	case MSG_USER_LEAVE:
	case MSG_SESSION_CONFIG:
	case MSG_STREAMPOS:
		_server->printDebug(QString("Warning: user #%1 sent server-to-user only command %2").arg(_id).arg(msg->type()));
		return;
	default: break;
	}

	if(msg->isOpCommand() && !_isOperator) {
		_server->printDebug(QString("Warning: normal user #%1 tried to use operator command %2").arg(_id).arg(msg->type()));
		return;
	}

	// Locking (note. applies only to command stream)
	if(msg->isCommand()) {
		if(isDropLocked()) {
			// ignore command
			return;
		} else if(isHoldLocked()) {
			_holdqueue.append(msg);
			return;
		}

		// Layer specific locking. Drop commands that affect layer contents
		switch(msg->type()) {
		using namespace protocol;
		case MSG_PEN_MOVE:
			if(isLayerLocked(_server->session().drawingctx[_id].currentLayer))
				return;
			break;
		case MSG_LAYER_ATTR:
			if(isLayerLocked(msg.cast<LayerAttributes>().id()))
				return;
			break;
		case MSG_LAYER_DELETE:
			if(isLayerLocked(msg.cast<LayerDelete>().id()))
				return;
			break;
		case MSG_PUTIMAGE:
			if(isLayerLocked(msg.cast<PutImage>().layer()))
				return;
			break;
		default: /* other types are always allowed */ break;
		}
	}

	// Make sure the origin user ID is set
	msg->setOrigin(_id);

	// Track state and special commands
	switch(msg->type()) {
	using namespace protocol;
	case MSG_TOOLCHANGE:
		_server->session().drawingContextToolChange(msg.cast<ToolChange>());
		break;
	case MSG_PEN_MOVE:
		_server->session().drawingContextPenDown(msg.cast<PenMove>());
		break;
	case MSG_PEN_UP:
		_server->session().drawingContextPenUp(msg.cast<PenUp>());
		break;
	case MSG_LAYER_CREATE:
		_server->session().createLayer(msg.cast<LayerCreate>(), true);
		break;
	case MSG_LAYER_ORDER:
		_server->session().reorderLayers(msg.cast<LayerOrder>());
		break;
	case MSG_LAYER_DELETE:
		// drop message if layer didn't exist
		if(!_server->session().deleteLayer(msg.cast<LayerDelete>().id()))
			return;
		break;
	case MSG_LAYER_ACL:
		// drop message if layer didn't exist
		if(!_server->session().updateLayerAcl(msg.cast<LayerACL>()))
			return;
		break;
	case MSG_ANNOTATION_CREATE:
		_server->session().createAnnotation(msg.cast<AnnotationCreate>(), true);
		break;
	case MSG_ANNOTATION_DELETE:
		// drop message if annotation didn't exist
		if(!_server->session().deleteAnnotation(msg.cast<AnnotationDelete>().id()))
			return;
	case MSG_CHAT:
		// Chat is used also for operator commands
		if(_isOperator && handleOperatorCommand(msg.cast<Chat>().message()))
			return;
		break;

	default: break;
	}

	// Add to main command stream to be distributed to everyone
	_server->addToCommandStream(msg);
}

bool Client::isHoldLocked() const
{
	return _state == WAIT_FOR_SYNC;
}

bool Client::isDropLocked() const
{
	return _userLock || _server->session().locked;
}

bool Client::isLayerLocked(int layerid)
{
	const LayerState *layer = _server->session().getLayerById(layerid);
	return layer==0 || layer->locked || !(layer->exclusive.isEmpty() || layer->exclusive.contains(_id));
}

void Client::grantOp()
{
	_server->printDebug(QString("Granted operator privileges to user #%1 (%2)").arg(_id).arg(_username));
	_isOperator = true;
	sendUpdatedAttrs();
}

void Client::deOp()
{
	_server->printDebug(QString("Revoked operator privileges from user #%1 (%2)").arg(_id).arg(_username));
	_isOperator = false;
	sendUpdatedAttrs();
}

void Client::lockUser()
{
	_server->printDebug(QString("Locked user #%1 (%2)").arg(_id).arg(_username));
	_userLock = true;
	sendUpdatedAttrs();
}

void Client::unlockUser()
{
	_server->printDebug(QString("Unlocked user #%1 (%2)").arg(_id).arg(_username));
	_userLock = false;
	sendUpdatedAttrs();
}

void Client::kick(int kickedBy)
{
	_server->printDebug(QString("User #%1 (%2) kicked by #%3").arg(_id).arg(_username).arg(kickedBy));
	_socket->close();
}

void Client::sendUpdatedAttrs()
{
	// Note. These changes are applied immediately on the server, but may take some
	// time to reach the clients. This doesn't matter much though, since locks and operator
	// privileges are enforced by the server only.
	_server->addToCommandStream(MessagePtr(new protocol::UserAttr(_id, _userLock, _isOperator)));
}

void Client::enqueueHeldCommands()
{
	if(isHoldLocked())
		return;

	foreach(protocol::MessagePtr msg, _holdqueue)
		handleSessionMessage(msg);
	_holdqueue.clear();
}

void Client::snapshotNowAvailable()
{
	if(_state == WAIT_FOR_SYNC) {
		_state = IN_SESSION;
		_streampointer = _server->mainstream().snapshotPointIndex();
		_substreampointer = 0;
		sendAvailableCommands();
		enqueueHeldCommands();
	}
}

/**
 * @brief Handle the login state
 *
 * The login process goes like this:
 * Server sends "DRAWPILE <version> [PASS]"
 * If password is required (PASS):
 * 1. Client responds with the password
 * Server sends BADPASS and disconnects or OK
 *
 * The client responds to OK (or initial hello) with "HOST ***" or "JOIN ***"
 * Server responds with BADNAME, NOSESSION, CLOSED or OK <userid>
 *
 * @param loginmsg login message
 */
void Client::handleLoginMessage(const protocol::Login &loginmsg)
{
	QString msg = loginmsg.message();
	QByteArray errormsg = "WHAT?";

	// TODO password protection
	try {
		switch(_substate) {
		case 0: /* expecting a password */
			// TODO
			break;
		case 1: /* expecting HOST or JOIN */
			if(msg.startsWith("HOST ")) {
				handleHostSession(msg);
				return;
			} else if(msg.startsWith("JOIN ")) {
				handleJoinSession(msg);
				return;
			}
		}
	} catch(const ProtocolViolation &pv) {
		errormsg = pv.what();
	}

	// Unexpected input
	_server->printError(QString("Error (%1) during login from %2").arg(QString(errormsg)).arg(peerAddress().toString()));
	_msgqueue->send(MessagePtr(new protocol::Login(errormsg)));
	_msgqueue->closeWhenReady();
}

void Client::handleHostSession(const QString &msg)
{
	// Cannot host if session has already started
	if(_server->isSessionStarted())
		throw ProtocolViolation("CLOSED");

	// Parse and validate command
	// Expected form is "HOST <version> <userid> <username>"
	QStringList tokens = msg.split(' ', QString::SkipEmptyParts);
	if(tokens.length() < 4)
		throw ProtocolViolation("WHAT?");

	bool ok;
	int minorVersion = tokens[1].toInt(&ok);
	if(!ok)
		throw ProtocolViolation("WHAT?");

	int userid = tokens[2].toInt(&ok);
	if(!ok || userid<1 || userid>255)
		throw ProtocolViolation("WHAT?");

	QString username = QStringList(tokens.mid(3)).join(' ');
	if(!validateUsername(username))
		throw ProtocolViolation("BADNAME");

	_id = userid;
	_username = username;
	_server->session().minorVersion = minorVersion;

	// Reserve ID
	_server->session().userids.reserve(_id);

	emit loggedin(this);

	_server->printDebug(QString("User %1 hosts the session").arg(_id));

	_msgqueue->send(MessagePtr(new protocol::Login(QString("OK %1").arg(_id))));

	// Initial state for host is always WAIT_FOR_SYNC, because the server
	// is not yet in sync with the user!
	_state = WAIT_FOR_SYNC;

	// Send request for initial state
	_server->startSession();
	requestSnapshot();
	_server->addToCommandStream(MessagePtr(new protocol::UserJoin(_id, _username)));
	grantOp();
}

void Client::handleJoinSession(const QString &msg)
{
	// Cannot join a session that hasn't started yet
	if(!_server->isSessionStarted())
		throw ProtocolViolation("NOSESSION");

	// Session may be closed or full
	if(_server->session().closed || _server->userCount() >= _server->session().maxusers)
		throw ProtocolViolation("CLOSED");

	// Parse and validate command
	// Expected form is "JOIN <username>"
	QString username = msg.mid(msg.indexOf(' ') + 1).trimmed();
	if(!validateUsername(username))
		throw ProtocolViolation("BADNAME");

	_username = username;

	// Assign ID
	_id = _server->session().userids.takeNext();
	if(_id<1) {
		// Out of space!
		throw ProtocolViolation("CLOSED");
	}

	emit loggedin(this);
	_msgqueue->send(MessagePtr(new protocol::Login(QString("OK %1").arg(_id))));

	_state = _server->mainstream().hasSnapshot() ? IN_SESSION : WAIT_FOR_SYNC;
	if(_state == IN_SESSION) {
		_streampointer = _server->mainstream().snapshotPointIndex();
		_substreampointer = 0;
	}

	_server->printDebug(QString("User %1 joined, wait_for_sync is=%2").arg(_id).arg(_state==WAIT_FOR_SYNC));

	_server->addToCommandStream(MessagePtr(new protocol::UserJoin(_id, _username)));

	// Give op to this user if it is the only one here
	if(_server->userCount() == 1)
		grantOp();

}

bool Client::validateUsername(const QString &username)
{
	if(username.isEmpty())
		return false;

	// TODO check for duplicates
	return true;
}

/**
 * @brief Handle IRC style operator commands
 * @param cmd
 * @return true if command was accepted
 */
bool Client::handleOperatorCommand(const QString &cmd)
{
	// Operator command must start with a slash
	if(cmd.length() == 0 || cmd.at(0) != '/')
		return false;

	// Supported commands
	/*
	 * /lock <user>   - lock the given user
	 * /unlock <user> - unlock the given user
	 * /kick <user>   - kick the user off the server
	 * /lock          - lock the whole board
	 * /unlock        - unlock the board
	 * /close         - prevent further logins
	 * /open          - reallow logins
	 * /title <tite>  - change session title (for those who like IRC commands)
	 */
	QStringList tokens = cmd.split(' ', QString::SkipEmptyParts);
	if(tokens[0] == "/lock" && tokens.count()==2) {
		bool ok;
		Client *c = _server->getClientById(tokens[1].toInt(&ok));
		if(c && ok) {
			c->lockUser();
			return true;
		}
	} else if(tokens[0] == "/unlock" && tokens.count()==2) {
		bool ok;
		Client *c = _server->getClientById(tokens[1].toInt(&ok));
		if(c && ok) {
			c->unlockUser();
			return true;
		}
	} else if(tokens[0] == "/kick" && tokens.count()==2) {
		bool ok;
		Client *c = _server->getClientById(tokens[1].toInt(&ok));
		if(c && ok) {
			c->kick(_id); // TODO inform of the reason
			return true;
		}
	} else if(tokens[0] == "/lock" && tokens.count()==1) {
		_server->session().locked = true;
		_server->addToCommandStream(_server->session().sessionConf());
		return true;
	} else if(tokens[0] == "/unlock" && tokens.count()==1) {
		_server->session().locked = false;
		_server->addToCommandStream(_server->session().sessionConf());
		return true;
	} else if(tokens[0] == "/close" && tokens.count()==1) {
		_server->session().closed = true;
		_server->addToCommandStream(_server->session().sessionConf());
		return true;
	} else if(tokens[0] == "/open" && tokens.count()==1) {
		_server->session().closed = false;
		_server->addToCommandStream(_server->session().sessionConf());
		return true;
	} else if(tokens[0] == "/title" && tokens.count()>1) {
		QString title = QStringList(tokens.mid(1)).join(' ');
		_server->addToCommandStream(protocol::MessagePtr(new protocol::SessionTitle(title)));
		return true;
	}

	return false;
}

}

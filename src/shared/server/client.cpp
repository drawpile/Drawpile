/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include "client.h"
#include "session.h"
#include "opcommands.h"

#include "../net/messagequeue.h"
#include "../net/annotation.h"
#include "../net/image.h"
#include "../net/layer.h"
#include "../net/login.h"
#include "../net/meta.h"
#include "../net/flow.h"
#include "../net/pen.h"
#include "../net/snapshot.h"
#include "../net/undo.h"
#include "../util/logger.h"

#include <QTcpSocket>
#include <QSslSocket>
#include <QStringList>

namespace server {

using protocol::MessagePtr;

Client::Client(QTcpSocket *socket, QObject *parent)
	: QObject(parent),
	  _session(0),
	  _socket(socket),
	  _state(LOGIN), _substate(0),
	  _awaiting_snapshot(false),
	  _uploading_snapshot(false),
	  _substreampointer(-1),
	  _id(0),
	  _isOperator(false),
	  _isModerator(false),
	  _isAuth(false),
	  _userLock(false),
	  _barrierlock(BARRIER_NOTLOCKED)
{
	_msgqueue = new protocol::MessageQueue(socket, this);

	_socket->setParent(this);

	connect(_socket, SIGNAL(disconnected()), this, SLOT(socketDisconnect()));
	connect(_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
	connect(_msgqueue, SIGNAL(messageAvailable()), this, SLOT(receiveMessages()));
	connect(_msgqueue, SIGNAL(snapshotAvailable()), this, SLOT(receiveSnapshot()));
	connect(_msgqueue, SIGNAL(badData(int,int)), this, SLOT(gotBadData(int,int)));
}

Client::~Client()
{
}

QString Client::toLogString() const {
	if(_session) 
		return QStringLiteral("#%1 [%2] %3@%4:").arg(QString::number(id()), peerAddress().toString(), username(), _session->id());
	else
		return QStringLiteral("[%1] %2@(lobby):").arg(peerAddress().toString(), username());
}

void Client::setSession(SessionState *session)
{
	_session = session;
	setParent(session);

	if(_session->mainstream().hasSnapshot()) {
		_state = IN_SESSION;
		_streampointer = _session->mainstream().snapshotPointIndex();
		_substreampointer = 0;
	} else {
		_state = WAIT_FOR_SYNC;
		_streampointer = 0;
	}
}

void Client::setConnectionTimeout(int timeout)
{
	_msgqueue->setIdleTimeout(timeout);
}

#ifndef NDEBUG
void Client::setRandomLag(uint lag)
{
	_msgqueue->setRandomLag(lag);
}
#endif

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
		const protocol::MessagePtr sptr = _session->mainstream().at(_streampointer);
		Q_ASSERT(sptr->type() == protocol::MSG_SNAPSHOTPOINT);
		const protocol::SnapshotPoint &sp = sptr.cast<const protocol::SnapshotPoint>();

		if(_substreampointer == 0) {
			// User is in the beginning of a stream, send stream position message
			uint streamlen = 0;
			for(int i=0;i<sp.substream().length();++i)
				streamlen += sp.substream().at(i)->length();
			for(int i=_streampointer+1;i<_session->mainstream().end();++i)
				streamlen += _session->mainstream().at(i)->length();

			_msgqueue->send(MessagePtr(new protocol::StreamPos(streamlen)));
		}
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
		while(_streampointer < _session->mainstream().end()) {
			MessagePtr msg = _session->mainstream().at(_streampointer++);
			if(msg->type() != protocol::MSG_SNAPSHOTPOINT)
				_msgqueue->send(msg);
		}
	}
}

void Client::sendDirectMessage(protocol::MessagePtr msg)
{
	_msgqueue->send(msg);
}

void Client::sendSystemChat(const QString &message)
{
	_msgqueue->send(MessagePtr(new protocol::Chat(0, message, false, false)));
}

void Client::receiveMessages()
{
	while(_msgqueue->isPending()) {
		MessagePtr msg = _msgqueue->getPending();

		if(_state == LOGIN) {
			if(msg->type() == protocol::MSG_LOGIN)
				emit loginMessage(msg);
			else
				logger::notice() << this << "Got non-login message (type=" << msg->type() << ") in login state";
		} else {
			handleSessionMessage(msg);
		}
	}
}

void Client::handleSnapshotStart(const protocol::SnapshotMode &msg)
{
	if(!_awaiting_snapshot) {
		logger::notice() << this << "Got unexpected snapshot message from";
		return;
	}

	_awaiting_snapshot = false;

	if(msg.mode() != protocol::SnapshotMode::ACK) {
		logger::notice() << this << "Got unexpected snapshot message. Expected ACK, got mode" << msg.mode();
		_session->abandonSnapshotPoint();
		return;
	} else {
		_session->snapshotSyncStarted();
		_uploading_snapshot = true;
	}
}

void Client::receiveSnapshot()
{
	if(!_uploading_snapshot) {
		logger::notice() << this << "Received snapshot data when not expecting it!";
		disconnectError("Didn't expect snapshot data");
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
		case MSG_DISCONNECT:
			continue;
		default: break;
		}

		// Add message
		if(_session->addToSnapshotStream(msg)) {
			logger::debug() << this << "Finished getting snapshot.";
			_uploading_snapshot = false;

			// If this was the hosting user, graduate to full session status
			// The server now needs to be brought up to date with the initial uploaded state
			if(_state == WAIT_FOR_SYNC) {
				logger::debug() << this << "Session sync complete.";
				_state = IN_SESSION;
				_streampointer = _session->mainstream().snapshotPointIndex();
				_session->syncInitialState(_session->mainstream().snapshotPoint().cast<protocol::SnapshotPoint>().substream());
				enqueueHeldCommands();
				sendAvailableCommands();
			}

			if(_msgqueue->isPendingSnapshot()) {
				logger::notice() << this << "Received too much snapshot data!";
				disconnectError("too much snapshot data");
			}
			break;
		}
	}
}

void Client::gotBadData(int len, int type)
{
	logger::notice() << this << "Received unknown message type #" << type << "of length" << len;
	_socket->abort();
}

void Client::socketError(QAbstractSocket::SocketError error)
{
	if(error != QAbstractSocket::RemoteHostClosedError) {
		logger::error() << this << "Socket error" << _socket->errorString();
		_socket->abort();
	}
}

void Client::socketDisconnect()
{
	emit disconnected(this);
}

bool Client::isDownloadingLatestSnapshot() const
{
	if(!_session)
		return false;

	return _substreampointer >= 0 && _streampointer == _session->mainstream().snapshotPointIndex();
}

bool Client::isUploadingSnapshot() const
{
	return _uploading_snapshot;
}

void Client::requestSnapshot(bool forcenew)
{
	Q_ASSERT(_state != LOGIN);

#ifndef NDEBUG
	sendSystemChat("(Requesting snapshot...)");
#endif

	sendDirectMessage(MessagePtr(new protocol::SnapshotMode(forcenew ? protocol::SnapshotMode::REQUEST_NEW : protocol::SnapshotMode::REQUEST)));
	_awaiting_snapshot = true;
	_session->addSnapshotPoint();
	logger::debug() << this << "Created a new snapshot point and requested data from";
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
		logger::notice() << this << "Got server-to-user only command" << msg->type();
		return;
	case MSG_DISCONNECT:
		// we don't do anything with disconnect notifications from the client
		return;
	default: break;
	}

	if(msg->isOpCommand() && !isOperator()) {
		logger::notice() << this << "Tried to use operator command" << msg->type();
		return;
	}

	// Layer control locking
	if(_session->isLayerControlLocked() && !isOperator()) {
		switch(msg->type()) {
		using namespace protocol;
		case MSG_LAYER_CREATE:
		case MSG_LAYER_ATTR:
		case MSG_LAYER_ORDER:
		case MSG_LAYER_RETITLE:
		case MSG_LAYER_DELETE:
			logger::debug() << this << "Non-operator use of layer control command";
			return;
		default: break;
		}
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
			if(isLayerLocked(_session->drawingContext(_id).currentLayer))
				return;
			break;
		case MSG_LAYER_ATTR:
			if(!isOperator() && isLayerLocked(msg.cast<LayerAttributes>().id()))
				return;
			break;
		case MSG_LAYER_RETITLE:
			if(!isOperator() && isLayerLocked(msg.cast<LayerRetitle>().id()))
				return;
			break;
		case MSG_LAYER_DELETE: {
			const LayerDelete &ld = msg.cast<LayerDelete>();
			if(!isOperator() && isLayerLocked(ld.id()))
				return;

			// When merging, the layer below must be unlocked (and exist!)
			if(ld.merge()) {
				const LayerState *layer = _session->getLayerBelowId(ld.id());
				if(!layer || isLayerLocked(layer->id))
					return;
			}

			} break;
		case MSG_PUTIMAGE:
			if(isLayerLocked(msg.cast<PutImage>().layer()))
				return;
			break;
		case MSG_FILLRECT:
			if(isLayerLocked(msg.cast<FillRect>().layer()))
				return;
			break;
		default: /* other types are always allowed */ break;
		}
	}

	// Make sure the origin user ID is set
	msg->setContextId(_id);

	// Track state and special commands
	switch(msg->type()) {
	using namespace protocol;
	case MSG_TOOLCHANGE:
		_session->drawingContextToolChange(msg.cast<ToolChange>());
		break;
	case MSG_PEN_MOVE:
		_session->drawingContextPenDown(msg.cast<PenMove>());
		break;
	case MSG_PEN_UP:
		_session->drawingContextPenUp(msg.cast<PenUp>());
		if(_barrierlock == BARRIER_WAIT) {
			_barrierlock = BARRIER_LOCKED;
			emit barrierLocked();
		}
		break;
	case MSG_LAYER_CREATE:
		// drop message if it didn't pass validation
		if(!_session->createLayer(msg.cast<LayerCreate>(), true))
			return;
		break;
	case MSG_LAYER_ORDER:
		_session->reorderLayers(msg.cast<LayerOrder>());
		break;
	case MSG_LAYER_DELETE:
		// drop message if layer didn't exist
		if(!_session->deleteLayer(msg.cast<LayerDelete>().id()))
			return;
		break;
	case MSG_LAYER_ACL:
		// drop message if layer didn't exist
		if(!_session->updateLayerAcl(msg.cast<LayerACL>()))
			return;
		break;
	case MSG_ANNOTATION_CREATE:
		// drop message if it didn't pass validation
		if(!msg.cast<AnnotationCreate>().isValidId())
			return;
		break;
	case MSG_UNDOPOINT:
		// keep track of undo points
		handleUndoPoint();
		break;
	case MSG_UNDO:
		// validate undo command
		if(!handleUndoCommand(msg.cast<Undo>()))
			return;
		break;
	case MSG_CHAT:
		// Chat is used also for operator commands
		if(msg->isOpCommand()) {
			handleSessionOperatorCommand(this, msg.cast<Chat>().message());
			return;
		}

		// Normal chat messages are not included in the session history,
		// except when preservechat setting is set
		if(!msg.cast<Chat>().isAnnouncement() && !_session->isChatPreserved()) {
			for(Client *c : _session->clients())
				c->sendDirectMessage(msg);
			return;
		}

		break;
	case MSG_SNAPSHOT:
		handleSnapshotStart(msg.cast<SnapshotMode>());
		return;
	case MSG_SESSION_TITLE:
		// Keep track of title changes
		_session->setTitle(msg.cast<SessionTitle>().title());
		break;
	default: break;
	}

	// Add to main command stream to be distributed to everyone
	_session->addToCommandStream(msg);
}

bool Client::isHoldLocked() const
{
	return _state == WAIT_FOR_SYNC || isBarrierLocked();
}

bool Client::isDropLocked() const
{
	return _userLock || _session->isLocked();
}

bool Client::isLayerLocked(int layerid)
{
	const LayerState *layer = _session->getLayerById(layerid);
	return layer==0 || layer->locked || !(layer->exclusive.isEmpty() || layer->exclusive.contains(_id));
}

void Client::grantOp()
{
	_isOperator = true;
	sendUpdatedAttrs();
}

void Client::deOp()
{
	_isOperator = false;
	sendUpdatedAttrs();
}

void Client::lockUser()
{
	_userLock = true;
	sendUpdatedAttrs();
}

void Client::unlockUser()
{
	_userLock = false;
	sendUpdatedAttrs();
}

void Client::barrierLock()
{
	if(_barrierlock != BARRIER_NOTLOCKED) {
		logger::error() << this << "Tried to double-barrier lock";
		return;
	}

	if(_session->drawingContext(_id).penup) {
		_barrierlock = BARRIER_LOCKED;
		emit barrierLocked();
	} else {
		_barrierlock = BARRIER_WAIT;
	}
}

void Client::barrierUnlock()
{
	_barrierlock = BARRIER_NOTLOCKED;
	enqueueHeldCommands();
}

void Client::disconnectKick(const QString &kickedBy)
{
	logger::info() << this << "Kicked by" << kickedBy;
	_msgqueue->sendDisconnect(protocol::Disconnect::KICK, kickedBy);
}

void Client::disconnectError(const QString &message)
{
	logger::info() << this << "Disconnecting due to error:" << message;
	_msgqueue->sendDisconnect(protocol::Disconnect::ERROR, message);
}

void Client::disconnectShutdown()
{
	_msgqueue->sendDisconnect(protocol::Disconnect::SHUTDOWN, QString());
}

void Client::sendUpdatedAttrs()
{
	// Note. These changes are applied immediately on the server, but may take some
	// time to reach the clients. This doesn't matter much though, since locks and operator
	// privileges are enforced by the server only.
	_session->addToCommandStream(MessagePtr(new protocol::UserAttr(
		id(),
		isUserLocked(),
		isOperator(),
		isModerator(),
		isAuthenticated()
	)));
}

void Client::enqueueHeldCommands()
{
	if(isHoldLocked())
		return;

	while(!_holdqueue.isEmpty())
		handleSessionMessage(_holdqueue.takeFirst());
}

void Client::snapshotNowAvailable()
{
	if(_state == WAIT_FOR_SYNC) {
		_state = IN_SESSION;
		_streampointer = _session->mainstream().snapshotPointIndex();
		_substreampointer = 0;
		sendAvailableCommands();
		enqueueHeldCommands();
	}
}

void Client::handleUndoPoint()
{
	// New undo point. This branches the undo history. Since we store the
	// commands in a linear sequence, this branching is represented by marking
	// the unreachable UndoPoints as GONE.
	int pos = _session->mainstream().end() - 1;
	int limit = protocol::UNDO_HISTORY_LIMIT;
	while(_session->mainstream().isValidIndex(pos) && limit>0) {
		protocol::MessagePtr msg = _session->mainstream().at(pos);
		if(msg->type() == protocol::MSG_UNDOPOINT) {
			--limit;
			if(msg->contextId() == _id) {
				// optimization: we can stop searching after finding the first GONE point
				if(msg->undoState() == protocol::GONE)
					break;
				else if(msg->undoState() == protocol::UNDONE)
					msg->setUndoState(protocol::GONE);
			}
		}
		--pos;
	}
}

bool Client::handleUndoCommand(protocol::Undo &undo)
{
	// First check if user context override is used
	if(undo.overrideId()) {
		undo.setContextId(undo.overrideId());
	}

	// Undo history is limited to last UNDO_HISTORY_LIMIT undopoints
	// or the latest snapshot point, whichever comes first
	// Clients usually store more than that, but to ensure a consistent
	// experience, we enforce the limit here.

	if(undo.points()>0) {
		// Undo mode: iterator towards the beginning of the history,
		// marking not-undone UndoPoints as undone.
		int limit = protocol::UNDO_HISTORY_LIMIT;
		int pos = _session->mainstream().end()-1;
		int points = 0;
		while(limit>0 && points<undo.points() && _session->mainstream().isValidIndex(pos)) {
			MessagePtr msg = _session->mainstream().at(pos);

			if(msg->type() == protocol::MSG_SNAPSHOTPOINT)
				break;

			if(msg->type() == protocol::MSG_UNDOPOINT) {
				--limit;
				if(msg->contextId() == undo.contextId() && msg->undoState()==protocol::DONE) {
					msg->setUndoState(protocol::UNDONE);
					++points;
				}
			}
			--pos;
		}

		// Did we undo anything?
		if(points==0)
			return false;

		// Number of undoable actions may be less than expected, but undo what we got.
		undo.setPoints(points);
		return true;
	} else if(undo.points()<0) {
		// Redo mode: find the start of the latest undo sequence, then mark
		// (points) UndoPoints as redone.
		int redostart = _session->mainstream().end();
		int limit = protocol::UNDO_HISTORY_LIMIT;
		int pos = _session->mainstream().end();
		while(_session->mainstream().isValidIndex(--pos) && limit>0) {
			protocol::MessagePtr msg = _session->mainstream().at(pos);
			if(msg->type() == protocol::MSG_UNDOPOINT) {
				--limit;
				if(msg->contextId() == undo.contextId()) {
					if(msg->undoState() != protocol::DONE)
						redostart = pos;
					else
						break;
				}
			}
		}

		// There may be nothing to redo
		if(redostart == _session->mainstream().end())
			return false;

		pos = redostart;

		// Mark undone actions as done again
		int actions = -undo.points() + 1;
		int points = 0;
		while(pos < _session->mainstream().end()) {
			protocol::MessagePtr msg = _session->mainstream().at(pos);
			if(msg->contextId() == undo.contextId() && msg->type() == protocol::MSG_UNDOPOINT) {
				if(msg->undoState() == protocol::UNDONE) {
					if(--actions==0)
						break;

					msg->setUndoState(protocol::DONE);
					++points;
				}
			}
			++pos;
		}

		// Did we redo anything
		if(points==0)
			return false;

		undo.setPoints(-points);
		return true;
	} else {
		// points==0 is invalid
		return false;
	}
}

void Client::sendOpWhoList()
{
	const QList<Client*> &clients = _session->clients();
	QStringList msgs;
	foreach(const Client *c, clients) {
		QString flags;
		if(c->isOperator())
			flags = "@";
		if(c->isUserLocked())
			flags += "L";
		if(c->isHoldLocked())
			flags += "l";
		msgs << QString("#%1: %2 [%3]").arg(c->id()).arg(c->username(), flags);
	}
	for(const QString &m : msgs)
		sendSystemChat(m);
}

void Client::sendOpServerStatus()
{
	QStringList msgs;
	msgs << QString("Session #%1, up %2").arg(_session->id()).arg(_session->uptime());
	msgs << QString("Logged in users: %1 (max: %2)").arg(_session->userCount()).arg(_session->maxUsers());
	msgs << QString("Persistent session: %1").arg(_session->isPersistenceAllowed() ? (_session->isPersistent() ? "yes" : "no") : "not allowed");
	msgs << QString("Password protected: %1").arg(!_session->passwordHash().isEmpty() ? "yes" : "no");
	msgs << QString("History size: %1 Mb (limit: %2 Mb)")
			.arg(_session->mainstream().lengthInBytes() / qreal(1024*1024), 0, 'f', 2)
			.arg(_session->historyLimit() / qreal(1024*1024), 0, 'f', 2);
	msgs << QString("History indices: %1 -- %2").arg(_session->mainstream().offset()).arg(_session->mainstream().end());
	msgs << QString("Snapshot point exists: %1").arg(_session->mainstream().hasSnapshot() ? "yes" : "no");

	for(const QString &m : msgs)
		sendSystemChat(m);
}

bool Client::hasSslSupport() const
{
	return _socket->inherits("QSslSocket");
}

bool Client::isSecure() const
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(_socket);
	return socket && socket->isEncrypted();
}

void Client::startTls()
{
	QSslSocket *socket = qobject_cast<QSslSocket*>(_socket);
	Q_ASSERT(socket);
	socket->startServerEncryption();
}

}

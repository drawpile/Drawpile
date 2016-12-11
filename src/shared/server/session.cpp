/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

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

#include "session.h"
#include "client.h"
#include "../net/layer.h"
#include "../net/meta.h"
#include "../net/pen.h"
#include "../net/snapshot.h"
#include "../record/writer.h"
#include "../util/passwordhash.h"
#include "../util/filename.h"

#include "config.h"


#include <QTimer>

namespace server {

using protocol::MessagePtr;

SessionState::SessionState(const SessionId &id, int minorVersion, const QString &founder, QObject *parent)
	: QObject(parent),
	_recorder(0),
	_lastUserId(0),
	_startTime(QDateTime::currentDateTime()), _lastEventTime(QDateTime::currentDateTime()),
	_id(id), _minorVersion(minorVersion), _maxusers(255), _historylimit(0),
	_founder(founder),
	_locked(false), _layerctrllocked(true), _putimagelocked(false), _closed(false),
	_lockdefault(false), _allowPersistent(false), _persistent(false), _hibernatable(false),
	_preservechat(false), m_privateUserList(false)
{
}

QString SessionState::toLogString() const {
	return QStringLiteral("Session %1:").arg(id());
}

void SessionState::assignId(Client *user)
{
	int loops=0;
	while(++loops<256) {
		++_lastUserId;
		if(_lastUserId>255)
			_lastUserId=1;

		bool isFree = true;
		for(const Client *c : _clients) {
			if(c->id() == _lastUserId) {
				isFree = false;
				break;
			}
		}

		if(isFree) {
			user->setId(_lastUserId);
			break;
		}

	}
	// shouldn't happen: we don't let users in if the session is full
	Q_ASSERT(user->id() != 0);
}

void SessionState::joinUser(Client *user, bool host)
{
	user->setSession(this);
	_clients.append(user);

	connect(user, SIGNAL(barrierLocked()), this, SLOT(userBarrierLocked()));
	connect(user, SIGNAL(disconnected(Client*)), this, SLOT(removeUser(Client*)));
	connect(this, SIGNAL(newCommandsAvailable()), user, SLOT(sendAvailableCommands()));

	MessagePtr joinmsg(new protocol::UserJoin(user->id(), user->username()));

	if(host) {
		Q_ASSERT(!_mainstream.hasSnapshot());
		// Send login message directly, since the distributable login
		// notification will be part of the initial snapshot.
		user->sendDirectMessage(joinmsg);

		// Request initial session content. This creates a snapshot point for the session
		// and upgrades the client to full session state
		user->requestSnapshot(false);
	} else {
		connect(this, SIGNAL(snapshotCreated()), user, SLOT(snapshotNowAvailable()));
		addToCommandStream(joinmsg);
	}

	// Welcome the user (this typically contains a link to the server's terms-of-use page or such)
	if(!welcomeMessage().isEmpty()) {
		user->sendDirectMessage(MessagePtr(new protocol::Chat(0, welcomeMessage(), false, false)));
	}

	// Give op to this user if it is the only one here
	if(userCount() == 1)
		user->grantOp();
	else if(_lockdefault)
		user->lockUser();
	else
		user->sendUpdatedAttrs(); // make sure up to date attrs are always sent

	logger::info() << user << "Joined session";
	emit userConnected(this, user);
}

void SessionState::removeUser(Client *user)
{
	Q_ASSERT(_clients.contains(user));

	if(user->isUploadingSnapshot()) {
		abandonSnapshotPoint();
	}

	_clients.removeOne(user);

	if(!_drawingctx[user->id()].penup)
		addToCommandStream(MessagePtr(new protocol::PenUp(user->id())));
	addToCommandStream(MessagePtr(new protocol::UserLeave(user->id())));

	// Make sure there is at least one operator in the session
	bool hasOp=false;
	foreach(const Client *c, _clients) {
		if(c->isOperator()) {
			hasOp=true;
			break;
		}
	}

	if(!hasOp && !_clients.isEmpty())
		_clients.first()->grantOp();

	// Reopen the session when the last user leaves
	if(_clients.isEmpty()) {
		setClosed(false);
	}

	user->deleteLater();

	_lastEventTime = QDateTime::currentDateTime();

	emit userDisconnected(this);
}

Client *SessionState::getClientById(int id)
{
	foreach(Client *c, _clients) {
		if(c->id() == id)
			return c;
	}
	return nullptr;
}

Client *SessionState::getClientByUsername(const QString &username)
{
	foreach(Client *c, _clients) {
		if(c->username().compare(username, Qt::CaseInsensitive)==0)
			return c;
	}
	return nullptr;
}

void SessionState::setClosed(bool closed)
{
	if(_closed != closed) {
		_closed = closed;
		addToCommandStream(sessionConf());
		emit sessionAttributeChanged(this);
	}
}

void SessionState::setLocked(bool locked)
{
	if(_locked != locked) {
		_locked = locked;
		addToCommandStream(sessionConf());
	}
}

void SessionState::setLayerControlLocked(bool locked)
{
	if(_layerctrllocked != locked) {
		_layerctrllocked = locked;
		addToCommandStream(sessionConf());
	}
}

void SessionState::setPutImageLocked(bool locked)
{
	if(_putimagelocked != locked) {
		_putimagelocked = locked;
		addToCommandStream(sessionConf());
	}
}

void SessionState::setUsersLockedByDefault(bool locked)
{
	if(_lockdefault != locked) {
		_lockdefault = locked;
		addToCommandStream(sessionConf());
	}
}

void SessionState::setMaxUsers(int maxusers)
{
	Q_ASSERT(maxusers>0);
	if(_maxusers != maxusers) {
		_maxusers = maxusers;
		addToCommandStream(sessionConf());
		emit sessionAttributeChanged(this);
	}
}

void SessionState::setPersistent(bool persistent)
{
	if(!_allowPersistent && persistent)
		return;

	if(_persistent != persistent) {
		_persistent = persistent;
		addToCommandStream(sessionConf());
		emit sessionAttributeChanged(this);
	}
}

void SessionState::setPassword(const QString &password)
{
	if(password.isEmpty())
		_passwordhash = QByteArray();
	else
		_passwordhash = passwordhash::hash(password);

	emit sessionAttributeChanged(this);
}

void SessionState::setPasswordHash(const QByteArray &passwordhash)
{
	_passwordhash = passwordhash;
	emit sessionAttributeChanged(this);
}

void SessionState::setTitle(const QString &title)
{
	if(_title != title) {
		_title = title;
		emit sessionAttributeChanged(this);
	}
}

void SessionState::setPreserveChat(bool preserve)
{
	if(_preservechat != preserve) {
		_preservechat = preserve;
		addToCommandStream(sessionConf());
		emit sessionAttributeChanged(this);
	}
}

void SessionState::addToCommandStream(protocol::MessagePtr msg)
{
	if(_historylimit>0 && _mainstream.lengthInBytes() > _historylimit) {
		// Cannot cleanup until previous snapshot is finished
		if(!_mainstream.hasSnapshot() || _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>().isComplete()) {

			// An operator must be present, otherwise the session will end due to a lack
			// of snapshot point
			bool hasOp = false;
			foreach(Client *c, _clients) {
				if(c->isOperator()) {
					hasOp = true;
					break;
				}
			}

			if(hasOp) {
				int streampos = _mainstream.end();
				foreach(const Client *c, _clients) {
					streampos = qMin(streampos, c->streampointer());
				}

				uint oldsize = _mainstream.lengthInBytes();
				_mainstream.hardCleanup(0, streampos);
				uint difference = oldsize - _mainstream.lengthInBytes();
				logger::debug() << this << QString("History cleanup. Removed %1 Mb.").arg(difference / qreal(1024*1024), 0, 'f', 2);

				// TODO perhaps this can be deferred? Doing it now will cut off undo history,
				// but deferring new snapshot generation risks leaving the session in unjoinable state.
				startSnapshotSync();
			}
		}
	}

	_mainstream.append(msg);
	if(_recorder)
		_recorder->recordMessage(msg);
	_lastEventTime = QDateTime::currentDateTime();
	emit newCommandsAvailable();
}

void SessionState::addSnapshotPoint()
{
	// Create new snapshot point
	_mainstream.addSnapshotPoint();

	// Add current session state to snapshot point
	addToCommandStream(sessionConf());

	foreach(const Client *c, _clients) {
		if(c->id()>0) {
			addToSnapshotStream(protocol::MessagePtr(new protocol::UserJoin(c->id(), c->username())));
			addToSnapshotStream(protocol::MessagePtr(new protocol::UserAttr(c->id(), c->isUserLocked(), c->isOperator(), c->isModerator(), c->isAuthenticated())));
		}
	}

	emit snapshotCreated();
}

bool SessionState::addToSnapshotStream(protocol::MessagePtr msg)
{
	if(!_mainstream.hasSnapshot()) {
		logger::error() << this << "Tried to add a snapshot command, but there is no snapshot point!";
		return true;
	}
	protocol::SnapshotPoint &sp = _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>();
	if(sp.isComplete()) {
		logger::error() << this << "Tried to add a snapshot command, but the snapshot point is already complete!";
		return true;
	}

	sp.append(msg);

	if(sp.isComplete()) {
		// Add latest layer ACL status to snapshot stream
		foreach(const LayerState &layer, _layers) {
			sp.append(MessagePtr(new protocol::LayerACL(0, layer.id, layer.locked, layer.exclusive)));
		}

		// Messages older than the snapshot point can now be removed
		cleanupCommandStream();

		// Start a new recording using the fresh snapshot point
		if(!_recordingFile.isEmpty()) {
			if(_splitRecording || !_recorder) {
				logger::debug() << "Snapshotted: restarting recording" << _recordingFile;
				stopRecording();
				startRecording(sp.substream());
			}

		}
	}

	emit newCommandsAvailable();

	return sp.isComplete();
}

void SessionState::abandonSnapshotPoint()
{
	logger::info() << this << "Abandoning snapshot point" << _mainstream.snapshotPointIndex();

	if(!_mainstream.hasSnapshot()) {
		logger::error() << this << "Tried to abandon a snapshot point that doesn't exist!";
		return;
	}
	const protocol::SnapshotPoint &sp = _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>();
	if(sp.isComplete()) {
		logger::error() << this << "Tried to abandon a complete snapshot point!";
		return;
	}

	foreach(Client *c, _clients) {
		if(c->isDownloadingLatestSnapshot()) {
			c->disconnectError("Session snapshot aborted");
		}
	}

	_mainstream.abandonSnapshotPoint();

	// Make sure no-one remains locked
	foreach(Client *c, _clients)
		c->barrierUnlock();

	logger::debug() << this << "Snapshot point rolled back to" << _mainstream.snapshotPointIndex();
}

void SessionState::cleanupCommandStream()
{
	int removed = _mainstream.cleanup();
	logger::debug() << this << "Cleaned up" << removed << "messages from the command stream.";
}

void SessionState::startSnapshotSync()
{
	logger::debug() << this << "Starting snapshot sync!";

	// Barrier lock all clients
	foreach(Client *c, _clients)
		c->barrierLock();
}

void SessionState::snapshotSyncStarted()
{
	logger::debug() << this << "Snapshot sync started!";

	// Lift barrier lock
	foreach(Client *c, _clients)
		c->barrierUnlock();
}

void SessionState::userBarrierLocked()
{
	// Count locked users
	int locked=0;
	foreach(const Client *c, _clients)
		if(c->isBarrierLocked())
			++locked;

	if(locked == _clients.count()) {
		// All locked, we can now send the snapshot sync request
		foreach(Client *c, _clients) {
			if(c->isOperator()) {
				c->requestSnapshot(true);
				break;
			}
		}
	}
}

void SessionState::syncInitialState(const QList<protocol::MessagePtr> &messages)
{
	using namespace protocol;
	foreach(MessagePtr msg, messages) {
		switch(msg->type()) {
		case MSG_TOOLCHANGE:
			drawingContextToolChange(msg.cast<ToolChange>());
			break;
		case MSG_PEN_MOVE:
			drawingContextPenDown(msg.cast<PenMove>());
			break;
		case MSG_PEN_UP:
			drawingContextPenUp(msg.cast<PenUp>());
			break;
		case MSG_LAYER_CREATE:
			// accept all layer IDs here
			createLayer(msg.cast<LayerCreate>(), false);
			break;
		case MSG_LAYER_ORDER:
			reorderLayers(msg.cast<LayerOrder>());
			break;
		case MSG_LAYER_DELETE:
			deleteLayer(msg.cast<LayerDelete>().id());
			break;
		case MSG_LAYER_ACL:
			updateLayerAcl(msg.cast<LayerACL>());
			break;
		case MSG_SESSION_CONFIG:
			setSessionConfig(msg.cast<SessionConf>());
			break;
		case MSG_SESSION_TITLE:
			setTitle(msg.cast<SessionTitle>().title());
			break;
		default: break;
		}
	}
}

const LayerState *SessionState::getLayerById(int id)
{
	foreach(const LayerState &l, _layers)
		if(l.id == id)
			return &l;
	return 0;
}

const LayerState *SessionState::getLayerBelowId(int id)
{
	for(int i=1;i<_layers.size();++i) {
		if(_layers.at(i).id == id) {
			return &_layers.at(i-1);
		}
	}
	return 0;
}

bool SessionState::createLayer(protocol::LayerCreate &cmd, bool validate)
{
	bool ok = cmd.isValidId();

	if(validate) {
		// check that layer doesn't exist already
		for(const LayerState &ls : _layers)
			if(ls.id == cmd.id())
				return false;
	}

	// Find referenced layer
	int pos = -1;
	if(cmd.source()) {
		for(int i=0;i<_layers.size();++i) {
			if(_layers.at(i).id == cmd.source()) {
				pos = i;
				break;
			}
		}
	}

	if((cmd.flags() & protocol::LayerCreate::FLAG_COPY)) {
		// Referenced layer must exist in COPY mode
		if(pos<0)
			return false;
	}

	if(!(cmd.flags() & protocol::LayerCreate::FLAG_INSERT)) { 
		// If not in INSERT mode, place layer on top of the stack
		pos = _layers.size() - 1;
	}

	if(ok || !validate)
		_layers.insert(pos+1, LayerState(cmd.id()));

	return ok;
}

void SessionState::reorderLayers(protocol::LayerOrder &cmd)
{
	// Sanitize the new order
	QList<uint16_t> currentOrder;
	currentOrder.reserve(_layers.size());
	for(const LayerState &ls : _layers)
		currentOrder.append(ls.id);

	QList<uint16_t> newOrder = cmd.sanitizedOrder(currentOrder);

	// Set new order
	QVector<LayerState> newlayers;
	QVector<LayerState> oldlayers = _layers;
	newlayers.reserve(_layers.size());

	for(uint16_t id : newOrder) {
		for(int i=0;i<oldlayers.size();++i) {
			if(oldlayers[i].id == id) {
				newlayers.append(oldlayers[i]);
				oldlayers.remove(i);
				break;
			}
		}
	}

	_layers = newlayers;
}

bool SessionState::deleteLayer(int id)
{
	for(int i=0;i<_layers.size();++i) {
		if(_layers[i].id == id) {
			_layers.remove(i);
			return true;
		}
	}
	return false;
}

bool SessionState::updateLayerAcl(const protocol::LayerACL &cmd)
{
	for(int i=0;i<_layers.size();++i) {
		if(_layers[i].id == cmd.id()) {
			_layers[i].locked = cmd.locked();
			_layers[i].exclusive = cmd.exclusive();
			return true;
		}
	}
	return false;
}

void SessionState::drawingContextToolChange(const protocol::ToolChange &cmd)
{
	_drawingctx[cmd.contextId()].currentLayer = cmd.layer();
}

void SessionState::drawingContextPenDown(const protocol::PenMove &cmd)
{
	_drawingctx[cmd.contextId()].penup = false;
}

void SessionState::drawingContextPenUp(const protocol::PenUp &cmd)
{
	_drawingctx[cmd.contextId()].penup = true;
}

/**
 * @brief Kick all users off the server
 */
void SessionState::kickAllUsers()
{
	foreach(Client *c, _clients)
		c->disconnectShutdown();
}

void SessionState::killSession()
{
	unlistAnnouncement();
	setClosed(true);
	setHibernatable(false);
	setPersistent(false);
	stopRecording();
	for(Client *c : _clients)
		c->disconnectKick(QString());
}

void SessionState::wall(const QString &message)
{
	for(Client *c : _clients) {
		c->sendSystemChat(message);
	}
}

void SessionState::sendLog(const QString &message)
{
	addToCommandStream(protocol::Chat::log(message));
}

void SessionState::setSessionConfig(protocol::SessionConf &cmd)
{
	_maxusers = cmd.maxUsers();
	_locked = cmd.isLocked();
	_closed = cmd.isClosed();
	_layerctrllocked = cmd.isLayerControlsLocked();
	_putimagelocked = cmd.isPutImageLocked();
	_lockdefault = cmd.isUsersLockedByDefault();
	_persistent = cmd.isPersistent() && isPersistenceAllowed();
}

protocol::MessagePtr SessionState::sessionConf() const
{
	return protocol::MessagePtr(new protocol::SessionConf(
		_maxusers,
		_locked,
		_closed,
		_layerctrllocked,
		_lockdefault,
		_persistent,
		_preservechat,
		_putimagelocked
	));
}

void SessionState::startRecording(const QList<protocol::MessagePtr> &snapshot)
{
	Q_ASSERT(_recorder==0);

	// Start recording
	QString filename = utils::makeFilenameUnique(_recordingFile, ".dprec");
	logger::info() << this << "Starting session recording" << filename;

	_recorder = new recording::Writer(filename, this);
	if(!_recorder->open()) {
		logger::error() << this << "Couldn't write session recording to" << filename << _recorder->errorString();
		delete _recorder;
		_recorder = 0;
		return;
	}

	_recorder->writeHeader();

	// Record snapshot and what is in the main stream
	foreach(protocol::MessagePtr msg, snapshot) {
		_recorder->recordMessage(msg);
	}

	for(int i=_mainstream.offset();i<_mainstream.end();++i)
		_recorder->recordMessage(_mainstream.at(i));
}

void SessionState::stopRecording()
{
	if(_recorder) {
		_recorder->close();
		delete _recorder;
		_recorder = 0;
	}
}

QString SessionState::uptime() const
{
	qint64 up = (QDateTime::currentMSecsSinceEpoch() - _startTime.toMSecsSinceEpoch()) / 1000;

	int days = up / (60*60*24);
	up -= days * (60*60*24);

	int hours = up / (60*60);
	up -= hours * (60*60);

	int minutes = up / 60;

	QString uptime;
	if(days==1)
		uptime = "one day, ";
	else if(days>1)
		uptime = QString::number(days) + " days, ";

	if(hours==1)
		uptime += "1 hour and ";
	else
		uptime += QString::number(hours) + " hours and ";

	if(minutes==1)
		uptime += "1 minute";
	else
		uptime += QString::number(minutes) + " minutes.";

	return uptime;
}

QStringList SessionState::userNames() const
{
	QStringList lst;
	for(const Client *c : _clients)
		lst << c->username();
	return lst;
}

void SessionState::makeAnnouncement(const QUrl &url)
{
	sessionlisting::Session s {
		QString(),
		0,
		id(),
		QStringLiteral("%1.%2").arg(DRAWPILE_PROTO_MAJOR_VERSION).arg(minorProtocolVersion()),
		title(),
		userCount(),
		passwordHash().isEmpty() && !m_privateUserList ? userNames() : QStringList(),
		!passwordHash().isEmpty(),
		false, // TODO: explicit NSFM tag
		founder(),
		sessionStartTime()
	};

	emit requestAnnouncement(url, s);
}

void SessionState::unlistAnnouncement()
{
	if(_publicListing.listingId>0) {
		emit requestUnlisting(_publicListing);
		_publicListing.listingId=0;
	}
}

}

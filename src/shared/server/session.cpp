/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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

#include "session.h"
#include "client.h"
#include "../net/annotation.h"
#include "../net/layer.h"
#include "../net/meta.h"
#include "../net/pen.h"
#include "../net/snapshot.h"

namespace server {

using protocol::MessagePtr;

SessionState::SessionState(int minorVersion, SharedLogger logger, QObject *parent)
	: QObject(parent),
	_logger(logger),
	_syncstate(NOT_SYNCING),
	_userids(255), _layerids(255), _annotationids(255),
	_minorVersion(minorVersion), _maxusers(255),
	_locked(false), _layerctrllocked(true), _closed(false),
	_lockdefault(false)
{ }

void SessionState::assignId(Client *user)
{
	user->setId(_userids.takeNext());
}

void SessionState::joinUser(Client *user, bool host)
{
	user->setSession(this);
	_clients.append(user);

	// Make sure the ID is reserved (hosting user gets to choose their own)
	_userids.reserve(user->id());

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
	}

	addToCommandStream(joinmsg);

	// Give op to this user if it is the only one here
	if(userCount() == 1)
		user->grantOp();
	else if(_lockdefault)
		user->lockUser();

	_logger->logDebug(QString("User %1 joined").arg(user->id()));
}

void SessionState::removeUser(Client *user)
{
	Q_ASSERT(_clients.contains(user));

	if(user->isUploadingSnapshot()) {
		abandonSnapshotPoint();
	}

	_clients.removeOne(user);
	_userids.release(user->id());

	if(_drawingctx[user->id()].penup)
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
		emit lastClientLeft();
	}

	user->deleteLater();
}

Client *SessionState::getClientById(int id)
{
	foreach(Client *c, _clients) {
		if(c->id() == id)
			return c;
	}
	return 0;
}

void SessionState::setClosed(bool closed)
{
	if(_closed != closed) {
		_closed = closed;
		addToCommandStream(sessionConf());
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
	_maxusers = maxusers;
}

void SessionState::addToCommandStream(protocol::MessagePtr msg)
{
	// TODO history size limit. Can clear up to lowest stream pointer index.
	_mainstream.append(msg);
	emit newCommandsAvailable();
}

void SessionState::addSnapshotPoint()
{
	// Create new snapshot point
	_mainstream.addSnapshotPoint();

	// Add user introductions to snapshot point
	foreach(const Client *c, _clients) {
		if(c->id()>0) {
			addToSnapshotStream(protocol::MessagePtr(new protocol::UserJoin(c->id(), c->username())));
			addToSnapshotStream(protocol::MessagePtr(new protocol::UserAttr(c->id(), c->isUserLocked(), c->isOperator())));
		}
	}

	emit snapshotCreated();
}

bool SessionState::addToSnapshotStream(protocol::MessagePtr msg)
{
	if(!_mainstream.hasSnapshot()) {
		_logger->logError("Tried to add a snapshot command, but there is no snapshot point!");
		return true;
	}
	protocol::SnapshotPoint &sp = _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>();
	if(sp.isComplete()) {
		_logger->logError("Tried to add a snapshot command, but the snapshot point is already complete!");
		return true;
	}

	sp.append(msg);

	emit newCommandsAvailable();

	if(sp.isComplete())
		cleanupCommandStream();

	return sp.isComplete();
}

void SessionState::abandonSnapshotPoint()
{
	_logger->logWarning(QString("Abandoning snapshot point (%1)!").arg(_mainstream.snapshotPointIndex()));

	if(!_mainstream.hasSnapshot()) {
		_logger->logError("Tried to abandon a snapshot point that doesn't exist!");
		return;
	}
	const protocol::SnapshotPoint &sp = _mainstream.snapshotPoint().cast<protocol::SnapshotPoint>();
	if(sp.isComplete()) {
		_logger->logError("Tried to abandon a complete snapshot point!");
		return;
	}

	foreach(Client *c, _clients) {
		if(c->isDownloadingLatestSnapshot()) {
			// TODO inform of the reason why
			c->kick();
		}
	}

	_mainstream.abandonSnapshotPoint();

	// Make sure no-one remains locked
	foreach(Client *c, _clients)
		c->barrierUnlock();

	_logger->logDebug(QString("Snapshot point rolled back to %1").arg(_mainstream.snapshotPointIndex()));
}

void SessionState::cleanupCommandStream()
{
	int removed = _mainstream.cleanup();
	_logger->logDebug(QString("Cleaned up %1 messages from the command stream.").arg(removed));
}

void SessionState::startSnapshotSync()
{
	_logger->logDebug("Starting snapshot sync!");

	// Barrier lock all clients
	foreach(Client *c, _clients)
		c->barrierLock();
}

void SessionState::snapshotSyncStarted()
{
	_logger->logDebug("Snapshot sync started!");

	// Lift barrier lock
	foreach(Client *c, _clients)
		c->barrierUnlock();
}

void SessionState::userBarrierLocked()
{
	// Count locked users
	int locked=0;
	foreach(const Client *c, _clients)
		if(c->isHoldLocked() || c->isDropLocked())
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
			_userids.reserve(msg.cast<ToolChange>().contextId());
			drawingContextToolChange(msg.cast<ToolChange>());
			break;
		case MSG_PEN_MOVE:
			drawingContextPenDown(msg.cast<PenMove>());
			break;
		case MSG_PEN_UP:
			drawingContextPenUp(msg.cast<PenUp>());
			break;
		case MSG_LAYER_CREATE:
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
		case MSG_ANNOTATION_CREATE:
			createAnnotation(msg.cast<AnnotationCreate>(), false);
			break;
		case MSG_ANNOTATION_DELETE:
			deleteAnnotation(msg.cast<AnnotationDelete>().id());
			break;
		case MSG_SESSION_CONFIG:
			setSessionConfig(msg.cast<SessionConf>());
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

void SessionState::createLayer(protocol::LayerCreate &cmd, bool assign)
{
	if(assign)
		cmd.setId(_layerids.takeNext());
	else
		_layerids.reserve(cmd.id());
	_layers.append(LayerState(cmd.id()));
}

void SessionState::reorderLayers(protocol::LayerOrder &cmd)
{
	QVector<LayerState> newlayers;
	QVector<LayerState> oldlayers = _layers;
	newlayers.reserve(_layers.size());

	// Set new order
	foreach(int id, cmd.order()) {
		for(int i=0;i<oldlayers.size();++i) {
			if(oldlayers[i].id == id) {
				newlayers.append(oldlayers[i]);
				oldlayers.remove(i);
			}
		}
	}
	// If there were any layers that were missed, add them to
	// the top of the stack in their original relative order
	for(int i=0;i<oldlayers.size();++i)
		newlayers.append(oldlayers[i]);

	_layers = newlayers;

	// Update commands ID list
	QList<uint8_t> validorder;
	validorder.reserve(_layers.size());
	for(int i=0;i<_layers.size();++i)
		validorder.append(_layers[i].id);

	cmd.setOrder(validorder);
}

bool SessionState::deleteLayer(int id)
{
	for(int i=0;i<_layers.size();++i) {
		if(_layers[i].id == id) {
			_layers.remove(i);
			_layerids.release(id);
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

void SessionState::createAnnotation(protocol::AnnotationCreate &cmd, bool assign)
{
	if(assign)
		cmd.setId(_annotationids.takeNext());
	else
		_annotationids.reserve(cmd.id());
}

bool SessionState::deleteAnnotation(int id)
{
	_annotationids.release(id);
	return true; // TODO implement ID tracker properly
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
		c->kick(0);
}

void SessionState::setSessionConfig(protocol::SessionConf &cmd)
{
	_locked = cmd.isLocked();
	_closed = cmd.isClosed();
	_layerctrllocked = cmd.isLayerControlsLocked();
	_lockdefault = cmd.isUsersLockedByDefault();
}

protocol::MessagePtr SessionState::sessionConf() const
{
	return protocol::MessagePtr(new protocol::SessionConf(
		_locked,
		_closed,
		_layerctrllocked,
		_lockdefault
	));
}

}

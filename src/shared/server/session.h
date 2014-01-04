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

#ifndef DP_SHARED_SERVER_SESSION_H
#define DP_SHARED_SERVER_SESSION_H

#include <QVector>
#include <QHash>
#include <QString>

#include "../util/idlist.h"
#include "../util/logger.h"
#include "../net/message.h"
#include "../net/messagestream.h"

namespace protocol {
	class ToolChange;
	class PenMove;
	class PenUp;
	class LayerCreate;
	class LayerOrder;
	class LayerACL;
	class AnnotationCreate;
	class SessionConf;
}

namespace server {

class Client;

struct LayerState {
	LayerState() : id(0), locked(false) {}
	LayerState(int id) : id(id), locked(false) {}

	int id;
	bool locked;
	QList<uint8_t> exclusive;
};

struct DrawingContext {
	DrawingContext() : currentLayer(0), penup(true) {}

	int currentLayer;
	bool penup;
};

/**
 * The serverside session state.
 */
class SessionState : public QObject {
	Q_OBJECT
public:
	enum SessionSyncState {NOT_SYNCING, SYNC_WAIT_FOR_LOCK, SYNC_WAIT_FOR_ACK };

	explicit SessionState(int minorVersion, SharedLogger _logger, QObject *parent=0);

	/**
	 * @brief Get the minor protocol version of this session
	 *
	 * The server does not care about differences in the minor version, but
	 * clients do. The session's minor version is set by the user who creates
	 * the session.
	 * @return protocol minor version
	 */
	int minorProtocolVersion() const { return _minorVersion; }

	/**
	 * @brief Get the session password
	 *
	 * This is an empty string if no password is required
	 * @return password
	 */
	const QString &password() const { return _password; }
	void setPassword(const QString &password) { _password = password; }

	/**
	 * @brief Is the session closed to new users?
	 * @return true if new users will not be admitted
	 */
	bool isClosed() const { return _closed; }
	void setClosed(bool closed);

	/**
	 * @brief Is the session locked?
	 * @return true if general session lock is in place
	 */
	bool isLocked() const { return _locked; }
	void setLocked(bool locked);

	/**
	 * @brief Have layer controls been locked for non-operators?
	 * @return true if layer controls are locked
	 */
	bool isLayerControlLocked() const { return _layerctrllocked; }
	void setLayerControlLocked(bool locked);

	/**
	 * @brief Are newly joining users locked by default?
	 * @return true if users are automatically locked when they join
	 */
	bool isUsersLockedByDefault() const { return _lockdefault; }
	void setUsersLockedByDefault(bool locked);

	/**
	 * @brief Get the maximum number of users allowed in the session
	 *
	 * This setting only affects new joins. Old users are not removed,
	 * even if the limit is lowered.
	 * @return user limit
	 */
	int maxUsers() const { return _maxusers; }
	void setMaxUsers(int maxusers);

	/**
	 * @brief Add a new client to the session
	 * @param user
	 */
	void joinUser(Client *user, bool host);

	/**
	 * @brief Assign an ID for this user
	 *
	 * This is used during the login phase to prepare
	 * the user for joining the session
	 * @param user
	 */
	void assignId(Client *user);

	/**
	 * @brief Get a client by ID
	 * @param id user ID
	 * @return user or 0 if not found
	 */
	Client *getClientById(int id);

	/**
	 * @brief Get the number of clients in the session
	 * @return user count
	 */
	int userCount() const { return _clients.size(); }

	const QList<Client*> &clients() { return _clients; }

	/**
	 * @brief Get the drawing context for the given user
	 * @param id
	 * @return
	 */
	DrawingContext &drawingContext(int id) { return _drawingctx[id]; }

	/**
	 * @brief get the main command stream
	 * @return reference to main command stream
	 */
	const protocol::MessageStream &mainstream() const { return _mainstream; }

	/**
	 * @brief Add a command to the message stream.
	 *
	 * Emits newCommandsAvailable
	 * @param msg
	 */
	void addToCommandStream(protocol::MessagePtr msg);

	/**
	 * @brief Add a new snapshot point.
	 * @pre there are no unfinished snapshot points
	 */
	void addSnapshotPoint();

	/**
	 * @brief Add a message to the latest snapshot point.
	 * @param msg
	 * @pre there is an unfinished snapshot point
	 * @return true if this was the command that completed the snapshot
	 */
	bool addToSnapshotStream(protocol::MessagePtr msg);

	/**
	 * @brief Remove all pre-snapshot messages from the command stream
	 */
	void cleanupCommandStream();

	/**
	 * @brief Synchronize clients so that a new snapshot point can be generated
	 */
	void startSnapshotSync();

	/**
	 * @brief Snapshot synchronization has started
	 */
	void snapshotSyncStarted();

	/**
	 * @brief Get the layer with the given ID
	 * @param id
	 * @return pointer to layer or 0 if not found
	 */
	const LayerState *getLayerById(int id);

	/**
	 * @brief Set up the initial state based on the hosting users snapshot
	 *
	 * This is called only once
	 * @param messages
	 */
	void syncInitialState(const QList<protocol::MessagePtr> &messages);

	/**
	 * @brief Add an new layer to the list
	 * @param cmd layer creation command (will be updated with the new ID)
	 * @param assign if true, assign an ID for the layer
	 */
	void createLayer(protocol::LayerCreate &cmd, bool assign);

	/**
	 * @brief Reorder layers
	 *
	 * Layer list may be incomplete. The messag will be updated
	 * with the complete layer list.
	 * @param cmd
	 */
	void reorderLayers(protocol::LayerOrder &cmd);

	/**
	 * @brief Delete the layer with the given ID
	 * @param id
	 * @return true if layer existed
	 */
	bool deleteLayer(int id);

	/**
	 * @brief Update layer access control list
	 * @param cmd
	 * @return true if layer existed
	 */
	bool updateLayerAcl(const protocol::LayerACL &cmd);

	/**
	 * @brief Add a new annotation
	 *
	 * This just reserves the ID for the annotation
	 * @param assign if true, assign a new ID for the annotation, otherwise reserve the given one.
	 */
	void createAnnotation(protocol::AnnotationCreate &cmd, bool assign);

	/**
	 * @brief Delete an annotation
	 * @param id
	 * @return true if annotation was deleted
	 */
	bool deleteAnnotation(int id);

	/**
	 * @brief Set the configurable options of the session
	 * @param cmd
	 */
	void setSessionConfig(protocol::SessionConf &cmd);

	/**
	 * @brief Update a drawing context's layer
	 * @param cmd
	 */
	void drawingContextToolChange(const protocol::ToolChange &cmd);

	void drawingContextPenDown(const protocol::PenMove &cmd);

	void drawingContextPenUp(const protocol::PenUp &cmd);

	void kickAllUsers();

signals:
	//! Last user in the session just left
	void lastClientLeft();

	//! New commands have been added to the main stream
	void newCommandsAvailable();

	//! A new snapshot was just created
	void snapshotCreated();

private slots:
	void removeUser(Client *user);
	void userBarrierLocked();

private:
	protocol::MessagePtr sessionConf() const;

	SharedLogger _logger;
	QList<Client*> _clients;

	protocol::MessageStream _mainstream;
	SessionSyncState _syncstate;

	UsedIdList _userids;
	UsedIdList _layerids;
	UsedIdList _annotationids;
	QVector<LayerState> _layers;
	QHash<int, DrawingContext> _drawingctx;

	int _minorVersion;
	int _maxusers;
	QString _password;

	bool _locked;
	bool _layerctrllocked;
	bool _closed;
	bool _lockdefault;
};

}

#endif


/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#ifndef DP_SHARED_SERVER_SESSION_H
#define DP_SHARED_SERVER_SESSION_H

#include <QVector>
#include <QHash>
#include <QString>
#include <QObject>
#include <QDateTime>

#include "sessiondesc.h"
#include "../util/logger.h"
#include "../util/announcementapi.h"
#include "../net/message.h"
#include "../net/messagestream.h"

namespace protocol {
	class ToolChange;
	class PenMove;
	class PenUp;
	class LayerCreate;
	class LayerOrder;
	class LayerACL;
	class SessionConf;
}

namespace recording {
	class Writer;
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
	SessionState(const SessionId &id, int minorVersion, const QString &founder, QObject *parent=0);

	/**
	 * \brief Get the ID of the session
	 */
	SessionId id() const { return _id; }

	/**
	 * @brief Get the name of the user who started this session
	 * @return founder username
	 */
	QString founder() const { return _founder; }

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
	 * @brief Set whether session persistence is allowed
	 * @param allow
	 */
	void setPersistenceAllowed(bool allowed) { _allowPersistent = allowed; _persistent = _persistent & allowed; }
	bool isPersistenceAllowed() const { return _allowPersistent; }

	/**
	 * @brief Get the maximum session history size in bytes
	 *
	 * If the session history grows beyond this limit, a new snapshot point will
	 * be requested.
	 * @return
	 */
	uint historyLimit() const { return _historylimit; }
	void setHistoryLimit(uint limit) { _historylimit = limit; }

	/**
	 * @brief Set the name of the recording file to create
	 *
	 * The recording will be created after a snapshot point has been created.
	 * @param filename path to output file
	 * @param split if true, a new file will be started at every snapshot
	 */
	void setRecordingFile(const QString &filename, bool split) { _recordingFile = filename; _splitRecording = split; }

	/**
	 * @brief Stop any recording that might be in progress
	 */
	void stopRecording();

	/**
	 * @brief Get the session password hash
	 *
	 * This is an empty string if no password is required
	 * @return password
	 */
	const QByteArray &passwordHash() const { return _passwordhash; }
	void setPasswordHash(const QByteArray &passwordhash);

	/**
	 * @brief Set the session password
	 * @param password
	 */
	void setPassword(const QString &password);

	/**
	 * @brief A chat message that will be sent to users who join the session
	 * @param message message content. If empty, no welcome message will be sent
	 */
	void setWelcomeMessage(const QString &message) { _welcomeMessage = message; }
	const QString &welcomeMessage() const { return _welcomeMessage; }

	/**
	 * @brief Get the title of the session
	 * @return
	 */
	const QString &title() const { return _title; }
	void setTitle(const QString &title);

	/**
	 * @brief Is the session closed to new users?
	 *
	 * A session can be closed in two ways:
	 * - by setting the Closed flag
	 * - when user count reaches maxUsers()
	 *
	 * @return true if new users will not be admitted
	 */
	bool isClosed() const { return _closed || userCount() >= maxUsers(); }
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
	 * @brief Have putimage and fillrect been locked for non-operators?
	 * @return true if putimages are locked
	 */
	bool isPutImageLocked() const { return _putimagelocked; }
	void setPutImageLocked(bool locked);

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
	 * @brief Is this a persistent session
	 *
	 * A persistent session is not automatically deleted when the last user leaves.
	 */
	bool isPersistent() const { return _persistent; }
	void setPersistent(bool persistent);

	/**
	 * @brief Is this session eligible for hibernation
	 *
	 * Hibernatable sessions will be stored before they are deleted
	 */
	bool isHibernatable() const { return _hibernatable; }
	void setHibernatable(bool h) { _hibernatable = h; }

	/**
	 * @brief Are chat messages preserved in the session history?
	 *
	 * Normally, only messages tagged as "announcements" are stored.
	 *
	 */
	bool isChatPreserved() const { return _preservechat; }
	void setPreserveChat(bool preserve);

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
	 * @brief Get a client by user name
	 *
	 * The name comparison is case insensitive.
	 * Note! In debug mode username uniqueness is not enforced!
	 * @param username
	 * @return
	 */
	Client *getClientByUsername(const QString &username);

	/**
	 * @brief Get the number of clients in the session
	 * @return user count
	 */
	int userCount() const { return _clients.size(); }

	const QList<Client*> &clients() const { return _clients; }

	/**
	 * @brief Get the name of the session owner
	 *
	 * The operator who has been in the session the longest
	 * is considered the owner.
	 */
	QString ownerName() const;

	/**
	 * @brief Get the time the session was started
	 * @return timestamp
	 */
	const QDateTime &sessionStartTime() const { return _startTime; }

	/**
	 * @brief Get session uptime in nice human readable format
	 * @return
	 */
	QString uptime() const;

	/**
	 * @brief Get the time of the last join/logout event
	 * @return timestamp
	 */
	const QDateTime &lastEventTime() const { return _lastEventTime; }

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
	 * @brief Abandon the snapshot point currently under construction
	 *
	 * This will disconnect any client currently download the point.
	 */
	void abandonSnapshotPoint();

	/**
	 * @brief Add a message to the latest snapshot point.
	 * @param msg
	 * @pre there is an unfinished snapshot point
	 * @return true if this was the command that completed the snapshot
	 */
	bool addToSnapshotStream(protocol::MessagePtr msg);

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
	 * @brief Get the layer below the one with the given ID
	 * @param id
	 * @return layer or 0 if there is no such layer
	 */
	const LayerState *getLayerBelowId(int id);

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
	 * @param validate only create the layer if ID is valid
	 * @return true if layer ID namespace matched context ID
	 */
	bool createLayer(protocol::LayerCreate &cmd, bool validate);

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

	/**
	 * @brief Initiate the shutdown of this session
	 *
	 * The session will not persist after the users have been disconnected,
	 * nor will it be hibernated.
	 */
	void killSession();

	/**
	 * @brief Send a message to every user of this session
	 * @param message
	 */
	void wall(const QString &message);

	QString toLogString() const;

	sessionlisting::Announcement publicListing() const { return _publicListing; }
	void setPublicListing(const sessionlisting::Announcement &a) { _publicListing = a; }

	/**
	 * @brief Generate a request for session announcement
	 *
	 * @param url listing server API url
	 */
	void makeAnnouncement(const QUrl &url);

	/**
	 * @brief Generate a request for session announcement unlisting
	 */
	void unlistAnnouncement();

signals:
	//! A user just connected to the session
	void userConnected(SessionState *thisSession, Client *client);

	//! A user disconnected from the session
	void userDisconnected(SessionState *thisSession);

	/**
	 * @brief A publishable session attribute just changed.
	 *
	 * This signal is emitted when any of the following attributes are changed:
	 *
	 * - title
	 * - open/closed status
	 * - maximum user count
	 * - password
	 * - persistent mode
	 *
	 * @param thisSession
	 */
	void sessionAttributeChanged(SessionState *thisSession);

	//! New commands have been added to the main stream
	void newCommandsAvailable();

	//! A new snapshot was just created
	void snapshotCreated();

	//! Request session announcement
	void requestAnnouncement(const QUrl &url, const sessionlisting::Session &session);

	//! Request this session to be unlisted
	void requestUnlisting(const sessionlisting::Announcement &listing);

private slots:
	void removeUser(Client *user);
	void userBarrierLocked();

private:
	void cleanupCommandStream();
	void startRecording(const QList<protocol::MessagePtr> &snapshot);

	protocol::MessagePtr sessionConf() const;

	recording::Writer *_recorder;
	QString _recordingFile;
	bool _splitRecording;

	QList<Client*> _clients;

	protocol::MessageStream _mainstream;

	sessionlisting::Announcement _publicListing;

	int _lastUserId;
	QVector<LayerState> _layers;
	QHash<int, DrawingContext> _drawingctx;

	const QDateTime _startTime;
	QDateTime _lastEventTime;

	const SessionId _id;
	int _minorVersion;
	int _maxusers;
	uint _historylimit;

	QByteArray _passwordhash;
	QString _title;
	QString _founder;
	QString _welcomeMessage;

	bool _locked;
	bool _layerctrllocked;
	bool _putimagelocked;
	bool _closed;
	bool _lockdefault;
	bool _allowPersistent;
	bool _persistent;
	bool _hibernatable;
	bool _preservechat;
};

}

#endif


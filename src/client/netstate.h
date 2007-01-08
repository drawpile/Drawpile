/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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
#ifndef NETSTATE_H
#define NETSTATE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QQueue>

class QImage;

namespace protocol {
	class Message;
	class HostInfo;
	class UserInfo;
	class SessionInfo;
	class SessionSelect;
	class Authentication;
	class Error;
	class Acknowledgement;
	class Raster;
	class ToolInfo;
	class StrokeInfo;
	class StrokeEnd;
	class Synchronize;
	class SyncWait;
};

namespace drawingboard {
	class Brush;
	class Point;
}

namespace network {

class Connection;
class SessionState;

//! Information about a session
struct Session {
	Session(const protocol::SessionInfo *info);

	int id;
	int owner;
	QString title;
	quint16 width;
	quint16 height;
};

struct User {
	User(const QString& n, int i);

	QString name;
	int id;
};

typedef QList<Session> SessionList;
typedef QList<User> UserList;

//! Network state machine
/**
 * This class handles the state of a host connection.
 */
class HostState : public QObject {
	Q_OBJECT
	friend class SessionState;
	public:
		//! Construct a HostState object
		HostState(QObject *parent);

		//! Get the local user ID as assigned by the server
		int localUserId() const { return userid_; }

		//! Get the session state
		SessionState *session(int id) { return mysessions_.value(id); }

		//! Set network connection object to use
		void setConnection(Connection *net);

		//! Initiate login sequence
		void login(const QString& username);

		//! Send a password
		void sendPassword(const QString& password);

		//! Host a session
		void host(const QString& title, const QString& password,
				quint16 width, quint16 height);

		//! Try joining automatically
		void join(const QString& name = "");

		//! Join a specific session
		void join(int id);

	public slots:
		//! Get a message from the network handler object
		void receiveMessage();

	signals:
		//! A password must be requested from the user
		/**
		 * A password can be needed to log in to the server and to join
		 * a drawing session.
		 * @param session if true, the password is for a session
		 */
		void needPassword();

		//! Login sequence completed succesfully
		void loggedin();

		//! Session joined succesfully
		/**
		 * @param id session id
		 */
		void joined(int id);

		//! Session left
		/**
		 * @param id session id
		 */
		void parted(int id);

		//! An error message was received from the host
		void error(const QString& message);

		//! Host has no sessions, cannot join
		void noSessions();

		//! The session we tried to join didn't exist
		void sessionNotFound();

		//! A session should be selected from the list and joined
		void selectSession(const network::SessionList& sessions);

		//! Session list was refreshed
		void sessionsListed();

	private slots:
		//! Join the latest session that the local user owns.
		void joinLatest();

		//! Automatically join the only session available
		void autoJoin();

	private:
		//! Refresh the list of sessions
		void listSessions();

		//! Handle a HostInfo message
		void handleHostInfo(const protocol::HostInfo *msg);

		//! Handle a UserInfo message
		void handleUserInfo(const protocol::UserInfo *msg);

		//! Handle a SessionInfo message
		void handleSessionInfo(const protocol::SessionInfo *msg);

		//! Handle a SessionSelect message
		void handleSessionSelect(const protocol::SessionSelect *msg);

		//! Handle authentication request
		void handleAuthentication(const protocol::Authentication *msg);

		//! Handle Acknowledgements
		void handleAck(const protocol::Acknowledgement *msg);

		//! Handle errors
		void handleError(const protocol::Error *msg);

		Connection *net_;

		QString username_;
		QByteArray passwordseed_;
		int passwordsession_;
		QString autojointitle_;

		int userid_;

		SessionState *newsession_;
		QHash<int, SessionState*> mysessions_;
		QHash<int,int> usersessions_;
		SessionList sessions_;

		bool loggedin_;
};

//! Network session state machine
/**
 * This class handles the state of a single session.
 */
class SessionState : public QObject {
	Q_OBJECT
	public:
		//! Construct a session state object
		SessionState(HostState *parent, const Session& info);

		//! Get session info
		const Session& info() const { return info_; }

		//! Get the list of users
		const UserList& users() const { return users_; }

		//! Get an image from received raster data
		bool sessionImage(QImage& image) const;

		//! Check if raster upload is in progress
		bool isUploading() const;

		//! Release raster data
		void releaseRaster();

		//! Send raster data
		void sendRaster(const QByteArray& raster);

		//! Select this session as active
		void select();

		//! Send a tool info message
		void sendToolInfo(const drawingboard::Brush& brush);

		//! Send a stroke info message
		void sendStrokeInfo(const drawingboard::Point& point);

		//! Send a stroke end message
		void sendStrokeEnd();

		//! Send ack/sync message to readyness for synchronization
		void sendAckSync();

		//! Handle session acks
		void handleAck(const protocol::Acknowledgement *msg);

		//! Handle session specific user info
		void handleUserInfo(const protocol::UserInfo *msg);

		//! Handle raster data
		void handleRaster(const protocol::Raster *msg);

		//! Handle sync request
		void handleSynchronize(const protocol::Synchronize *msg);

		//! Handle SyncWait command
		void handleSyncWait(const protocol::SyncWait *msg);

		//! Handle ToolInfo messages
		bool handleToolInfo(protocol::ToolInfo *msg);
		//
		//! Handle StrokeInfo messages
		bool handleStrokeInfo(protocol::StrokeInfo *msg);

		//! Handle StrokeEnd messages
		bool handleStrokeEnd(protocol::StrokeEnd *msg);

	signals:
		//! Raster data has been received
		/**
		 * @param percent percent of data received, range is [0..100]
		 */
		void rasterReceived(int percent);

		//! Raster data has been sent
		/**
		  * @param percent percent of data uploaded, range is [0..100]
		  */
		void rasterSent(int percent);

		//! A user has joined the session
		void userJoined(int id);

		//! A user has left the session
		void userLeft(int id);

		//! Raster data upload request
		void syncRequest();

		//! Lock the board when possible, in preparation for syncing a new user
		void syncWait();

		//! Synchronization complete, unlock board
		void syncDone();

		//! Results of a ToolInfo message
		void toolReceived(int user, const drawingboard::Brush& brush);

		//! Results of a StrokeInfo message
		void strokeReceived(int user, const drawingboard::Point& point);

		//! Results of a StrokeEnd message
		void strokeEndReceived(int user);

	private slots:
		//! Send a chunk of the raster buffer
		void sendRasterChunk();

	private:
		void flushDrawBuffer();

		HostState *host_;
		Session info_;
		UserList users_;
		QString password_;
		QByteArray raster_;
		unsigned int rasteroffset_;

		bool bufferdrawing_;
		QQueue<protocol::Message*> drawbuffer_;
};

}

#endif


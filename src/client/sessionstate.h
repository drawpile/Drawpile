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
#ifndef SESSIONSTATE_H
#define SESSIONSTATE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QQueue>

#include "sessioninfo.h"

class QImage;

namespace protocol {
	class Message;
	class UserInfo;
	class Acknowledgement;
	class Raster;
	class ToolInfo;
	class StrokeInfo;
	class StrokeEnd;
	class Synchronize;
	class SyncWait;
	class SessionEvent;
	class Chat;
};

namespace drawingboard {
	class Brush;
	class Point;
}

namespace network {

class HostState;
class Session;

//! Network session state machine
/**
 * This class handles the state of a single session.
 */
class SessionState : public QObject {
	Q_OBJECT
	public:
		//! Construct a session state object
		SessionState(HostState *parent, const Session& info);

		//! Handle session message
		void handleMessage(protocol::Message *msg);

		//! Get the host to which the session belongs
		HostState *host() const { return host_; }

		//! Get session info
		const Session& info() const { return info_; }

		//! Update session description
		void update(const Session& info);

		//! Does a user with the specified id exist in this session
		bool hasUser(int id) const;

		//! Get a specific user
		User &user(int id);

		//! Get a list of user ids in session
		QList<int> users() const { return users_.uniqueKeys(); }

		//! Get the number of users
		int userCount() const { return users_.count(); }

		//! Get an image from received raster data
		bool sessionImage(QImage& image) const;

		//! Check if raster upload is in progress
		bool isUploading() const;

		//! Check if general session lock is in place
		bool isLocked() const { return lock_; }

		//! Release raster data
		void releaseRaster();

		//! Send raster data
		void sendRaster(const QByteArray& raster);

		//! Select this session as active
		void select();

		//! Set password for this session
		void setPassword(const QString& password);

		//! Admin command. Lock/unlock the session
		void lock(bool l);

		//! Admin command. Set session user limit
		void setUserLimit(int count);

		//! Send a tool info message
		void sendToolInfo(const drawingboard::Brush& brush);

		//! Send a stroke info message
		void sendStrokeInfo(const drawingboard::Point& point);

		//! Send a stroke end message
		void sendStrokeEnd();

		//! Send ack/sync message to readyness for synchronization
		void sendAckSync();

		//! Send a chat message
		void sendChat(const QString& message);

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

		//! User has been (un)locked
		void userLocked(int id, bool lock);

		//! Session has been (un)locked
		void sessionLocked(bool lock);

		//! User limit has changed
		void userLimitChanged(int limit);

		//! Session owner changed
		void ownerChanged();

		//! A user got kicked from the session
		void userKicked(int id);

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

		//! A chat message
		void chatMessage(const QString& nick, const QString& msg);

	private slots:
		//! Send a chunk of the raster buffer
		void sendRasterChunk();

	private:
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

		//! Handle session event
		void handleSessionEvent(const protocol::SessionEvent *msg);

		//! Handle ToolInfo messages
		bool handleToolInfo(protocol::ToolInfo *msg);
		//
		//! Handle StrokeInfo messages
		bool handleStrokeInfo(protocol::StrokeInfo *msg);

		//! Handle StrokeEnd messages
		bool handleStrokeEnd(protocol::StrokeEnd *msg);

		//! Handle chat messages
		void handleChat(const protocol::Chat *msg);

		//! Flush the drawing command buffer
		void flushDrawBuffer();

		//! The host this session belongs to
		HostState *host_;

		//! Information about this session
		Session info_;

		//! List of users in this session
		QHash<int, User> users_;

		//! Buffer to hold raster data for receiving or sending
		QByteArray raster_;

		//! Starting position of raster data chunk that will be sent next
		uint rasteroffset_;

		//! Is the session lock
		bool lock_;

		//! Buffer drawing commands, instead of emitting them right away
		bool bufferdrawing_;

		//! Drawing command buffer
		/**
		 * The buffer is used to accumulate drawing commands that arrive
		 * while the initial board contents (raster data) has not yet
		 * fully downloaded.
		 */
		QQueue<protocol::Message*> drawbuffer_;
};

}

#endif


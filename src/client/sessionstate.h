/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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
class QStringList;

namespace drawingboard {
	class Brush;
	class Point;
}

namespace protocol {
	class Packet;
	class ToolSelect;
	class StrokePoint;
	class StrokeEnd;
	class BinaryChunk;
}

namespace network {

class HostState;
class Session;

//! Network session state machine
/**
 * This class handles the state of a single session.
 */
class SessionState : public QObject {
	friend class HostState;
	Q_OBJECT
	public:
		//! Construct a session state object
		SessionState(HostState *parent, const Session& info);

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

		//! Release raster data
		void releaseRaster();

		//! Send raster data
		void sendRaster(const QByteArray& raster);

		//! Set password for this session
		void setPassword(const QString& password);

		//! Admin command. Lock/unlock the session
		void lock(bool l);

		//! Admin command. Set session user limit
		void setUserLimit(int count);

		//! Send a tool select message
		void sendToolSelect(const drawingboard::Brush& brush);

		//! Send a stroke info message
		void sendStrokePoint(const drawingboard::Point& point);

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

		//! Session owner changed
		void boardChanged();

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
		//! Handle a session message
		bool handleMessage(const QStringList& tokens);

		//! Handle a tool select
		bool handleToolSelect(protocol::ToolSelect *ts);

		//! Handle a stroke
		bool handleStroke(protocol::StrokePoint *s);

		//! Handle stroke end
		bool handleStrokeEnd(protocol::StrokeEnd *se);

		//! Handle a binary chunk (raster data)
		bool handleBinaryChunk(protocol::BinaryChunk *bc);

		//! Handle a chat message
		void handleChat(const QStringList& tokens);

		//! Change user info
		void updateUser(const QStringList& tokens);

		//! A user left the session
		void partUser(const QStringList& tokens);

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

		//! How many bytes to expect
		int expectRaster_;

		//! Starting position of raster data chunk that will be sent next
		int rasteroffset_;

		//! Buffer drawing commands, instead of emitting them right away
		bool bufferdrawing_;
		
		//! Drawing command buffer
		/**
		 * The buffer is used to accumulate drawing commands that arrive
		 * while the initial board contents (raster data) has not yet
		 * fully downloaded.
		 */
		QQueue<protocol::Packet*> drawbuffer_;
};

}

#endif


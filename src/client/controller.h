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
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>

#include "tools.h"
#include "sessioninfo.h"

namespace drawingboard {
	class Board;
	class BoardEditor;
	class Point;
}

namespace interface {
	class BrushSource;
	class ColorSource;
}

namespace network {
	class Connection;
	class HostState;
	class SessionState;
}

class QUrl;
class QImage;

//! Controller for drawing and network operations
/**
 * The controller handles all drawing commands coming in from the
 * network or the user. Drawing commands received from the network
 * are committed to the board and user commands are sent to the server.
 *
 * Before finishing their roundtrip from the server, user commands
 * are displayed on a special preview layer. This provides immediate
 * feedback even when the network is congested. Preview strokes are
 * removed as the real ones are received from the server.
 */
class Controller : public QObject
{
	Q_OBJECT
	friend class tools::BrushBase;
	friend class tools::ColorPicker;
	public:
		Controller(QObject *parent=0);
		~Controller();

		//! Set drawing board to use
		void setModel(drawingboard::Board *board);

		//! Connect to host and join a session
		void joinSession(const QUrl& url);

		//! Connect to host and start a new session
		void hostSession(const QUrl& url, const QString& password, const QString& title, const QImage& image, int maxusers, bool allowDraw);

		//! Check if connection is still established
		bool isConnected() const;

		//! Check if raster upload is in progress
		bool isUploading() const;

		//! Get the session
		network::SessionState *session() const { return session_; }

	public slots:
		//! Send a password
		void sendPassword(const QString& password);

		//! Disconnect from host
		void disconnectHost();

		//! Lock the entire board
		void lockBoard(bool lock);

		//! Allow/disallow new users to join the session
		void disallowJoins(bool disallow);

		//! Send chat message
		void sendChat(const QString& msg);

		void penDown(const drawingboard::Point& point);
		void penMove(const drawingboard::Point& point);
		void penUp();
		void setTool(tools::Type tool);

	signals:
		//! This signal indicates that the drawing board has been changed
		void changed();

		//! Connection with remote host established
		void connected(const QString& address);

		//! Login succesfull
		void loggedin();

		//! Raster data download progress
		void rasterDownloadProgress(int percent);

		//!  Raster data upload progress
		void rasterUploadProgress(int percent);

		//! Host disconnected
		void disconnected(const QString& message);

		//! Session was joined
		void joined();

		//! Session was left
		void parted();

		//! A user joined the session
		void userJoined(const network::User& user);

		//! A user left the session
		void userParted(const network::User& user);

		//! A user got kicked out
		void userKicked(const network::User& user);

		//! User status has changed
		void userChanged(const network::User& user);

		//! Board info has changed
		void boardChanged();

		//! A password is required
		void needPassword();

		//! Lock the board UI
		void lockboard(const QString& reason);

		//! Unlock the board UI
		void unlockboard();

		//! A network error occured
		void netError(const QString& message);

		//! Chat message received
		void chat(const QString& nick, const QString& msg);

	private slots:
		//! Connection to a host was established
		void netConnected();

		//! Connection to a host was disconnected
		void netDisconnected();

		//! A session was joined
		void sessionJoined();

		//! A new user joins
		void addUser(int id);

		//! A user left the session
		void removeUser(int id);

		//! Raster data was received
		void rasterDownload(int p);

		//! A raster upload request was received
		void rasterUpload();

		//! Received a polite synchronization request
		void syncWait();

		//! Synchronization was completed
		void syncDone();

		//! Session was locked ungracefully
		void sessionLocked(bool lock);

		//! A single user has been (un)locked
		void userLocked(int id, bool lock);

		//! A user was kicked from the session
		void sessionKicked(int id);

	private:
		//! Connect to a host
		void connectHost(const QUrl& url);

		//! Enqueue board contents for upload
		void sendRaster();

		//! Lock the board for synchronizing a new user
		void lockForSync();

		//! The drawing board (model) associated with this controller
		drawingboard::Board *board_;

		//! A collection of tools for the local user to modify the board
		tools::ToolCollection toolbox_;

		//! The currently selected tool
		tools::Tool *tool_;

		//! The host
		network::HostState *host_;

		//! The session for this board
		network::SessionState *session_;

		//! Address of the remote host
		/**
		 * This address is not used internally, it is just shown to the
		 * user once connection is established.
		 */
		QString address_;

		//! User name to log in with
		QString username_;

		//! Path used when autojoining
		QString autojoinpath_;
		
		//! Server administrator password to send
		QString adminpasswd_;

		//! Initial maximum user count for the session
		int maxusers_;

		//! Is the pen down
		bool pendown_;

		//! Is a sync pending
		/**
		 * When pen is released, sendRaster is called
		 */
		bool sync_;

		//! Is a syncwait pending
		/**
		 * When pen is released, lockForSync is called
		 */
		bool syncwait_;

		//! Is the session locked
		bool lock_;
};

#endif


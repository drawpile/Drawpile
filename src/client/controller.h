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
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QUrl>

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

		//! Connect to host
		void connectHost(const QUrl& url, const QString& adminpasswd=QString());

		//! Start hosting a session
		void hostSession(const QString& title, const QString& password,
				const QImage& image, int userlimit, bool allowdraw,
				bool allowchat);

		//! Join a session
		void joinSession();

		//! Check if connection is still established
		bool isConnected() const { return net_ != 0; }

		//! Check if raster upload is in progress
		bool isUploading() const;

	public slots:
		//! Join a specific session
		void joinSession(int id);

		//! Send a password
		void sendPassword(const QString& password);

		//! Disconnect from host
		void disconnectHost();

		//! Remove a user from the drawing session
		void kickUser(int id);

		//! Lock/unlock user
		void lockUser(int id, bool lock);

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
		void joined(network::SessionState *session);

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

		//! The local user became session owner
		void becameOwner();

		//! There were no sessions to join
		void noSessions();

		//! The requested session didn't exist
		void sessionNotFound();

		//! A session should be selected from the list and joined
		void selectSession(const network::SessionList& list);

		//! A password is required
		void needPassword();

		//! Lock the board UI
		void lockboard(const QString& reason);

		//! Unlock the board UI
		void unlockboard();

		//! Allow/disallow new joins
		void joinsDisallowed(bool allow);

		//! A network error occured
		void netError(const QString& message);

		//! Chat message received
		void chat(const QString& nick, const QString& msg);

	private slots:
		void netConnected();
		void netDisconnected(const QString& message);
		void serverLoggedin();
		void finishLogin();
		void sessionJoined(int id);
		void sessionParted();
		void addUser(int id);
		void removeUser(int id);
		void rasterDownload(int p);
		void rasterUpload();
		void syncWait();
		void syncDone();
		void sessionLocked(bool lock);
		void userLocked(int id, bool lock);
		void sessionOwnerChanged();
		void sessionKicked(int id);
		void sessionUserLimitChanged(int count);

	private:
		void sendRaster();
		void lockForSync();

		drawingboard::Board *board_;
		tools::ToolCollection toolbox_;
		tools::Tool *tool_;

		network::Connection *net_;
		network::HostState *host_;
		network::SessionState *session_;

		QString address_;
		QString username_;
		QString autojoinpath_;
		QString adminpasswd_;
		int maxusers_;

		bool pendown_;
		bool sync_;
		bool syncwait_;
		bool lock_;
};

#endif


/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#ifndef DP_SRV_CLIENT_H
#define DP_SRV_CLIENT_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>

class QTcpSocket;

namespace protocol {
	class MessageQueue;
	class LoginId;
	class Message;
	class Packet;
	class BinaryChunk;
}

namespace server {

class Server;

/**
 * A client of the server.
 */
class Client : public QObject {
	Q_OBJECT
	public:
		enum State {
			// Expect protocol identifier
			CONNECT,
			// Expect password
			AUTHENTICATION,
			// Expect user details
			LOGIN,
			// Synchronizing
			SYNC,
			// User is an active part of the session
			ACTIVE
		};
		 //! Construct a new client. Clients start out in CONNECT state.
		Client(int id, Server *parent, QTcpSocket *socket, bool locked);

		//! Get the client's address
		const QHostAddress& address() const { return _address; }

		 //! Kick the user out.
		void kick(const QString& message);

		//! Get the state of the user.
		State state() const { return _state; }

		//! Get the ID of the user
		int id() const { return _id; }

		//! Get the name of the user. The name is only valid after LOGIN state.
		const QString& name() const { return _name; }

		//! Set the name of the user
		void setName(const QString& name) { _name = name; }

		//! Is the user locked? Drawing commands sent by a locked user are dropped.
		bool isLocked() const { return _lock | _syncready; }

		//! Lock or unlock this client
		void lock(bool status);

		//! Has the user sent a drawing command
		bool hasSentStroke() const { return _sentStroke; }

		//! Is the user ready for synchronization?
		bool isSyncReady() const { return _syncready; }

		//! Is the user a ghost?
		bool isGhost() const;

		//! Turn this user into a ghost
		void makeGhost();

		//! Send arbitrary data to this user.
		void sendRaw(const QByteArray& data);

		//! Lock the user for synchronization.
		void syncLock();

		//! Remove the synchronization lock.
		void syncUnlock();

		//! Request the user to start sending a copy of their board
		void requestRaster();

		//! Get the last tool select message received from this user
		const QByteArray& lastTool() const { return _lastTool; }

		//! Get the last layer select received from this user
		int lastLayer() const { return _lastLayer; }

		//! Get an info message about this user.
		QString toMessage() const;

	signals:
		void disconnected(int id);

		// User becomes ready or unready for synchronization
		void syncReady(int id, bool state);

	private slots:
		// New data is available
		void newData();
		// Kick the user out for a protocol violation
		void bail(const char* message="unspecified error");
		// Socket was closed
		void closeSocket();
		//! Send the next chunk of raster buffer
		void sendBufferChunk();

	private:
		void expectRaster(const QStringList& tokens);
		void handleChat(const QStringList& tokens);
		void handlePassword(const QStringList& tokens);
		void handleLock(const QString& token, bool lock);

		void handleLogin(const protocol::LoginId *pkt);
		void handleMessage(const protocol::Message *msg);
		void handleBinary(const protocol::BinaryChunk *bin);
		void handleDrawing(const protocol::Packet *bin);
		void handleAnnotation(const QStringList& tokens);

		void loginLocalUser(const QStringList& tokens);
		void sendBuffer();

		int _id;
		QString _name;

		Server *_server;
		protocol::MessageQueue *_socket;

		State _state;
		bool _sentStroke;
		bool _lock;
		bool _syncready;
		bool _giveraster;
		int _rasteroffset;
		QByteArray _lastTool;
		int _lastLayer;

		QString _salt;
		QHostAddress _address;
};

}

#endif


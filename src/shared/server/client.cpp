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

#include <QTcpSocket>
#include <QHostAddress>

#include "server.h"
#include "client.h"
#include "../net/messagequeue.h"
#include "../net/login.h"
#include "../net/message.h"
#include "../net/binary.h"

using protocol::Message;

namespace server {

/**
 * Construct a client
 * @param id client id
 * @param server parent server
 * @param socket client socket
 */
Client::Client(int id, Server *server, QTcpSocket *socket)
	: QObject(server), _id(id), _server(server), _socket(new protocol::MessageQueue(socket, this)), _state(CONNECT), _lock(true), _syncready(false), _giveraster(false)
{
	qDebug() << "New client connected from " << socket->peerAddress();
	connect(_socket, SIGNAL(messageAvailable()), this, SLOT(newData()));
	connect(_socket, SIGNAL(badData()), this, SLOT(bail()));
	connect(socket, SIGNAL(disconnected()), this, SLOT(closeSocket()));
}

/**
 * Handle received data.
 */
void Client::newData() {
	protocol::Packet *pkt;
	while((pkt=_socket->getPending())) {
		switch(pkt->type()) {
			case protocol::STROKE:
			case protocol::STROKE_END:
			case protocol::TOOL_SELECT:
				handleDrawing(pkt);
				break;
			case protocol::LOGIN_ID:
				handleLogin(static_cast<protocol::LoginId*>(pkt));
				break;
			case protocol::MESSAGE:
				handleMessage(static_cast<Message*>(pkt));
				break;
			case protocol::BINARY_CHUNK:
				handleBinary(static_cast<protocol::BinaryChunk*>(pkt));
				break;
			default: qDebug() << "unhandled!"; break; // TODO
		}
		delete pkt;
	}
}

/**
 * A message (KICK) is sent to the user to inform them of the reason
 * and the connection is cut.
 * @param message reason why the user was kicked
 */
void Client::kick(const QString& message) {
	qDebug() << "Kicking user" << _id << "reason:" << message;
	QStringList msg;
	msg << "KICK" << message;
	_socket->send(Message(Message::quote(msg)));
	_socket->closeWhenReady();
}

void Client::bail(const char* message) {
	qWarning() << "Disconnecting client" << _id << "due to protocol violation:" << message;
	_socket->close();
}

/**
 * A ghost is a user that has disconnected, but is kept around because
 * strokes drawn by them are still out there.
 * The user is marked as a ghost by prefixing the username with '!'
 */
bool Client::isGhost() const {
	return _name.length()>1 && _name.at(0)=='!';
}

void Client::makeGhost() {
	_name.prepend('!');
}

void Client::closeSocket() {
	emit disconnected(_id);
}

void Client::send(const QByteArray& data) {
	_socket->send(data);
}

/**
 * The users will lock themselves for real as soon as they have finished
 * any ongoing strokes.
 *
 * The client may allow the user to continue drawing while sync lock is in
 * place, just buffering the commands for sending when the lock is lifted.
 * Typically, a sync lock wont be on for very long, as it is removed almost
 * immediately after everyone has been synchronized.
 */
void Client::syncLock() {
	_socket->send(Message("SLOCK"));
}

/**
 * Lift the sync lock
 */
void Client::syncUnlock() {
	if(_syncready) {
		_socket->send(Message("SUNLOCK"));
		_syncready = false;
	}
}

/**
 * The user will set aside a copy of their drawing board and start sending
 * it. Normal drawing may continue while raster data is being transferred.
 */
void Client::requestRaster() {
	_socket->send(Message("GIVERASTER"));
	_giveraster = true;
}

/**
 * Login sequence goes as follows:
 * 1. Client sends login id.
 * 2. Server accepts or disconnects.
 * 3. Server asks for authentication (if no password set, skip to step 6)
 * 4. Client sends password
 * 5. Server accepts password or disconnects
 * 6. Server asks for client local users with LUSERS
 * 7. Client responds with USER
 * 8. Server goes to sync mode and sends list of users and tool selects
 * 9. All users lock themselves
 * 10. Server picks a random user and requests raster data, then goes back to normal mode
 * 11. Server relays raster data to new user
 * 12. Once data is received, new user goes to ACTIVE state.
 */
void Client::handleLogin(const protocol::LoginId *pkt) {
	if(_state != CONNECT) {
		bail("login not applicable to this state");
	} else {
		if(pkt->isCompatible()) {
			// TODO authentication
			_state = LOGIN;
			_socket->send(Message("WHORU"));
		} else {
			// Not a drawpile client.
			bail("not a DP client of proper version");
		}
	}
}

/**
 * Handle command messages and replies sent by the client
 */
void Client::handleMessage(const Message *msg) {
	QStringList tokens = msg->tokens();
	qDebug() << name() << "MSG:" << msg->message();
	if(tokens.isEmpty()) {
		bail("no message");
		return;
	}

	switch(_state) {
		case ACTIVE:
			if(tokens[0].compare("SAY")==0) {
				handleChat(tokens);
			} else if(tokens[0].compare("BOARD")==0) {
				if(_server->board().set(_id, tokens)==false)
					kick("Invalid board change command");
				else
					_server->redistribute(false, true, Message(_server->board().toMessage()).serialize());
			} else if(tokens[0].compare("SYNCREADY")==0) {
				qDebug() << _name << "is ready for sync.";
				_syncready = true;
				emit syncReady(_id, true);
			} else if(tokens[0].compare("RASTER")==0) {
				expectRaster(tokens);
			} else {
				qDebug() << "Unexpected message" << tokens[0];
				bail("unexpected message");
			}
			break;
		case SYNC:
			if(tokens[0].compare("SYNCREADY")==0) {
				qDebug() << _name << "is ready for first sync.";
				_syncready = true;
				emit syncReady(_id, true);
			} else {
				qDebug() << "Unexpected message for SYNC" << tokens[0];
				bail("unexpected message");
			}
			break;
		case LOGIN:
				if(tokens[0].compare("IAM")==0) {
						if(tokens.size()!=2)
							bail("bad login message");
						else {
							QString name = tokens[1];
							if(_server->hasClient(name))
								kick("Name already taken");
							else {
								_name = name;
								_state = SYNC;
								qDebug() << "New user" << _name;
								// Tell the user about the board
								_socket->send(Message(_server->board().toMessage()));
								// Tell everyone about the new user
								_server->redistribute(true, true, Message(toMessage()).serialize());
								// Tell the user about other users
								_server->briefClient(_id);
								// Synchronize
								if(_server->board().exists()) {
									if(_server->board().validBuffer()) {
										sendBuffer();
									} else {
										connect(&_server->board(), SIGNAL(rasterAvailable()), this, SLOT(sendBufferChunk()));
										_server->syncUsers();
									}
								} else {
									_state = ACTIVE;
								}
							}
						}
					} else {
					  bail("expected IAM");
					}
				  break;
		case AUTHENTICATION:
		case CONNECT: bail("todo"); break;
	}
}

/**
 * When we receive a promise for raster data (and we must be expecting it)
 * inform other users we will be relaying it.
 */
void Client::expectRaster(const QStringList& tokens) {
	if(_giveraster==false) {
		bail("user tried to send raster, but is not allowed to");
	} else {
		if(tokens.size()!=2) {
			bail("invalid RASTER message");
		} else {
			qDebug() << "User" << _id << "promised" << tokens[1] << "bytes of raster data.";
			_server->board().setExpectedBufferLength(tokens[1].toInt());
		}
	}
}

/**
 * Add the originator of the chat message and redistribute
 */
void Client::handleChat(const QStringList& tokens) {
	if(tokens.size()!=2)
		kick("Invalid chat message");
	else {
		QStringList chat;
		chat << "SAY" << QString::number(_id) << tokens[1];
		_server->redistribute(true, true, Message(chat).serialize());
	}
}

/**
 * A piece of binary data is received, redistribute it to other clients.
 * We currently don't care about the actual size of the data, although
 * we could add checks to make sure the user is actually sending as much
 * as they promised.
 */
void Client::handleBinary(const protocol::BinaryChunk *bin) {
	qDebug() << "Got" << bin->data().size() << "bytes of binary data. Still" << (_server->board().rasterBufferLength()-_server->board().rasterBuffer().size()-bin->data().size()) << "bytes to go.";
	if(_server->board().rasterBufferLength()==0) {
		bail("not authorized to send raster data");
		return;
	}
	bool more = true;
	int total = _server->board().rasterBuffer().size() + bin->data().size();

	if(total > _server->board().rasterBufferLength()) {
		kick("You sent too much data (" + QString::number(total - _server->board().rasterBufferLength()) + " extra bytes)");
		return;
	} else if(total ==_server->board().rasterBufferLength()) {
		_giveraster = false;
		qDebug() << "Got final raster piece.";
		more = false;
	}

	_server->board().addRaster(bin->data());
	if(more)
		_socket->send(Message("MORE"));
}

/**
 * Drawing commands are simply redistributed to all clients (including
 * the originator).
 * In the future, we could keep a cache of drawing commands sent since
 * the creation of the board or the last sync so new users could be added
 * without disturbing the others.
 * @return true when we're hanging on to the packet
 */
void Client::handleDrawing(const protocol::Packet *pkt) {
	// TODO check correct sender id
	QByteArray msg = pkt->serialize();
	_server->redistribute(true, true, msg);
	_server->board().addDrawingCommand(msg);
	if(pkt->type()==protocol::TOOL_SELECT)
		_lastTool = msg;
	_sentStroke = true;
}

/**
 * Start sending the user buffered data.
 * This is after the user has been brought up to speed on logged in
 * clients.
 */
void Client::sendBuffer() {
	qDebug() << _name << ": Sending valid buffer." << _server->board().drawingBuffer().size() << "bytes of drawing commands.";
	// First swamp the user with old drawing commands.
	// This is done before anyone else sends newer commands
	_socket->send(_server->board().drawingBuffer());

	Q_ASSERT(_server->board().rasterBufferLength()>0);
	// Now get the ball rolling by sending the user their first chunk
	// of raster data. Unlike the uploading user who cares about
	// the latency of the drawing commands, the receiving user just
	// wants to get started as soon as possible. Therefore, we sent
	// the raster data in as big chunks as possible.
	_rasteroffset = 0;
	sendBufferChunk();
	if(_server->board().rasterBuffer().size() < _server->board().rasterBufferLength())
		connect(&_server->board(), SIGNAL(rasterAvailable()),
				this, SLOT(sendBufferChunk()));
}

/**
 * Send the user a chunk of the raster buffer.
 */
void Client::sendBufferChunk() {
	if(_rasteroffset==0) {
		// If this is the first chunk, notify the client how much data
		// there is to come.
		QStringList rastermsg;
		rastermsg << "RASTER" << QString::number(_server->board().rasterBufferLength());
		_socket->send(Message(rastermsg));
	}
	const QByteArray& raster = _server->board().rasterBuffer();
	if(raster.size() == _server->board().rasterBufferLength())
		disconnect(&_server->board(), SIGNAL(rasterAvailable()),
				this, SLOT(sendBufferChunk()));

	do {
		int chunklen = raster.size() - _rasteroffset;
		if(chunklen > 0xffff-3)
			chunklen = 0xffff-3;
		qDebug() << "Sending user" << _id << chunklen << "bytes of raster data.";
		_socket->send(protocol::BinaryChunk(raster.mid(_rasteroffset, chunklen)));
		_rasteroffset += chunklen;
	} while(_rasteroffset < raster.size());
}

/**
 * The info messages are used to inform other clients about this
 * user.
 */
QString Client::toMessage() const {
	QStringList tkns;
	tkns << "USER" << QString::number(_id) << _name << (isLocked()?"1":"0");
	return Message::quote(tkns);
}

}


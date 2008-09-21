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
#include "../net/utils.h"

using protocol::Message;

namespace server {

/**
 * Make up a random string of letters and numbers
 */
QString randomSalt() {
	QString salt;
	for(int i=0;i<4;++i)
		salt += QChar(48 + qrand()%42);
	return salt;
}

/**
 * Construct a client
 * @param id client id
 * @param server parent server
 * @param socket client socket
 * @param locked does the client start out as locked?
 */
Client::Client(int id, Server *server, QTcpSocket *socket, bool locked)
	: QObject(server), _id(id), _server(server), _socket(new protocol::MessageQueue(socket, this)), _state(CONNECT), _lock(locked), _syncready(false), _giveraster(false), _address(socket->peerAddress())
{
	_server->printDebug("New client connected from " + socket->peerAddress().toString() + " and was given ID " + QString::number(id));
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
	_server->printDebug("Kicking user " + _name + ". Reason: " + message);
	QStringList msg;
	msg << "KICK" << message;
	_socket->send(Message(Message::quote(msg)));
	_socket->closeWhenReady();
}

void Client::bail(const char* message) {
	_server->printError("Disconnecting client " + QString::number(_id) + " due to protocol violation: " + message);
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

void Client::sendRaw(const QByteArray& data) {
	_socket->sendRaw(data);
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
 * This lock is set by the board owner and takes effect immediately.
 * If the user is currently drawing, there is a possibility that the strokes
 * sent before the LOCK command reaches the client go missing. This could
 * be averted with a lock buffer, but chances are, that if the admin
 * decided to cut someone off mid-drawing, we don't want them to continue.
 */
void Client::lock(bool status) {
	_lock = status;
	_server->redistribute(true, true, Message(toMessage()).serialize());
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
			if(_server->clientVersion()<0)
				_server->setClientVersion(pkt->softwareVersion());
			else if(pkt->softwareVersion() != _server->clientVersion()) {
				kick("Client version mismatch");
				return;
			}
			if(_server->password().isEmpty()) {
				_state = LOGIN;
				_socket->send(Message("WHORU"));
			} else {
				_state = AUTHENTICATION;
				_salt = randomSalt();
				QStringList msg;
				msg << "PASSWORD?" << _salt;
				_socket->send(Message(msg));
			}
		} else {
			// Not a drawpile client.
			bail("not a DP client of proper version");
		}
	}
}

void Client::handlePassword(const QStringList& tokens) {
	if(protocol::utils::hashPassword(_server->password(), _salt) == tokens.at(1)) {
		_state = LOGIN;
		_socket->send(Message("WHORU"));
	} else {
		kick("Incorrect password");
	}
}

/**
 * Lock the board or a single user. Admin only.
 * @param token user ID or "BOARD"
 * @param lock whether to lock or unlock
 */
void Client::handleLock(const QString& token, bool lock) {
	if(_server->board().owner() != _id) {
		kick("not owner");
	} else if(token == "BOARD") {
		_server->board().setLock(lock);
		_server->redistribute(true, true, Message(_server->board().toMessage()).serialize());
	} else
		_server->lockClient(_id, token.toInt(), lock);
}

/**
 * Handle command messages and replies sent by the client
 */
void Client::handleMessage(const Message *msg) {
	QStringList tokens = msg->tokens();
	if(tokens.isEmpty()) {
		bail("no message");
		return;
	}

	switch(_state) {
		case ACTIVE:
			if(tokens[0] == "ANNOTATE" || tokens[0]=="RMANNOTATION") {
				handleAnnotation(tokens);
			} else if(tokens[0] == "SAY") {
				handleChat(tokens);
			} else if(tokens[0] == "BOARD") {
				if(_server->board().owner()>0 && _server->board().owner()!=_id) {
					kick("not admin");
				} else {
					if(_server->board().set(_id, tokens)==false)
						kick("Invalid board change command");
					else
						_server->redistribute(false, true, Message(_server->board().toMessage()).serialize());
				}
			} else if(tokens[0] == "PASSWORD") {
				if(_server->board().owner()!=_id)
					kick("not admin");
				else
					_server->setPassword(tokens.at(1));
			} else if(tokens[0] == "SYNCREADY") {
				_server->printDebug(_name + " is ready for sync.");
				_syncready = true;
				emit syncReady(_id, true);
			} else if(tokens[0] == "RASTER") {
				expectRaster(tokens);
			} else if(tokens[0] == "LOCK") {
				handleLock(tokens.at(1), true);
			} else if(tokens[0] == "UNLOCK") {
				handleLock(tokens.at(1), false);
			} else if(tokens[0] == "KICK") {
				_server->kickClient(_id, tokens.at(1).toInt());
			} else if(tokens[0] == "USERLIMIT") {
				if(_server->board().owner() != _id) {
					kick("not owner");
				} else {
					_server->board().setMaxUsers(tokens.at(1).toInt());
					_server->redistribute(true, true, Message(_server->board().toMessage()).serialize());
				}
			} else {
				bail("unexpected message");
			}
			break;
		case SYNC:
			if(tokens[0] == "SYNCREADY") {
				_server->printDebug(_name + " is ready for first sync.");
				_syncready = true;
				emit syncReady(_id, true);
			} else {
				bail("unexpected message");
			}
			break;
		case LOGIN:
				if(tokens[0] == "IAM") {
						if(tokens.size()!=2)
							bail("bad login message");
						else {
							QString name = tokens[1];
							if(name.length() > _server->maxNameLength())
								kick("Name too long");
							else if(_server->hasClient(name))
								kick("Name already taken");
							else {
								_name = name;
								_state = SYNC;
								// Tell the user about the board
								_socket->send(Message(_server->board().toMessage()));
								// Tell everyone about the new user
								_server->redistribute(true, true, Message(toMessage()).serialize());
								// Tell the user about other users
								_server->briefClient(_id);
								// Synchronize
								if(_server->board().exists()) {
									sendBuffer();
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
				  if(tokens[0] == "PASSWORD")
					  handlePassword(tokens);
				  else
					  kick("Bad message for AUTHENTICATION state");
				  break;
		case CONNECT: break;
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
			_server->printDebug("User " + _name + " promised " + tokens[1] + "bytes of raster data.");
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
	if(_server->board().rasterBufferLength()==0) {
		kick("not authorized to send raster data");
		return;
	}
	bool more = true;
	int total = _server->board().rasterBuffer().size() + bin->data().size();

	if(total > _server->board().rasterBufferLength()) {
		kick("You sent too much data (" + QString::number(total - _server->board().rasterBufferLength()) + " extra bytes)");
		return;
	} else if(total ==_server->board().rasterBufferLength()) {
		_giveraster = false;
		more = false;
	}

	_server->board().addRaster(bin->data());
	if(more)
		_socket->send(Message("MORE"));
}

/**
 * Drawing commands are simply redistributed to all clients (including
 * the originator).
 * Commands from a locked user are dropped. This is not a fatal error condition,
 * since commands might have already been in the pipeline before the
 * lock message reached the user.
 * @return true when we're hanging on to the packet
 */
void Client::handleDrawing(const protocol::Packet *pkt) {
	// TODO check correct sender id
	if(_lock || _server->board().locked()) {
		_server->printError("Got a drawing command from locked user " + QString::number(_id));
		return;
	}
	QByteArray msg = pkt->serialize();
	_server->redistribute(true, true, msg);
	_server->board().addDrawingCommand(msg);
	if(pkt->type()==protocol::TOOL_SELECT)
		_lastTool = msg;
	_sentStroke = true;
}

/**
 * Clients can add, change or delete annotations. All annotations are
 * kept in the board state, so new users can be informed.
 */
void Client::handleAnnotation(const QStringList& tokens)
{
	if(tokens[0]=="RMANNOTATION") {
		if(_server->board().rmAnnotation(tokens.at(1).toInt())) {
			_server->redistribute(false, true, Message(tokens).serialize());
		} else {
			_server->printDebug("User " + QString::number(_id) + " tried to delete nonexistent annotation " + tokens.at(1));
		}
	} else {
		protocol::Annotation a(tokens);
		if(_server->board().addAnnotation(a))
			_server->redistribute(false, true, Message(a.tokens()).serialize());
		else
			_server->printDebug("Received change to an annotation that doesn't exist.");
	}
}

/**
 * Start sending the user buffered data.
 * This is after the user has been brought up to speed on logged in
 * clients.
 */
void Client::sendBuffer() {
	_rasteroffset = 0;
	if(_server->board().validBuffer()) {
		// First swamp the user with old drawing commands.
		// This is done before anyone else sends newer commands
		_socket->sendRaw(_server->board().drawingBuffer());

		// Now get the ball rolling by sending the user their first chunk
		// of raster data. Unlike the uploading user who cares about
		// the latency of the drawing commands, the receiving user just
		// wants to get started as soon as possible. Therefore, we sent
		// the raster data in as big chunks as possible.
		sendBufferChunk();
	} else {
		// If board buffer is not valid, a sync is needed.
		_server->syncUsers();
	}

	// If we don't have the whole buffer yet, send more chunks as soon
	// as we get them.
	if(_server->board().validBuffer()==false ||
			_server->board().rasterBuffer().size() < _server->board().rasterBufferLength())
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
		_socket->send(protocol::BinaryChunk(raster.mid(_rasteroffset, chunklen)));
		_rasteroffset += chunklen;
	} while(_rasteroffset < raster.size());
	// Once the raster buffer is completely sent, the user goes to ACTIVE
	// state.
	if(_rasteroffset == _server->board().rasterBufferLength()) {
		_state = ACTIVE;
		// Now that the user has a valid drawing board, send all
		// annotations.
		foreach(const protocol::Annotation& a, _server->board().annotations())
			_socket->send(protocol::Message(a.tokens()));
	}
}

/**
 * The info messages are used to inform other clients about this
 * user. The info message contains the following information:
 * - user ID
 * - user name
 * - lock status (sync lock not included)
 */
QString Client::toMessage() const {
	QStringList tkns;
	tkns << "USER" << QString::number(_id) << _name << (_lock?"1":"0");
	return Message::quote(tkns);
}

}


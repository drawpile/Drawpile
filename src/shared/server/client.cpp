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

Client::Client(int id, Server *server, QTcpSocket *socket)
	: QObject(server), _id(id), _server(server), _socket(new protocol::MessageQueue(socket, this)), _state(CONNECT), _lock(true), _giveraster(false)
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
		qDebug() << "got type: " << pkt->type();
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
			default: break; // TODO
		}
		delete pkt;
	}
}

void Client::kick(const QString& message) {
	QStringList msg;
	msg << "KICK" << message;
	_socket->send(Message(Message::quote(msg)));
	_socket->closeWhenReady();
}

void Client::bail(const char* message) {
	qWarning() << "Disconnecting client" << _id << "due to protocol violation:" << message;
	_socket->close();
}

void Client::closeSocket() {
	emit disconnected(_id);
}

void Client::send(const QByteArray& data) {
	_socket->send(data);
}

void Client::syncLock() {
	_synclock = true;
	if(_lock==false)
		_socket->send(Message("GLOCK"));
}

void Client::syncUnlock() {
	if(_synclock) {
		_synclock = false;
		// Unlock only if this user was not explicitly locked
		if(_lock==false)
			_socket->send(Message("UNLOCK"));
	}
}

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
			qDebug() << "identified";
			_socket->send(Message("WHORU"));
		} else {
			// Not a drawpile client.
			bail("not a DP client of proper version");
		}
	}
}

void Client::handleMessage(const Message *msg) {
	QStringList tokens = msg->tokens();
	qDebug() << "Msg:" << msg->message();
	if(tokens.isEmpty()) {
		bail("no message");
		return;
	}

	switch(_state) {
		case ACTIVE:
			if(tokens[0].compare("BOARD")==0) {
				if(_server->board().set(tokens)==false)
					kick("Invalid board change command");
				else
					_server->redistribute(false, true, Message(_server->board().toMessage()).serialize());
			} else {
				qDebug() << "Unexpected message" << tokens[0];
				bail("unexpected message");
			}
		case SYNC: break;
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
								qDebug() << "New user" << _name;
								_socket->send(Message(_server->board().toMessage()));
								if(_server->board().exists()) {
									_state = SYNC;
									_server->syncUsers();
								} else {
									_state = ACTIVE;
								}
								_server->redistribute(true, true, Message(toMessage()).serialize());
								_server->briefClient(_id);
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

void Client::handleBinary(const protocol::BinaryChunk *bin) {
	if(_giveraster==false) {
		bail("not authorized to send raster data");
		return;
	}
	if(bin->payloadLength()==0)
		_giveraster = false;
	if(_server->redistribute(true, false, bin->serialize())==0) {
		// Cancel sending if there are no users to receive data.
		_socket->send(Message("CANCELRASTER"));
		_giveraster = false;
	}
}

void Client::handleDrawing(const protocol::Packet *pkt) {
	// TODO check correct sender id
	_server->redistribute(true, true, pkt->serialize());
}

QString Client::toMessage() const {
	QStringList tkns;
	tkns << "USER" << QString::number(_id) << _name << (isLocked()?"1":"0");
	return Message::quote(tkns);
}

}


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

#include <QImage>

#include "../config.h"

#include "hoststate.h"
#include "sessionstate.h"
#include "sessioninfo.h"
#include "version.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/message.h"
#include "../shared/net/login.h"
#include "../shared/net/stroke.h"
#include "../shared/net/toolselect.h"
#include "../shared/net/binary.h"
#include "../shared/net/utils.h"

using protocol::Message;

namespace network {

HostState::HostState(QObject *parent)
	: QObject(parent), localuser_(-1), session_(0), host_(-1,"",0, 0, 0, false)
{
	// Relay socket signals
	connect(&socket_, SIGNAL(connected()), this, SIGNAL(connected()));
	connect(&socket_, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(networkError()));
	connect(&socket_, SIGNAL(disconnected()), this, SLOT(disconnectCleanup()));

	// Create a message queue to wrap the socket.
	// All data is sent and received through this.
	mq_ = new protocol::MessageQueue(&socket_, &socket_);
	connect(mq_, SIGNAL(messageAvailable()), this, SLOT(receiveMessage()));
	connect(mq_, SIGNAL(badData()), this, SLOT(gotBadData()));
}

void HostState::setHost(const QString& password, const QString& title, quint16 width, quint16 height, int maxusers, bool allowDraw) {
	host_ = Session(0, title, width, height, maxusers, !allowDraw);
	hostpassword_ = password;
}

void HostState::setNoHost() {
	host_ = Session(-1,"",0,0,0, false);
}

void HostState::connectToHost(const QString& host, quint16 port) {
	localuser_ = -1;
	socket_.connectToHost(host, port);
}

void HostState::disconnectFromHost() {
	if(socket_.state()==QAbstractSocket::UnconnectedState ||
			socket_.state()==QAbstractSocket::ClosingState) {
		qWarning("Tried to disconnect even though socket is not yet connected!");
	} else {
		socket_.disconnectFromHost();
		mq_->flush();
	}
}

/**
 * Receive messages and call the appropriate handlers.
 */
void HostState::receiveMessage()
{
	using namespace protocol;
	Packet *msg;
	// Get all available messages.
	while((msg = mq_->getPending())) {
		switch(msg->type()) {
			case STROKE:
				if(session_) {
					if(session_->handleStroke(static_cast<StrokePoint*>(msg)))
						msg = 0;
				} else {
					emit error("Received stroke before joining a session");
				}
				break;
			case STROKE_END:
				if(session_) {
					if(session_->handleStrokeEnd(static_cast<StrokeEnd*>(msg)))
						msg = 0;
				} else {
					emit error(tr("Received stroke end before joining a session"));
				}
				break;
			case TOOL_SELECT:
				if(session_) {
					if(session_->handleToolSelect(static_cast<ToolSelect*>(msg)))
						msg = 0;
				} else {
					emit error(tr("Received tool select before joining a session"));
				}
				break;
			case MESSAGE:
				handleMessage((Message*)msg);
				break;
			case BINARY_CHUNK:
				if(session_) {
					if(!session_->handleBinaryChunk(static_cast<BinaryChunk*>(msg)))
							emit error(tr("Received unexpected binary data"));
				} else {
					emit error(tr("Received binary chunk before joining a session"));
				}
				break;
			case LOGIN_ID: qWarning("Received login ID. We're not a server!"); break;
		}
		delete msg;
	}
}

void HostState::disconnectCleanup() {
	emit disconnected();
	delete session_;
	session_ = 0;
}

void HostState::gotBadData() {
	emit error(tr("Received invalid data"));
	disconnectFromHost();
}

void HostState::networkError() {
	if(socket_.error() != QAbstractSocket::RemoteHostClosedError) {
		emit error(socket_.errorString());
		disconnectFromHost();
	}
}

/**
 * Send Identifier message to log in. Server will disconnect or respond with
 * request for authentication or list of local users.
 * @pre \a isConnected()==true
 */
void HostState::login(const QString& username)
{
	Q_ASSERT(isConnected());
	username_ = username;
	mq_->send(protocol::LoginId(version::level));
}

/**
 * A password must be sent in response to an Authentication message.
 * The \a needPassword signal is used to notify when a password is required.
 * 
 * sendPassword is secure, a hash of the password + previously received seed
 * value is sent.
 * @param password password to send
 * @pre needPassword signal has been emitted
 */
void HostState::sendPassword(const QString& password)
{
	Q_ASSERT(password.length() != 0);
	
	mq_->send(Message("PASSWORD " + protocol::utils::hashPassword(password, salt_)));
}

void HostState::sendPacket(const protocol::Packet& packet) {
	mq_->send(packet);
}

/**
 * @param msg Message
 */
void HostState::handleMessage(const Message *msg)
{
	QStringList tkns = msg->tokens();
	if(tkns.isEmpty()) {
		emit error(tr("Server sent an invalid message."));
		return;
	}
	// Stateless messages
	if(tkns[0].compare("KICK")==0) {
		// User was kicked out
		emit error(tr("You were disconnected. Reason: %1").arg(tkns.at(1)));
	} else if(session_) {
		// Session messages
		if(session_->handleMessage(tkns)==false) {
			emit error(tr("Unhandled session message %1").arg(tkns[0]));
		}
	} else {
		// Login messages
		if(tkns[0] == "WHORU") {
			QStringList user;
			user << "IAM" << username_;
			mq_->send(Message(user));
		} else if(tkns[0] == "PASSWORD?") {
			salt_ = tkns.at(1);
			emit needPassword();
		} else if(tkns[0] == "NOBOARD") {
			session_ = new SessionState(this, Session(0, "", 0, 0, 0, false));
			if(host_.owner()==-1)
				emit error(tr("No session"));
			else {
				// Since there is no board, we don't expect any raster
				// data.
				mq_->send(Message(host_.tokens()));
				session_->setPassword(hostpassword_);
				session_->flushDrawBuffer();
				emit loggedin();
			}
		} else if(tkns[0] == "BOARD") {
			session_ = new SessionState(this, Session(tkns));
			if(host_.owner()!=-1)
				emit error(tr("Session already exists"));
			else
				emit loggedin();
		} else {
			emit error(tr("Unhandled message %1").arg(tkns[0]));
		}
	}
}

/**
 * This is called as soon as we get our local user id from the server.
 * The session will be ready to use then.
 */
void HostState::sessionJoinDone(int localid) {
	localuser_ = localid;
	emit joined();
}

}


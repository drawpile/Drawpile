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

#include <QDebug>
#include <QImage>
#include <memory>

#include "../config.h"

#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "sessioninfo.h"
#include "version.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/message.h"
#include "../shared/net/login.h"
#include "../shared/net/stroke.h"
#include "../shared/net/toolselect.h"

using protocol::Message;

namespace network {

HostState::HostState(QObject *parent)
	: QObject(parent), localuser_(-1), session_(0), host_(-1)
{
	// Relay socket signals
	connect(&socket_, SIGNAL(connected()), this, SIGNAL(connected()));
	connect(&socket_, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(networkError()));
	connect(&socket_, SIGNAL(disconnected()), this, SLOT(disconnectCleanup()));

	// Create a message queue to wrap the socket.
	// All data is sent and received through this.
	mq_ = new protocol::MessageQueue(&socket_);
	connect(mq_, SIGNAL(messageAvailable()), this, SLOT(receiveMessage()));
	connect(mq_, SIGNAL(badData()), this, SLOT(gotBadData()));
}

void HostState::setHost(const QString& title, quint16 width, quint16 height) {
	host_ = Session(0);
	host_.title = title;
	host_.width = width;
	host_.height = height;
}

void HostState::setNoHost() {
	host_ = Session(-1);
}

void HostState::connectToHost(const QString& host, quint16 port) {
	localuser_ = -1;
	socket_.connectToHost(host, port);
}

void HostState::disconnectFromHost() {
	socket_.disconnectFromHost();
	mq_->flush();
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
				if(session_)
					if(session_->handleStroke(static_cast<StrokePoint*>(msg)))
						msg = 0;
				else
					emit error("Received stroke before joining a session");
				break;
			case STROKE_END:
				if(session_)
					if(session_->handleStrokeEnd(static_cast<StrokeEnd*>(msg)))
						msg = 0;
				else
					emit error("Received stroke end before joining a session");
				break;
			case TOOL_SELECT:
				if(session_)
					if(session_->handleToolSelect(static_cast<ToolSelect*>(msg)))
						msg = 0;
				else
					emit error("Received tool select before joining a session");
				break;
			case MESSAGE:
				handleMessage((Message*)msg);
				break;
			default: emit error("unhandled message type");
		}
		delete msg;
	}
}

void HostState::disconnectCleanup() {
	delete session_;
	session_ = 0;
	emit disconnected();
}

void HostState::gotBadData() {
	emit error(tr("Received invalid data"));
	disconnectFromHost();
}

void HostState::networkError() {
	qDebug() << "A network error occurred:" << socket_.errorString();
	emit error(socket_.errorString());
	disconnectFromHost();
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
	mq_->send(protocol::LoginId(1));
}

/**
 * Change board settings.
 *
 * @param title session title
 * @param width board width
 * @param height board height
 * @pre user is logged in and is admin
 */
void HostState::changeBoard(const QString& title, quint16 width, quint16 height)
{
	Session ses(localuser_);
	ses.title = title;
	ses.width = width;
	ses.height = height;
	mq_->send(Message(ses.tokens()));
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
#if 0
	Q_ASSERT(password.length() != 0);
	
	protocol::Password *msg = new protocol::Password;
	msg->session_id = passwordsession_;
	
	int length;
	char *ptr = convert::toUTF(password, length, Utf16_);
	
	SHA1 hash;
	hash.Update(reinterpret_cast<const quint8*>(ptr), length);
	hash.Update(reinterpret_cast<const quint8*>(passwordseed_.constData()),
			passwordseed_.length());
	hash.Final();
	
	delete [] ptr;
	
	hash.GetHash(reinterpret_cast<quint8*>(msg->data));
	net_->send(msg);
#endif
}

/**
 * @param msg Message
 */
void HostState::handleMessage(const Message *msg)
{
	qDebug() << msg->message();
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
		if(tkns[0].compare("WHORU")==0) {
			QStringList user;
			user << "IAM" << username_;
			mq_->send(Message(user));
		} else if(tkns[0].compare("NOBOARD")==0) {
			session_ = new SessionState(this, Session(0));
			if(host_.owner==-1)
				emit error(tr("No session"));
			else {
				// Since there is no board, we don't expect any raster
				// data.
				mq_->send(Message(host_.tokens()));
				session_->flushDrawBuffer();
				emit loggedin();
			}
		} else if(tkns[0].compare("BOARD")==0) {
			session_ = new SessionState(this, Session(tkns));
			if(host_.owner!=-1)
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


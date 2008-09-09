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

#include "../config.h"

#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "brush.h"
#include "point.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/message.h"
#include "../shared/net/stroke.h"
#include "../shared/net/toolselect.h"

namespace network {

/**
 * A utility function to put an RGB value + opacity in 32 bits.
 */
quint32 encodeColor(const QColor& color, qreal opacity) {
	return (color.red() << 24) |
		(color.green() << 16) |
		(color.blue() << 8) |
		qRound(opacity*255);
}

/**
 * A utility to get an RGB value + opacity from 32 bits
 */
QColor decodeColor(quint32 color, qreal &opacity) {
	opacity = (color & 0x000000ff)/255.0;
	return QColor(
			(color >> 24),
			(color >> 16) & 0x000000ff,
			(color >> 8) & 0x000000ff
			);
}

/**
 * @param parent parent HostState
 * @param info session information
 */
SessionState::SessionState(HostState *parent, const Session& info)
	: QObject(parent), host_(parent), info_(info), rasteroffset_(0),lock_(false),bufferdrawing_(true)
{
	Q_ASSERT(parent);
#if 0
	users_[host_->localUser().id()] = User(
			host_->localUser().name(),
			host_->localUser().id(),
			fIsSet(info.mode, static_cast<quint8>(protocol::user::Locked)),
			this
			);
#endif
}

/**
 * @param info new session info
 */
void SessionState::update(const Session& info)
{
#if 0
	if(info_.maxusers != info.maxusers) {
		emit userLimitChanged(info.maxusers);
	}
#endif
	/** @todo check for other changes too */
	info_ = info;
}

/**
 * @param id user id
 * @return true if user exists
 */
bool SessionState::hasUser(int id) const
{
	return users_.contains(id);
}

/**
 * @param id user id
 * @pre hasUser(id)==true
 */
User &SessionState::user(int id)
{
	Q_ASSERT(hasUser(id));
	return users_[id];
}

/**
 * Get an image from received raster data. If received raster was empty,
 * a null image is loaded.
 * @param[out] image loaded image is put here
 * @retval false if buffer contained invalid data
 */
bool SessionState::sessionImage(QImage &image) const
{
	QImage img;
	if(raster_.isEmpty()) {
		image = QImage();
	} else {
		if(img.loadFromData(raster_)==false)
			return false;
		image = img;
	}
	return true;
}

/**
 * @retval true if uploading
 */
bool SessionState::isUploading() const
{
	if(raster_.length()>0 && rasteroffset_>0)
		return true;
	return false;
}

/**
 * Release raster data
 */
void SessionState::releaseRaster()
{
	raster_ = QByteArray();
}

/**
 * Start sending raster data.
 * The data will be sent in pieces, interleaved with other messages.
 * @param raster raster data buffer
 */
void SessionState::sendRaster(const QByteArray& raster)
{
	raster_ = raster;
	rasteroffset_ = 0;
	sendRasterChunk();
}

void SessionState::sendRasterChunk()
{
#if 0
	uint chunklen = 1024*4;
	if(rasteroffset_ + chunklen > unsigned(raster_.length()))
		chunklen = raster_.length() - rasteroffset_;
	if(chunklen==0) {
		rasteroffset_ = 0;
		releaseRaster();
		return;
	}
	protocol::Raster *msg = new protocol::Raster(
			rasteroffset_,
			chunklen,
			raster_.length(),
			new char[chunklen]
			);
	msg->session_id = info_.id;
	memcpy(msg->data, raster_.constData()+rasteroffset_, chunklen);
	rasteroffset_ += chunklen;
	host_->connection()->send(msg);
	emit rasterSent(100*rasteroffset_/raster_.length());
#endif
}

/**
 * @param l lock status
 * @pre user is session owner
 */
void SessionState::lock(bool l)
{
#if 0
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			(l ? protocol::SessionEvent::Lock : protocol::SessionEvent::Unlock),
			protocol::null_user,
			0 // aux (unused)
			);
	
	msg->session_id = info_.id;
	
	host_->connection()->send(msg);
#endif
}

/**
 * Changes the user limit for the session. This doesn't affect current users,
 * setting the limit lower than the number of users currently logged in will
 * just prevent new users from joining.
 * 
 * @param count number of users allowed
 */
void SessionState::setUserLimit(int count)
{
#if 0
	qDebug() << "Chaning user limit to" << count;
	protocol::SessionInstruction *msg = new protocol::SessionInstruction(
			protocol::SessionInstruction::Alter,
			info_.width,
			info_.height,
			info_.mode, // Set user mode (unchanged)
			count, // Set user limit
			0, // flags (unused)
			0, // title length (FIXME, empties the session title)
			0 // title (FIXME)
			);
	msg->session_id = info_.id;

	host_->lastsessioninstr_ = msg->action;
	host_->connection()->send(msg);
#endif
}

/**
 * @param brush brush info to send
 */
void SessionState::sendToolSelect(const drawingboard::Brush& brush)
{
	host_->mq_->send( protocol::ToolSelect(
			host_->localuser_,
			1,
			1,
			encodeColor(brush.color(1), brush.opacity(1)),
			encodeColor(brush.color(0), brush.opacity(0)),
			brush.radius(1),
			brush.radius(0),
			qRound(brush.hardness(1)*255),
			qRound(brush.hardness(0)*255),
			brush.spacing()
			)
		);

}

/**
 * @param point stroke coordinates to send
 */
void SessionState::sendStrokePoint(const drawingboard::Point& point)
{
	host_->mq_->send(protocol::StrokePoint(
				host_->localuser_,
				point.x(),
				point.y(),
				qRound(point.pressure()*255)
				)
			);
}

void SessionState::sendStrokeEnd()
{
	host_->mq_->send(protocol::StrokeEnd( host_->localuser_ ) );
}

void SessionState::sendAckSync()
{
#if 0
	protocol::Acknowledgement *msg = new protocol::Acknowledgement(
			protocol::Message::SyncWait
			);
	msg->session_id = info_.id;
	host_->connection()->send(msg);
#endif
}

void SessionState::sendChat(const QString& message)
{
	QStringList msg;
	msg << "SAY" << message;
	host_->mq_->send(protocol::Message(msg));
}

/**
 * @param tokens message tokens
 */
bool SessionState::handleMessage(const QStringList& tokens)
{
	if(tokens[0].compare("SAY")==0) {
		qDebug() << tokens;
	} else if(tokens[0].compare("USER")==0) {
		User user(this, tokens);
		users_[user.id()] = user;
		qDebug() << "User" << user.id() << user.name() << "locked:" << user.locked();
		// The session joining is complete when we get our own user ID
		// for the first time.
		if(host_->localuser_<0 && user.name().compare(host_->username_)==0)
			host_->sessionJoinDone(user.id());
	} else if(tokens[0].compare("BOARD")==0) {
		qDebug() << "board change";
	} else
		return false;
	return true;

#if 0
	do {
		using namespace protocol;
		Message *next = msg->next;
		switch(msg->type) {
			case Message::StrokeInfo:
				handleStrokeInfo(static_cast<StrokeInfo*>(msg));
				msg = 0;
				break;
			case Message::StrokeEnd:
				handleStrokeEnd(static_cast<StrokeEnd*>(msg));
				msg = 0;
				break;
			case Message::ToolInfo:
				handleToolInfo(static_cast<ToolInfo*>(msg));
				msg = 0;
				break;
			case Message::Acknowledgement:
				handleAck(static_cast<Acknowledgement*>(msg));
				break;
			case Message::Chat:
				handleChat(static_cast<Chat*>(msg));
				break;
			case Message::Raster:
				handleRaster(static_cast<Raster*>(msg));
				break;
			case Message::Synchronize:
				handleSynchronize(static_cast<Synchronize*>(msg));
				break;
			case Message::SyncWait:
				handleSyncWait(static_cast<SyncWait*>(msg));
				break;
			case Message::SessionEvent:
				handleSessionEvent(static_cast<SessionEvent*>(msg));
				break;
			case Message::UserInfo:
				handleUserInfo(static_cast<UserInfo*>(msg));
				break;
			case Message::SessionSelect:
				// Ignore session select
				break;

			default:
				qDebug() << "unhandled session message type" << int(msg->type);
		}
		delete msg;
		msg = next;
	} while(msg);
#endif
}

#if 0
/**
 * Receive raster data. When joining an empty session, a raster message
 * with all fields zeroed is received.
 * The rasterReceived signal is emitted every time data is received.
 * @param msg Raster data message
 */
void SessionState::handleRaster(const protocol::Raster *msg)
{
	if(msg->size==0) {
		// Special case, zero size raster
		emit rasterReceived(100);
		flushDrawBuffer();
	} else {
		if(msg->offset==0) {
			// Raster data has just started or has been restarted.
			raster_.truncate(0);
		}
		// Note. We make an assumption here that the data is sent in a 
		// sequential manner with no gaps in between.
		raster_.append(QByteArray(msg->data,msg->length));
		if(msg->offset+msg->length<msg->size) {
			emit rasterReceived(99*(msg->offset+msg->length)/msg->size);
		} else {
			emit rasterReceived(100);
			flushDrawBuffer();
		}
	}
}
#endif

#if 0
/**
 * A synchronize request causes the client to start transmitting a copy of
 * the drawingboard as soon as the user stops drawing.
 * @param msg Synchronize message
 */
void SessionState::handleSynchronize(const protocol::Synchronize *msg)
{
	emit syncRequest();
}
#endif

#if 0
/**
 * Client will enter SyncWait state. The board will be locked as soon as
 * the current stroke is finished. The client will respond with Ack/SyncWait
 * when the board is locked. Ack/sync from the server will unlock the board.
 * @param msg SyncWait message
 */
void SessionState::handleSyncWait(const protocol::SyncWait *msg)
{
	emit syncWait();
}
#endif

#if 0
/**
 * Received session events contain information about other users in the
 * session.
 * @param msg SessionEvent message
 */
void SessionState::handleSessionEvent(const protocol::SessionEvent *msg)
{
	User *user = 0;
	if(msg->target != protocol::null_user) {
		if(users_.contains(msg->target)) {
			user = &this->user(msg->target);
		} else {
			qDebug() << "received SessionEvent for user" << int(msg->target)
				<< "who is not part of the session";
			return;
		}
	}

	switch(msg->action) {
		case protocol::SessionEvent::Lock:
			if(user) {
				user->setLocked(true);
				emit userLocked(msg->target, true);
			} else {
				lock_ = true;
				emit sessionLocked(true);
			}
			break;
		case protocol::SessionEvent::Unlock:
			if(user) {
				user->setLocked(false);
				emit userLocked(msg->target, false);
			} else {
				lock_ = false;
				emit sessionLocked(false);
			}
			break;
		case protocol::SessionEvent::Kick:
			emit userKicked(msg->target);
			break;
		case protocol::SessionEvent::Delegate:
			info_.owner = msg->target;
			emit ownerChanged();
			break;
		default:
			qDebug() << "unhandled session event action" << int(msg->action);
	}
}
#endif

/**
 * @param msg ToolInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleToolSelect(protocol::ToolSelect *ts)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(ts);
		return true;
	}
	qreal o1;
	QColor c1 = decodeColor(ts->c1(), o1);
	qreal o0;
	QColor c0 = decodeColor(ts->c0(), o0);
	drawingboard::Brush brush(
			ts->s1(),
			ts->h1()/255.0,
			o1,
			c1,
			ts->spacing());
	brush.setRadius2(ts->s0());
	brush.setColor2(c0);
	brush.setHardness2(ts->h0()/255.0);
	brush.setOpacity2(o0);
	emit toolReceived(ts->user(), brush);
	return false;
}

/**
 * @param msg StrokeInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStroke(protocol::StrokePoint *s)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(s);
		return true;
	}
	emit strokeReceived(
			s->user(),
			drawingboard::Point(
				(qint16)(s->point(0).x),
				(qint16)(s->point(0).y),
				s->point(0).z/255.0
				)
			);
	return false;
}

/**
 * @param msg StrokeEnd message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStrokeEnd(protocol::StrokeEnd *se)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(se);
		return true;
	}
	emit strokeEndReceived(se->user());
	return false;
}

#if 0
/**
 * @param msg chat message
 */
void SessionState::handleChat(const protocol::Chat *msg)
{
	const User *u = 0;
	if(users_.contains(msg->user_id))
		u = &user(msg->user_id);
	
	emit chatMessage(u?u->name():"<unknown>",
			convert::fromUTF(msg->data, msg->length, Utf16_));
}
#endif

/**
 * The drawing command queue is cleared and a signal is emitted for
 * each command. Buffering is disabled for the rest of the session.
 * @post drawing buffer is disabled for the rest of the session
 */
void SessionState::flushDrawBuffer()
{
	bufferdrawing_ = false;
	while(drawbuffer_.isEmpty() == false) {
		protocol::Packet *pk = drawbuffer_.dequeue();
		switch(pk->type()) {
			case protocol::STROKE:
				handleStroke(static_cast<protocol::StrokePoint*>(pk));
				break;
			case protocol::STROKE_END:
				handleStrokeEnd(static_cast<protocol::StrokeEnd*>(pk));
				break;
			case protocol::TOOL_SELECT:
				handleToolSelect(static_cast<protocol::ToolSelect*>(pk));
				break;
			default:
				qFatal("unhandled packet got into the drawing buffer!");
		}
		delete pk;
	}
}

}


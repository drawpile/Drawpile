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

#include <QDebug>
#include <QImage>
#include <memory>

#include "../config.h"

#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "brush.h"
#include "point.h"

#include "../shared/protocol.h"
#include "../shared/protocol.tools.h"
#include "../shared/protocol.flags.h"
#include "../shared/SHA1.h"
#include "../shared/templates.h" // for bswap

namespace network {

/**
 * @param parent parent HostState
 * @param info session information
 */
SessionState::SessionState(HostState *parent, const Session& info)
	: QObject(parent), host_(parent), info_(info), rasteroffset_(0),lock_(false),bufferdrawing_(true)
{
	Q_ASSERT(parent);
	users_[host_->localUser().id()] = User(
			host_->localUser().name(),
			host_->localUser().id(),
			fIsSet(info.mode, protocol::user_mode::Locked),
			this
			);
}

/**
 * @param info new session info
 */
void SessionState::update(const Session& info)
{
	if(info_.maxusers != info.maxusers) {
		emit userLimitChanged(info.maxusers);
	}
	// TODO check for other changes too
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
}

/**
 * Send a SessionSelect message to indicate this session as the one
 * where drawing occurs.
 */
void SessionState::select()
{
	protocol::SessionSelect *msg = new protocol::SessionSelect;
	msg->session_id = info_.id;
	host_->usersessions_[host_->localuser_.id()] = info_.id;
	host_->connection()->send(msg);
}

/**
 * Set the session password.
 * @param password password to set
 */
void SessionState::setPassword(const QString& password)
{
	host_->setPassword(password, info_.id);
}

/**
 * @param l lock status
 * @pre user is session owner
 */
void SessionState::lock(bool l)
{
	protocol::SessionEvent *msg = new protocol::SessionEvent(
			(l ? protocol::SessionEvent::Lock : protocol::SessionEvent::Unlock),
			protocol::null_user,
			0 // aux (unused)
			);
	
	msg->session_id = info_.id;
	
	host_->connection()->send(msg);
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
}

/**
 * @param brush brush info to send
 */
void SessionState::sendToolInfo(const drawingboard::Brush& brush)
{
	const QColor hi = brush.color(1);
	const QColor lo = brush.color(0);
	protocol::ToolInfo *msg = new protocol::ToolInfo(
			protocol::tool_type::Brush,
			protocol::tool_mode::Normal,
			0, // lo color
			0, // hi color
			brush.radius(0),
			brush.radius(1),
			qRound(brush.hardness(0)*255),
			qRound(brush.hardness(1)*255),
			brush.spacing()
			);

	msg->session_id = info_.id;
	msg->lo_color.red = lo.red();
	msg->lo_color.green = lo.green();
	msg->lo_color.blue = lo.blue();
	msg->lo_color.alpha = qRound(brush.opacity(0) * 255);
	msg->hi_color.red = hi.red();
	msg->hi_color.green = hi.green();
	msg->hi_color.blue = hi.blue();
	msg->hi_color.alpha = qRound(brush.opacity(1) * 255);
	host_->connection()->send(msg);
}

/**
 * @param point stroke coordinates to send
 */
void SessionState::sendStrokeInfo(const drawingboard::Point& point)
{
	protocol::StrokeInfo *msg = new protocol::StrokeInfo(
			point.x(),
			point.y(),
			qRound(point.pressure()*255)
			);
	msg->session_id = info_.id;
	host_->connection()->send(msg);
}

void SessionState::sendStrokeEnd()
{
	protocol::StrokeEnd *msg = new protocol::StrokeEnd;
	msg->session_id = info_.id;;
	host_->connection()->send(msg);
}

void SessionState::sendAckSync()
{
	protocol::Acknowledgement *msg = new protocol::Acknowledgement(
			protocol::Message::SyncWait
			);
	msg->session_id = info_.id;
	host_->connection()->send(msg);
}

void SessionState::sendChat(const QString& message)
{
	QByteArray arr = message.toUtf8();
	protocol::Chat *msg = new protocol::Chat(
			arr.length(),
			new char[arr.length()]
			);
	msg->session_id = info_.id;
	memcpy(msg->data,arr.constData(),arr.length());
	host_->connection()->send(msg);
}

/**
 * @param msg Acknowledgement message
 */
void SessionState::handleAck(const protocol::Acknowledgement *msg)
{
	switch(msg->event) {
		case protocol::Message::SyncWait:
			emit syncDone();
		case protocol::Message::SessionSelect:
			// Ignore session select ack
			break;
		case protocol::Message::Raster:
			sendRasterChunk();
			break;
		default:
			qDebug() << "unhandled session ack" << int(msg->event);
			break;
	}
}

/**
 * @param msg UserInfo message
 */
void SessionState::handleUserInfo(const protocol::UserInfo *msg)
{
	switch(msg->event) {
		case protocol::UserInfo::Join:
			if(users_.contains(msg->user_id)) {
				qDebug() << "Got join event for user " << int(msg->user_id)
					<< "who is already in session!";
			} else {
				bool islocked = fIsSet(msg->mode, protocol::user_mode::Locked);
				users_[msg->user_id] = User(msg->name, msg->user_id, islocked, this);
				emit userJoined(msg->user_id);
			}
			break;
		case protocol::UserInfo::Leave:
		case protocol::UserInfo::Disconnect:
		case protocol::UserInfo::BrokenPipe:
		case protocol::UserInfo::TimedOut:
		case protocol::UserInfo::Dropped:
		case protocol::UserInfo::Kicked:
			if(users_.contains(msg->user_id)) {
				emit userLeft(msg->user_id);
				users_.remove(msg->user_id);
				host_->usersessions_.remove(msg->user_id);
			} else {
				qDebug() << "got logout message for user" << int(msg->user_id)
				   << "who is not in session!";
			}
			break;
		default:
			qDebug() << "unhandled user event " << int(msg->event);
			break;
	}
}

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

/**
 * A synchronize request causes the client to start transmitting a copy of
 * the drawingboard as soon as the user stops drawing.
 * @param msg Synchronize message
 */
void SessionState::handleSynchronize(const protocol::Synchronize *msg)
{
	emit syncRequest();
}

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

/**
 * @param msg ToolInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleToolInfo(protocol::ToolInfo *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	drawingboard::Brush brush(
			msg->hi_size,
			msg->hi_hardness/255.0,
			msg->hi_color.alpha/255.0,
			QColor(msg->hi_color.red, msg->hi_color.green, msg->hi_color.blue),
			msg->spacing);
	brush.setRadius2(msg->lo_size);
	brush.setColor2(QColor(msg->lo_color.red, msg->lo_color.green, msg->lo_color.blue));
	brush.setHardness2(msg->lo_hardness/255.0);
	brush.setOpacity2(msg->lo_color.alpha/255.0);
	emit toolReceived(msg->user_id, brush);
	return false;
}

/**
 * @param msg StrokeInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStrokeInfo(protocol::StrokeInfo *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	Q_ASSERT(msg->type == protocol::Message::StrokeInfo);
	emit strokeReceived(
			msg->user_id,
			drawingboard::Point(
				(qint16)(msg->x),
				(qint16)(msg->y),
				msg->pressure/255.0
				)
			);
	return false;
}

/**
 * @param msg StrokeEnd message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStrokeEnd(protocol::StrokeEnd *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	emit strokeEndReceived(msg->user_id);
	return false;
}

/**
 * @param msg chat message
 */
void SessionState::handleChat(const protocol::Chat *msg)
{
	const User *u = 0;
	if(users_.contains(msg->user_id))
		u = &user(msg->user_id);
	QString str = QString::fromUtf8(msg->data, msg->length);
	emit chatMessage(u?u->name():"<unknown>", str);
}

/**
 * Flush the drawing buffer and emit the commands.
 * @post drawing buffer is disabled for the rest of the session
 */
void SessionState::flushDrawBuffer()
{
	bufferdrawing_ = false;
	while(drawbuffer_.isEmpty() == false) {
		protocol::Message *msg = drawbuffer_.dequeue();
		switch(msg->type) {
			case protocol::Message::StrokeInfo:
				handleStrokeInfo(static_cast<protocol::StrokeInfo*>(msg));
				break;
			case protocol::Message::StrokeEnd:
				handleStrokeEnd(static_cast<protocol::StrokeEnd*>(msg));
				break;
			default:
				handleToolInfo(static_cast<protocol::ToolInfo*>(msg));
				break;
		}
		delete msg;
	}
}


}


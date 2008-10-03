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

#include "hoststate.h"
#include "sessionstate.h"
#include "core/brush.h"
#include "core/point.h"

#include "../shared/net/messagequeue.h"
#include "../shared/net/message.h"
#include "../shared/net/stroke.h"
#include "../shared/net/toolselect.h"
#include "../shared/net/binary.h"
#include "../shared/net/annotation.h"

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
	: QObject(parent), host_(parent), info_(info), expectRaster_(0), rasteroffset_(0),bufferdrawing_(true)
{
	Q_ASSERT(parent);
}

/**
 * @param info new session info
 */
void SessionState::update(const Session& info)
{
	Session old = info_;
	info_ = info;
	// Locking warrants its own signal
	if(old.lock() != info.lock())
		emit sessionLocked(info.lock());
	emit boardChanged();
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
	if(raster_.isEmpty()) {
		image = QImage();
	} else {
		QImage img;
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
	QStringList rastermsg;
	rastermsg << "RASTER" << QString::number(raster.length());
	host_->sendPacket(protocol::Message(rastermsg));
	sendRasterChunk();
}

/**
 * Send the next piece of the raster data.
 * Raster data is sent in pieces so we can interleave drawing messages
 * between them. The next piece wont be sent until the server requests it.
 */
void SessionState::sendRasterChunk()
{
	int chunklen = 1024*4; // chunk length is a tradeoff between efficiency and responsiveness.
	if(rasteroffset_ + chunklen > raster_.length())
		chunklen = raster_.length() - rasteroffset_;
	host_->sendPacket(protocol::BinaryChunk(
				raster_.mid(rasteroffset_, chunklen)
			));
	rasteroffset_ += chunklen;
	emit rasterSent(100*rasteroffset_/raster_.length());
	if(rasteroffset_ == raster_.length()) {
		rasteroffset_ = 0;
		releaseRaster();
	}
}

void SessionState::setPassword(const QString& password) {
	QStringList msg;
	msg << "PASSWORD" << password;
	host_->sendPacket(protocol::Message(msg));
}

/**
 * A session lock applies to the entire session, not individual users.
 * @param l lock status
 * @pre user is session owner
 */
void SessionState::lock(bool l)
{
	host_->sendPacket(protocol::Message(l?"LOCK BOARD":"UNLOCK BOARD"));
}

/**
 * Changes the user limit for the session. This doesn't affect current users,
 * only those who are trying to join the session.
 * @param count number of users allowed
 */
void SessionState::setUserLimit(int count)
{
	QStringList msg;
	msg << "USERLIMIT" << QString::number(count);
	host_->sendPacket(protocol::Message(msg));
}

/**
 * @param brush brush info to send
 */
void SessionState::sendToolSelect(const dpcore::Brush& brush)
{
	host_->sendPacket( protocol::ToolSelect(
			host_->localuser_,
			1,
			brush.blendingMode(),
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
void SessionState::sendStrokePoint(const dpcore::Point& point)
{
	host_->sendPacket(protocol::StrokePoint(
				host_->localuser_,
				point.x(),
				point.y(),
				point.xFrac(),
				point.yFrac(),
				qRound(point.pressure()*255)
				)
			);
}

/**
 * Sends a list of stroke points in a single message.
 */
void SessionState::sendAtomicStroke(const QList<dpcore::Point>& points)
{
	protocol::StrokePoint sp(host_->localuser_,
			points[0].x(),
			points[0].y(),
			points[0].xFrac(),
			points[0].yFrac(),
			qRound(points[0].pressure()*255));
	for(int i=1;i<points.size();++i) {
		sp.addPoint(points[i].x(),
				points[i].y(),
				points[i].xFrac(),
				points[i].yFrac(),
				qRound(points[i].pressure()*255));
	}
	host_->sendPacket(sp);
}

void SessionState::sendStrokeEnd()
{
	host_->sendPacket(protocol::StrokeEnd( host_->localuser_ ) );
}

/**
 * This message is sent when the client has locked itself for
 * synchronization.
 */
void SessionState::sendAckSync()
{
	host_->sendPacket(protocol::Message("SYNCREADY"));
}

void SessionState::sendAnnotation(const protocol::Annotation& a)
{
	host_->sendPacket(protocol::Message(a.tokens()));
}

void SessionState::sendRmAnnotation(int id)
{
	QStringList msg;
	msg << "RMANNOTATION" << QString::number(id);
	host_->sendPacket(protocol::Message(msg));
}

void SessionState::sendChat(const QString& message)
{
	QStringList msg;
	msg << "SAY" << message;
	host_->sendPacket(protocol::Message(msg));
}

/**
 * @param tokens message tokens
 */
bool SessionState::handleMessage(const QStringList& tokens)
{
	if(tokens[0] == "SAY") {
		int userid = tokens.at(1).toInt();
		if(hasUser(userid))
			emit chatMessage(user(userid).name(), tokens.at(2));
		else
			qWarning() << "Got chat message from unknown user:" << tokens.at(2);
	} else if(tokens[0] == "USER") {
		updateUser(tokens);
	} else if(tokens[0] == "PART") {
		partUser(tokens);
	} else if(tokens[0] == "SLOCK") {
		emit syncWait();
	} else if(tokens[0] == "SUNLOCK") {
		emit syncDone();
	} else if(tokens[0] == "BOARD") {
		update(Session(tokens));
	} else if(tokens[0] == "RASTER") {
		releaseRaster();
		expectRaster_ = tokens.at(1).toInt();
		if(expectRaster_ == 0)
			emit rasterReceived(100);
	} else if(tokens[0] == "GIVERASTER") {
		emit syncRequest();
	} else if(tokens[0] == "MORE") {
		if(isUploading())
			sendRasterChunk();
		else
			qWarning() << "Got request for more raster data, but we're not uploading anything!";
	} else if(tokens[0] == "ANNOTATE") {
		protocol::Annotation a(tokens);
		if(!a.isValid())
			qWarning() << "Received an invalid annotation";
		else
			emit annotation(a);
	} else if(tokens[0] == "RMANNOTATION") {
		emit rmAnnotation(tokens.at(1).toInt());
	} else
		return false;
	return true;
}

void SessionState::updateUser(const QStringList& tokens) {
	User user(this, tokens);
	bool newuser=true;
	if(users_.contains(user.id())) {
		// Existing user changed
		newuser = false;
		User olduser = users_.value(user.id());
		users_[user.id()] = user;
		if(olduser.locked() != user.locked())
			emit userLocked(user.id(), user.locked());
	} else {
		users_[user.id()] = user;
	}
	// The session joining is complete when we get our own user ID
	// for the first time.
	if(host_->localuser_<0 && user.name() == host_->username_)
		host_->sessionJoinDone(user.id());

	if(newuser) {
		emit userJoined(user.id());
		if(user.locked())
			emit userLocked(user.id(), true);
	}
}

/**
 * Inform the rest of the program that a user has just left and
 * remove it.
 */
void SessionState::partUser(const QStringList& tokens) {
	int id = tokens.at(1).toInt();
	if(users_.contains(id)) {
		emit userLeft(id);
		users_.remove(id);
	} else {
		qWarning() << "Got PART message for a user (" << id << ") who isn't part of the session.";
	}
}

/**
 * Receive raster data.
 * The rasterReceived signal is emitted every time data is received.
 * @param msg Raster data message
 */
bool SessionState::handleBinaryChunk(protocol::BinaryChunk *bc)
{
	if(expectRaster_>0) {
		raster_.append(bc->data());
		if(raster_.size()<expectRaster_) {
			emit rasterReceived(99*(raster_.size())/expectRaster_);
		} else {
			emit rasterReceived(100);
			flushDrawBuffer();
			expectRaster_ = 0;
		}
		return true;
	}
	return false;
}

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
	dpcore::Brush brush(
			ts->s1(),
			ts->h1()/255.0,
			o1,
			c1,
			ts->spacing());
	brush.setRadius2(ts->s0());
	brush.setColor2(c0);
	brush.setHardness2(ts->h0()/255.0);
	brush.setOpacity2(o0);
	brush.setBlendingMode(ts->mode());
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
	for(int p=0;p<s->points();++p) {
		emit strokeReceived(
				s->user(),
				dpcore::Point(QPointF(
					(qint16)(s->point(p).x) + s->point(p).xf,
					(qint16)(s->point(p).y) + s->point(p).yf),
					s->point(p).z/255.0
					)
				);
	}
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

/**
 * @param msg chat message
 */
void SessionState::handleChat(const QStringList& tokens)
{
	if(tokens.size()!=3) {
		qWarning() << "Received invalid chat message";
	} else {
		const User *u = 0;
		if(users_.contains(tokens[1].toInt()))
			u = &users_[tokens[1].toInt()];
		
		emit chatMessage(u?u->name():"<unknown>", tokens[2]);
	}
}

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


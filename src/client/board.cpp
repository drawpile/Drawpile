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
#include "board.h"
#include "layer.h"
#include "user.h"
#include "boardeditor.h"
#include "preview.h"
#include "interfaces.h"

namespace drawingboard {

Board::Board(QObject *parent, interface::BrushSource *brush, interface::ColorSource *color)
	: QGraphicsScene(parent), image_(0),localuser_(-1), brushsrc_(brush), colorsrc_(color)
{
	setItemIndexMethod(NoIndex);
}

Board::~Board()
{
	clearUsers();
}

/**
 * A new image is created with the given size and initialized to a solid color
 * @param size size of the drawing board
 * @param background background color
 */
void Board::initBoard(const QSize& size, const QColor& background)
{

	QImage image(size, QImage::Format_RGB32);
	image.fill(background.rgb());

	setSceneRect(0,0,size.width(), size.height());
	delete image_;
	image_ = new Layer(image,0,this);
	foreach(User *u, users_)
		u->setLayer(image_);
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
	previewstarted_ = false;
}

/**
 * An existing image is used as a base.
 * @param image image to use
 */
void Board::initBoard(QImage image)
{
	setSceneRect(0,0,image.width(), image.height());
	delete image_;
	image_ = new Layer(image.convertToFormat(QImage::Format_RGB32), 0, this);
	foreach(User *u, users_)
		u->setLayer(image_);
	QList<QRectF> regions;
	regions.append(sceneRect());
	emit changed(regions);
	previewstarted_ = false;
}

/**
 * A single (local) user is needed to modify the drawing board, be sure
 * to create one after deleting everyone.
 */
void Board::clearUsers()
{
	commitPreviews();
	foreach(User *u, users_) {
		delete u;
	}
	users_.clear();
	localuser_ = -1;
}

/**
 * @param id user id
 */
void Board::addUser(int id)
{
	User *user = new User(id);
	user->setLayer(image_);
	users_[id] = user;
}

/**
 * Designates one user as the local user
 * @param id user id
 * @pre id must have been added with addUser
 */
void Board::setLocalUser(int id)
{
	Q_ASSERT(users_.contains(id));
	localuser_ = id;
}

/**
 * @param id user id
 * @pre id must have been added with addUser
 */
void Board::removeUser(int id)
{
	Q_ASSERT(users_.contains(id));
	if(id == localuser_)
		commitPreviews();
	delete users_.take(id);
}

/**
 * @return board contents
 */
QImage Board::image() const
{
	return image_->image();
}

/**
 * @return board width
 */
int Board::width() const {
	return int(image_->boundingRect().width());
}

/**
 * @return board height
 */
int Board::height() const {
	return int(image_->boundingRect().height());
}

/**
 * Returns a BoardEditor for modifying the drawing board either
 * directly or over the network.
 * @param session which network session the editor works over. If 0, a local editor is returned
 */
BoardEditor *Board::getEditor(network::SessionState *session)
{
	Q_ASSERT(localuser_ != -1);
	User *user = users_.value(localuser_);
	if(session)
		return new RemoteBoardEditor(this, user, session, brushsrc_, colorsrc_);
	else
		return new LocalBoardEditor(this, user, brushsrc_, colorsrc_);
}

/**
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param point stroke point
 */
void Board::addPreview(const Point& point)
{
	Q_ASSERT(localuser_ != -1);
	User *user = users_.value(localuser_);

	Preview *pre;
	if(previewcache_.isEmpty())
		pre = new Preview(user->layer(), this);
	else
		pre = previewcache_.dequeue();
	if(previewstarted_) {
		pre->previewLine(lastpreview_, point, brushsrc_->getBrush());
	} else {
		pre->previewLine(point, point, brushsrc_->getBrush());
		previewstarted_ = true;
	}
	lastpreview_ = point;
	previews_.enqueue(pre);
}

/**
 */
void Board::endPreview()
{
	previewstarted_ = false;
}

/**
 * This is called when leaving a session. All pending preview strokes
 * are immediately drawn on the board.
 */
void Board::commitPreviews()
{
	Point lastpoint(-1,-1,0);
	while(previews_.isEmpty()==false) {
		Preview *p = previews_.dequeue();
		if(p->from() != lastpoint)
			image_->drawPoint(p->from(), p->brush());
		else
			image_->drawLine(p->from(), p->to(), p->brush());
		lastpoint = p->to();
		delete p;
	}

	while(previewcache_.isEmpty()==false)
		delete previewcache_.dequeue();
	previewcache_.clear();
}

/**
 * Remove all preview strokes from the board
 */
void Board::flushPreviews()
{
	while(previews_.isEmpty()==false) {
		Preview *p = previews_.dequeue();
		p->hide();
		previewcache_.enqueue(p);
	}
}

/**
 * @param user user id
 * @param brush brush to use
 * @pre user must exist
 */
void Board::userSetTool(int user, const Brush& brush)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->setBrush(brush);
}

/**
 * @param user user id
 * @param point coordinates
 * @pre user must exist
 */
void Board::userStroke(int user, const Point& point)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->addStroke(point);

	if(user == localuser_ && previews_.isEmpty() == false) {
		Preview *pre = previews_.dequeue();
		pre->hide();
		previewcache_.enqueue(pre);
	}
}

/**
 * @param user user id
 * @pre user must exist
 */
void Board::userEndStroke(int user)
{
	Q_ASSERT(users_.contains(user));
	users_.value(user)->endStroke();
}

}


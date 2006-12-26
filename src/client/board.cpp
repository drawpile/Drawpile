/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

#include <QGraphicsEllipseItem>
#include <QPainter>

#include "board.h"
#include "layer.h"
#include "user.h"
#include "boardeditor.h"

namespace drawingboard {

Board::Board(QObject *parent)
	: QGraphicsScene(parent), image_(0)
{
	setItemIndexMethod(NoIndex);
}

Board::~Board()
{
	foreach(User *u, users_) {
		delete u;
	}
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
 * @param id user id
 */
void Board::removeUser(int id)
{
	delete users_.take(id);
}

/**
 * File format is deduced from the filename.
 * @param filename filename
 * @return false on failure.
 */
bool Board::save(QString filename)
{
	return image_->image().save(filename);
}

/**
 * @param device QIODevice to which the image is saved
 * @param format image format. Eg. "PNG"
 * @param quality image quality. Range is [0..100]
 * @return false on failure.
 */
bool Board::save(QIODevice *device, const char *format, int quality)
{
	return image_->image().save(device, format, quality);
}

/**
 * Returns a BoardEditor for modifying the drawing board either
 * directly or over the network.
 * @param local if true, get an editor for modifying the board directly.
 */
BoardEditor *Board::getEditor(bool local)
{
	BoardEditor *editor;
	editor = new LocalBoardEditor(this, users_.value(0)); // TODO local user
	return editor;
}

#if 0
/**
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param x initial stroke coordinate
 * @param y initial stroke coordinate
 * @param pressure stroke pressure
 */
void Board::previewBegin(int x,int y, qreal pressure)
{
}

/**
 * @param x stroke x coordinate
 * @param y stroke y coordinate
 * @param pressure stroke pressure
 */
void Board::previewMotion(int x,int y, qreal pressure)
{
}

/**
 */
void Board::previewEnd()
{
}
#endif

/**
 * @param user user id
 * @param brush brush to use
 */
void Board::userSetTool(int user, const Brush& brush)
{
	if(users_.contains(user))
		users_.value(user)->setBrush(brush);
}

/**
 * @param user user id
 * @param point coordinates
 */
void Board::userStroke(int user, const Point& point)
{
	if(users_.contains(user)) {
		User *u = users_.value(user);
		u->addStroke(point);
	}
}

/**
 * @param user user id
 */
void Board::userEndStroke(int user)
{
	if(users_.contains(user)) {
		User *u = users_.value(user);
		u->endStroke();
	}
}

}


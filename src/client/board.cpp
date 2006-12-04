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

namespace drawingboard {

Board::Board(QObject *parent)
	: QGraphicsScene(parent)
{
	setItemIndexMethod(NoIndex);
	outline_ = new QGraphicsEllipseItem(0,this);
	outline_->hide();
	outline_->setZValue(9999);
	outline_->setPen(Qt::DotLine);
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
	image_ = new Layer(image,0,this);
}

/**
 * An existing image is used as a base.
 * @param image image to use
 */
void Board::initBoard(QImage image)
{
	setSceneRect(0,0,image.width(), image.height());
	image_ = new Layer(image, 0, this);
}

/**
 * @param id user id
 */
void Board::addUser(int id)
{
	users_[id] = new User(id);
}

/**
 * @param id user id
 */
void Board::removeUser(int id)
{
	users_.remove(id);
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
 * @param x x coordinate
 * @param y y coordinate
 * @return color at coordinates
 */
QColor Board::colorAt(int x,int y)
{
	return QColor(image_->image().pixel(0,0));
}

/**
 * Move cursor outline to the specified location, set its size and show it.
 * The outline will not be shown if its width is smaller than 2.
 * @param x x coordinate
 * @param y y coordinate
 * @param radius brush radius
 */
void Board::showCursorOutline(const QPoint& pos, int radius)
{
	if(radius>0) {
		outline_->setPos(pos);
		outline_->setRect(-radius,-radius,radius*2,radius*2);
		outline_->show();
	}
}

/**
 * @param x x coordinate
 * @param y y coordinate
 */
void Board::moveCursorOutline(const QPoint& pos)
{
	outline_->setPos(pos);
}

/**
 */
void Board::hideCursorOutline()
{
	outline_->hide();
}

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

/**
 * The new stroke is assigned to the indicated user.
 * @param user user id
 * @param x x coordinate
 * @param y y coordinate
 * @param pressure pen pressure
 * @param brush brush to use
 */
void Board::strokeBegin(int user, int x, int y, qreal pressure, const Brush& brush)
{
	if(users_.contains(user)) {
		User *u = users_.value(user);
		u->brush() = brush;
		u->strokeBegin(image_, x, y, pressure);
	}
}

/*
 * @param user user id
 * @param x x coordinate
 * @param y y coordinate
 * @param pressure pen pressure
 */
void Board::strokeMotion(int user, int x, int y, qreal pressure)
{
	if(users_.contains(user)) {
		User *u = users_.value(user);
		u->strokeMotion(x,y,pressure);
	}
}

/*
 * @param user user id
 */
void Board::strokeEnd(int user)
{
	if(users_.contains(user)) {
		User *u = users_.value(user);
		u->strokeEnd();
	}
}

}


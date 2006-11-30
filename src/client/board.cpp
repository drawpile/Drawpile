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
 * A new pixmap is created with the given size and initialized to a solid color
 * @param size size of the drawing board
 * @param background background color
 */
void Board::initBoard(const QSize& size, const QColor& background)
{

	QPixmap pixmap(size);
	pixmap.fill(background);

	setSceneRect(0,0,size.width(), size.height());
	image_ = new Layer(pixmap,0,this);
}

/**
 * An existing pixmap is used as a base.
 * @param pixmap pixmap to use
 */
void Board::initBoard(QPixmap pixmap)
{
	setSceneRect(0,0,pixmap.width(), pixmap.height());
	image_ = new Layer(pixmap, 0, this);
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
	return image_->pixmap().save(filename);
}

/**
 * @param device QIODevice to which the image is saved
 * @param format image format. Eg. "PNG"
 * @param quality image quality. Range is [0..100]
 * @return false on failure.
 */
bool Board::save(QIODevice *device, const char *format, int quality)
{
	return image_->pixmap().save(device, format, quality);
}

/**
 * @param x x coordinate
 * @param y y coordinate
 * @return color at coordinates
 */
QColor Board::colorAt(int x,int y)
{
	// There is no direct nice way of getting pixel values from QPixmaps
	QImage pixel = image_->pixmap().copy(x,y,1,1).toImage();
	return QColor(pixel.pixel(0,0));
}

/**
 * Move cursor outline to the specified location, set its size and show it.
 * The outline will not be shown if its width is smaller than 2.
 * @param x x coordinate
 * @param y y coordinate
 * @param size brush diameter
 */
void Board::showCursorOutline(const QPoint& pos, int size)
{
	if(size>1) {
		outline_->setPos(pos);
		outline_->setRect(-size*0.5,-size*0.5,size,size);
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


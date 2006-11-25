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

#include <QGraphicsPixmapItem>
#include <QPainter>

#include "board.h"
#include "layer.h"
#include "brush.h"

namespace drawingboard {

Board::Board(QObject *parent)
	: QGraphicsScene(parent)
{
	//setItemIndexMethod(NoIndex);
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
 * Preview strokes are used to give immediate feedback to the user,
 * before the stroke info messages have completed their roundtrip
 * through the server.
 * @param x initial stroke coordinate
 * @param y initial stroke coordinate
 * @param pressure stroke pressure
 */
void Board::beginPreview(int x,int y, qreal pressure)
{
	plastx_ = x;
	plasty_ = y;
}

/**
 * @param x initial stroke coordinate
 * @param y initial stroke coordinate
 * @param pressure stroke pressure
 */
void Board::strokePreview(int x,int y, qreal pressure)
{
#if 0
	new QGraphicsLineItem(plastx_,plasty_,x,y,0,this);
#else
	image_->drawLine(QLine(plastx_,plasty_,x,y),brush_);
#endif
	plastx_ = x;
	plasty_ = y;
}

void Board::endPreview()
{
}

}


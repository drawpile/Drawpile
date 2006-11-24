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

#include "board.h"

namespace drawingboard {

Board::Board(QObject *parent)
	: QGraphicsScene(parent)
{
}

void Board::initBoard(const QSize& size, const QColor& background)
{
	pixmap_ = QPixmap(size);
	pixmap_.fill(background);

	setSceneRect(0,0,size.width(), size.height());
	new QGraphicsPixmapItem(pixmap_,0,this);
}

void Board::initBoard(QPixmap pixmap)
{
	pixmap_ = pixmap;

	setSceneRect(0,0,pixmap_.width(), pixmap_.height());
	new QGraphicsPixmapItem(pixmap_,0,this);
}

void Board::beginPreview(int x,int y, qreal pressure)
{
	plastx_ = x;
	plasty_ = y;
}

void Board::strokePreview(int x,int y, qreal pressure)
{
	new QGraphicsLineItem(plastx_,plasty_,x,y,0,this);
	plastx_ = x;
	plasty_ = y;
}

void Board::endPreview()
{
}

}


/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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

#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include "canvasitem.h"

#include "core/layerstack.h"

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(QGraphicsItem *parent)
	: QGraphicsObject(parent)
{
	_image = new dpcore::LayerStack(this);
	connect(_image, SIGNAL(areaChanged(QRect)), this, SLOT(refreshImage(QRect)));
	connect(_image, SIGNAL(resized()), this, SLOT(canvasResize()));
}

void CanvasItem::refreshImage(const QRect &area)
{
	update(area.adjusted(-2, -2, 2, 2));
}

QRectF CanvasItem::boundingRect() const
{
	return QRectF(0,0, _image->width(), _image->height());
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	QRectF exposed = option->exposedRect.adjusted(-1, -1, 1, 1);
	exposed &= QRectF(0,0,_image->width(),_image->height());
	_image->paint(exposed, painter);
}

void CanvasItem::canvasResize()
{
	prepareGeometryChange();
}

}


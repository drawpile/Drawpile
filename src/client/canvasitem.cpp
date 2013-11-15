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

#include <cmath>

#include <QPainter>
#include <QGraphicsScene>
#include <QStyleOptionGraphicsItem>

#include "canvasitem.h"

#include "core/point.h"
#include "core/brush.h"
#include "core/layerstack.h"
#include "core/layer.h"

namespace drawingboard {

/**
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(QGraphicsItem *parent)
	: QGraphicsItem(parent), image_(0)
{
}

/**
 * @param image image to use
 * @param parent use another QGraphicsItem as a parent
 * @param scene the picture to which this layer belongs to
 */
CanvasItem::CanvasItem(const QImage& image, QGraphicsItem *parent)
	: QGraphicsItem(parent), image_(0)
{
	setImage(image);
}

/**
 * @param layers the layer stack for this board item
 */
CanvasItem::CanvasItem(dpcore::LayerStack *layers, QGraphicsItem *parent)
	: QGraphicsItem(parent)
{
	prepareGeometryChange();
	image_ = layers;
}

CanvasItem::~CanvasItem() {
	delete image_;
}

/**
 * @param image image to use
 */
void CanvasItem::setImage(const QImage& image)
{
	Q_ASSERT(image.format() == QImage::Format_RGB32 || image.format() == QImage::Format_ARGB32);
	prepareGeometryChange();
	delete image_;
	image_ = new dpcore::LayerStack();
	image_->addLayer(dpcore::LayerStack::tr("Background"), image);
}

/**
 * The brush pixmap is drawn at each point between point1 and point2.
 * Pressure values are interpolated between the points.
 * First pixel is not drawn. This is done on purpose, as drawLine is usually
 * used to draw multiple joined lines.
 *
 * @param layer ID of the layer on which to draw
 * @param point1 start coordinates
 * @param point2 end coordinates
 * @param brush brush to draw with
 * @param distance total drawn line length
 *
 * @todo delta pressure(?)
 */
void CanvasItem::drawLine(int layer, const dpcore::Point& point1, const dpcore::Point& point2, const dpcore::Brush& brush,qreal &distance)
{
	image_->getLayer(layer)->drawLine(brush, point1, point2, distance);
	// Update screen
	int rad = brush.radius(point1.pressure());
	update(QRect(point1, point2).normalized().adjusted(-rad-2,-rad-2,rad+2,rad+2));
}

/**
 * @param layer ID of the layer on which to draw
 * @param point coordinates
 * @param brush brush to use
 */
void CanvasItem::drawPoint(int layer, const dpcore::Point& point, const dpcore::Brush& brush)
{
	int r = brush.radius(point.pressure());
	image_->getLayer(layer)->dab(brush, point);
	update(point.x()-r-2,point.y()-r-2,r*2+4,r*2+4);
}

QRectF CanvasItem::boundingRect() const
{
	return QRectF(0,0, image_->width(), image_->height());
}

void CanvasItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
	 QWidget *)
{
	QRectF exposed = option->exposedRect.adjusted(-1, -1, 1, 1);
	exposed &= QRectF(0,0,image_->width(),image_->height());
	image_->paint(exposed, painter);
}

}


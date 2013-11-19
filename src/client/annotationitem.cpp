/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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

#include "annotationitem.h"

namespace drawingboard {

AnnotationItem::AnnotationItem(int id, QGraphicsItem *parent)
	: QGraphicsItem(parent),
	  id_(id),
	  bgcol_(Qt::transparent),
	  _highlight(false), _showborder(false)
{
}

void AnnotationItem::growTopLeft(qreal x, qreal y)
{
	if(_size.width() - x <= 0) x = 0;
	if(_size.height() - y <= 0) y = 0;
	prepareGeometryChange();
	moveBy(x, y);
	_size = QSizeF(_size.width() - x, _size.height() - y);
}

void AnnotationItem::growBottomRight(qreal x, qreal y)
{
	if(_size.width() + x <= 0) x = 0;
	if(_size.height() + y <= 0) y = 0;
	prepareGeometryChange();
	_size = QSizeF(_size.width() + x, _size.height() + y);
}

/**
 * Note. Assumes point is inside the text box.
 */
AnnotationItem::Handle AnnotationItem::handleAt(const QPointF point)
{
	if(point.x() < HANDLE && point.y() < HANDLE)
		return RS_TOPLEFT;
	else if(point.x() > _size.width()-HANDLE && point.y() > _size.height()-HANDLE)
		return RS_BOTTOMRIGHT;
	return TRANSLATE;

}

/**
 * Highlight is used to indicate the selected annotation.
 * @param hl
 */
void AnnotationItem::setHighlight(bool hl)
{
	bool old = _highlight;
	_highlight = hl;
	if(hl != old)
		update();
}

/**
 * Border is normally drawn when the annotation is highlighted or has no text.
 * The border is forced on when the annotation edit tool is selected.
 */
void AnnotationItem::setShowBorder(bool show)
{
	bool old = _showborder;
	_showborder = show;
	if(show != old)
		update();
}

void AnnotationItem::setGeometry(const QRect &rect)
{
	prepareGeometryChange();
	setPos(rect.topLeft());
	_size = rect.size();
}

QRect AnnotationItem::geometry() const
{
	return QRect(pos().toPoint(), _size.toSize());
}

QRectF AnnotationItem::boundingRect() const
{
	return QRectF(QPointF(), _size);
}

void AnnotationItem::setBackgroundColor(const QColor &color)
{
	bgcol_ = color;
	update();
}

void AnnotationItem::setText(const QString &text)
{
	_text.setHtml(text);
	update();
}

/**
 * Render the annotation using a painter.
 * @param painter painter to render with
 * @param rect rectangle to which the annotation is rendered
 */
void AnnotationItem::render(QPainter *painter, const QRectF& rect)
{
	painter->fillRect(rect, bgcol_);
	_text.setTextWidth(_size.width());
	_text.drawContents(painter, rect);
}

void AnnotationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	QRectF rect(QPointF(), _size);
	render(painter, rect);

	if(_showborder || _text.isEmpty()) {
		QColor border = Qt::red;// TODO
		border.setAlpha(255);

		QPen bpen(_highlight && _showborder ? Qt::DashLine : Qt::DotLine);
		bpen.setColor(border);
		painter->setPen(bpen);
		painter->drawRect(rect);

		// Draw resizing handles
		if(_highlight) {
			painter->setClipRect(QRectF(QPointF(), _size));
			painter->setPen(border);
			painter->setBrush(border);

			QPointF triangle[3] = {QPointF(0,0), QPointF(HANDLE,0), QPointF(0,HANDLE)};
			painter->drawConvexPolygon(triangle, 3);
			triangle[0] = QPointF(_size.width()-HANDLE, _size.height());
			triangle[1] = QPointF(_size.width(), _size.height());
			triangle[2] = QPointF(_size.width(),_size.height()-HANDLE);
			painter->drawConvexPolygon(triangle, 3);
		}
	}
}
#if 0
/**
 * The annotation will be rendered on the new layer with an offset
 * so it can be merged simply by compositing whole tiles.
 * @param x this will be set to the tile x index
 * @param y this will be set to the tile y index
 */
dpcore::Layer *AnnotationItem::toLayer(int *x, int *y)
{
	using namespace dpcore;
	int xi = int(pos().x()/Tile::SIZE);
	int yi = int(pos().y()/Tile::SIZE);
	QPoint offset(int(pos().x() - xi*Tile::SIZE),
			int(pos().y() - yi*Tile::SIZE));
	QImage img(offset.x() + int(_size.width()),
			int(offset.y() + _size.height()), QImage::Format_ARGB32);
	img.fill(0);
	QPainter painter(&img);
	render(&painter, QRectF(offset, _size));
	if(x)
		*x = xi;
	if(y)
		*y = yi;
	return new Layer(0, -1, "", img);
	return 0;
}
#endif

}


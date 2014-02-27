/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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

#include <QApplication>
#include <QPalette>
#include <QPainter>

#include "core/annotation.h"
#include "core/layerstack.h"
#include "scene/annotationitem.h"

namespace drawingboard {

AnnotationItem::AnnotationItem(int id, paintcore::LayerStack *image, QGraphicsItem *parent)
	: QGraphicsObject(parent), _id(id), _image(image), _highlight(false), _showborder(false)
{
}

/**
 * Note. Assumes point is inside the text box.
 */
AnnotationItem::Handle AnnotationItem::handleAt(const QPoint &point) const
{
	if(!_rect.contains(point))
		return OUTSIDE;

	QPoint p = point - _rect.topLeft().toPoint();

	if(p.x() < HANDLE) {
		if(p.y() < HANDLE)
			return RS_TOPLEFT;
		else if(p.y() > _rect.height()-HANDLE)
			return RS_BOTTOMLEFT;
		return RS_LEFT;
	} else if(p.x() > _rect.width() - HANDLE) {
		if(p.y() < HANDLE)
			return RS_TOPRIGHT;
		else if(p.y() > _rect.height()-HANDLE)
			return RS_BOTTOMRIGHT;
		return RS_RIGHT;
	} else if(p.y() < HANDLE)
		return RS_TOP;
	else if(p.y() > _rect.height()-HANDLE)
		return RS_BOTTOM;

	return TRANSLATE;
}

void AnnotationItem::adjustGeometry(Handle handle, const QPoint &delta)
{
	prepareGeometryChange();
	switch(handle) {
	case OUTSIDE: return;
	case TRANSLATE: _rect.translate(delta); break;
	case RS_TOPLEFT: _rect.adjust(delta.x(), delta.y(), 0, 0); break;
	case RS_TOPRIGHT: _rect.adjust(0, delta.y(), delta.x(), 0); break;
	case RS_BOTTOMRIGHT: _rect.adjust(0, 0, delta.x(), delta.y()); break;
	case RS_BOTTOMLEFT: _rect.adjust(delta.x(), 0, 0, delta.y()); break;
	case RS_TOP: _rect.adjust(0, delta.y(), 0, 0); break;
	case RS_RIGHT: _rect.adjust(0, 0, delta.x(), 0); break;
	case RS_BOTTOM: _rect.adjust(0, 0, 0, delta.y()); break;
	case RS_LEFT: _rect.adjust(delta.x(), 0, 0, 0); break;
	}

	if(_rect.left() > _rect.right() || _rect.top() > _rect.bottom())
		_rect = _rect.normalized();
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

const paintcore::Annotation *AnnotationItem::getAnnotation() const
{
	return _image->getAnnotation(_id);
}

void AnnotationItem::refresh()
{
	paintcore::Annotation *a = _image->getAnnotation(_id);
	Q_ASSERT(a);
	if(!a)
		return;

	prepareGeometryChange();
	_rect = a->rect();
	update();
}

QRectF AnnotationItem::boundingRect() const
{
	return _rect;
}

namespace {
void drawTriangle(QPainter *painter, AnnotationItem::Handle dir, const QPointF offset) {
	QPointF tri[3];
	tri[0] = offset;
	switch(dir) {
	case AnnotationItem::OUTSIDE:
	case AnnotationItem::TRANSLATE: return;
	case AnnotationItem::RS_TOPLEFT: tri[1] = {1, 0}; tri[2] = {0, 1}; break;
	case AnnotationItem::RS_TOPRIGHT: tri[1] = {0, 1}; tri[2] = {-1, 0}; break;
	case AnnotationItem::RS_BOTTOMRIGHT: tri[1] = {-1, 0}; tri[2] = {0, -1}; break;
	case AnnotationItem::RS_BOTTOMLEFT: tri[1] = {0, -1}; tri[2] = {1, 0}; break;

	case AnnotationItem::RS_TOP: tri[1] = {0.5, 0.5}; tri[2] = {-0.5, 0.5}; break;
	case AnnotationItem::RS_RIGHT: tri[1] = {-0.5, 0.5}; tri[2] = {-0.5, -0.5}; break;
	case AnnotationItem::RS_BOTTOM: tri[1] = {-0.5, -0.5}; tri[2] = {0.5, -0.5}; break;
	case AnnotationItem::RS_LEFT: tri[1] = {0.5, -0.5}; tri[2] = {0.5, 0.5}; break;
	}

	tri[1] = tri[1] * 8 + offset;
	tri[2] = tri[2] * 8 + offset;

	painter->drawConvexPolygon(tri, 3);
}
}

void AnnotationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	const paintcore::Annotation *state = _image->getAnnotation(_id);
	Q_ASSERT(state);
	if(!state)
		return;

	painter->save();
	painter->setClipRect(boundingRect().adjusted(0, 0, 1, 1));

	state->paint(painter, _rect);

	if(_showborder || state->isEmpty()) {
		QColor border = QApplication::palette().color(QPalette::Highlight);
		border.setAlpha(255);

		QPen bpen(_highlight && _showborder ? Qt::DashLine : Qt::DotLine);
		bpen.setCosmetic(true);
		bpen.setColor(border);
		painter->setPen(bpen);
		painter->drawRect(_rect);

		// Draw resizing handles
		if(_highlight) {
			QPen pen(border);
			pen.setCosmetic(true);
			painter->setPen(pen);

			drawTriangle(painter, RS_TOPLEFT, _rect.topLeft() + QPointF(2, 2));
			drawTriangle(painter, RS_TOP, _rect.topLeft() + QPointF(_rect.width()/ 2 + 2, 2));
			drawTriangle(painter, RS_TOPRIGHT, _rect.topRight() + QPointF(-2, 2));

			drawTriangle(painter, RS_LEFT, _rect.center() - QPointF(_rect.width()/2 - 2, 0));
			drawTriangle(painter, RS_RIGHT, _rect.center() + QPointF(_rect.width()/2 - 2, 0));

			drawTriangle(painter, RS_BOTTOMLEFT, _rect.bottomLeft() + QPointF(2, -2));
			drawTriangle(painter, RS_BOTTOM, _rect.bottomLeft() + QPointF(_rect.width()/ 2 + 2, -2));
			drawTriangle(painter, RS_BOTTOMRIGHT, _rect.bottomRight() + QPointF(-2, -2));
		}
	}
	painter->restore();
}

}


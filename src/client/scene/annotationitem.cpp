/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
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
AnnotationItem::Handle AnnotationItem::handleAt(const QPoint &point, float zoom) const
{
	const qreal H = HANDLE / (zoom/100.0f);

	const QRectF R = _rect.adjusted(-H/2, -H/2, H/2, H/2);

	if(!R.contains(point))
		return OUTSIDE;

	QPointF p = point - R.topLeft();

	if(p.x() < H) {
		if(p.y() < H)
			return RS_TOPLEFT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMLEFT;
		return RS_LEFT;
	} else if(p.x() > R.width() - H) {
		if(p.y() < H)
			return RS_TOPRIGHT;
		else if(p.y() > R.height()-H)
			return RS_BOTTOMRIGHT;
		return RS_RIGHT;
	} else if(p.y() < H)
		return RS_TOP;
	else if(p.y() > R.height()-H)
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

bool AnnotationItem::isChanged() const
{
	paintcore::Annotation *a = _image->getAnnotation(_id);
	Q_ASSERT(a);
	if(!a)
		return false;
	return _rect != a->rect();
}

QRectF AnnotationItem::boundingRect() const
{
	return _rect.adjusted(-HANDLE/2, -HANDLE/2, HANDLE/2, HANDLE/2);
}

void AnnotationItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	const qreal devicePixelRatio = qApp->devicePixelRatio();

	const paintcore::Annotation *state = _image->getAnnotation(_id);
	Q_ASSERT(state);
	if(!state)
		return;

	painter->save();
	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	state->paint(painter, _rect);

	if(_showborder || state->isEmpty()) {
		QColor border = QApplication::palette().color(QPalette::Highlight);
		border.setAlpha(255);

		QPen bpen(_highlight && _showborder ? Qt::DashLine : Qt::DotLine);
		bpen.setWidth(devicePixelRatio);
		bpen.setCosmetic(true);
		bpen.setColor(border);
		painter->setPen(bpen);
		painter->drawRect(_rect);

		// Draw resizing handles
		if(_highlight) {
			QPen pen(border);
			pen.setCosmetic(true);
			pen.setWidth(HANDLE);
			painter->setPen(pen);
			painter->drawPoint(_rect.topLeft());
			painter->drawPoint(_rect.topLeft() + QPointF(_rect.width()/2, 0));
			painter->drawPoint(_rect.topRight());

			painter->drawPoint(_rect.topLeft() + QPointF(0, _rect.height()/2));
			painter->drawPoint(_rect.topRight() + QPointF(0, _rect.height()/2));

			painter->drawPoint(_rect.bottomLeft());
			painter->drawPoint(_rect.bottomLeft() + QPointF(_rect.width()/2, 0));
			painter->drawPoint(_rect.bottomRight());
		}
	}

	painter->restore();
}

}


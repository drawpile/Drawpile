/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include "selectionitem.h"

namespace drawingboard {

SelectionItem::SelectionItem(QGraphicsItem *parent)
	: QGraphicsObject(parent), _marchingants(0)
{
	startTimer(100);
}

void SelectionItem::setRect(const QRect &rect)
{
	prepareGeometryChange();
	_rect = rect;
}

SelectionItem::Handle SelectionItem::handleAt(const QPoint &point)
{
	if(!_rect.contains(point))
		return OUTSIDE;

	QPoint p = point - _rect.topLeft();

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


void SelectionItem::adjustGeometry(Handle handle, const QPoint &delta)
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

void SelectionItem::setPasteImage(const QImage &image)
{
	_pasteimg = image;
	update();
}

QRectF SelectionItem::boundingRect() const
{
	return _rect;
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *)
{

	if(!_pasteimg.isNull()) {
		painter->drawImage(_rect, _pasteimg);
	}

	QPen pen;
	pen.setColor(Qt::white);
	pen.setStyle(Qt::DashLine);
	pen.setDashOffset(_marchingants);
	pen.setCosmetic(true);
	painter->setPen(pen);
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	//painter->setBrush(d->brush);
	painter->drawRect(_rect);
}

void SelectionItem::timerEvent(QTimerEvent *e)
{
	_marchingants += 1;
	update();
}

}

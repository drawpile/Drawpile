/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include "selectionitem.h"
#include "net/client.h"

#include <QPainter>
#include <QGraphicsScene>

namespace drawingboard {

SelectionItem::SelectionItem(QGraphicsItem *parent)
	: QGraphicsItem(parent), _marchingants(0)
{
}

void SelectionItem::setRect(const QRect &rect)
{
	prepareGeometryChange();
	_rect = rect;
}

SelectionItem::Handle SelectionItem::handleAt(const QPoint &point) const
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

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
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

void SelectionItem::marchingAnts()
{
	_marchingants += 1;
	update();
}

void SelectionItem::pasteToCanvas(net::Client *client, int layer) const
{
	if(_pasteimg.isNull())
		return;

	QImage image = _pasteimg;
	if(image.size() != _rect.size())
		image = image.scaled(_rect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	// Clip image to scene
	const QRect scenerect(0, 0, scene()->width(), scene()->height());
	QRect intersection = _rect & scenerect;
	if(!intersection.isEmpty()) {
		int xoff=0, yoff=0;
		if(intersection != _rect) {
			if(_rect.x() < 0)
				xoff = -_rect.x();
			if(_rect.y() < 0)
				yoff = -_rect.y();

			intersection.moveLeft(xoff);
			intersection.moveTop(yoff);
			image = image.copy(intersection);
		}

		// Merge image
		client->sendUndopoint();
		client->sendImage(layer, _rect.x() + xoff, _rect.y() + yoff, image, true);
	}
}

}

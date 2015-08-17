/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#include "core/blendmodes.h"
#include "scene/canvasscene.h"

#include <QApplication>
#include <QPainter>


namespace drawingboard {

SelectionItem::SelectionItem(QGraphicsItem *parent)
	: QGraphicsItem(parent), _marchingants(0), _closePolygon(false)
{
}

void SelectionItem::savePolygonShape()
{
	QPolygonF poly;
	poly.reserve(_polygon.size());
	const QPointF center = _polygon.boundingRect().center();
	for(const QPointF &p : _polygon)
		poly << p - center;
	_originalPolygonShape = poly;
}

void SelectionItem::resetPolygonShape()
{
	prepareGeometryChange();
	const QPointF center = _polygon.boundingRect().center();
	_polygon.clear();
	for(const QPointF &p : _originalPolygonShape)
		_polygon << p + center;
}

void SelectionItem::setRect(const QRect &rect)
{
	setPolygon(QPolygon({
		rect.topLeft(),
		QPoint(rect.left() + rect.width(), rect.top()),
		QPoint(rect.left() + rect.width(), rect.top() + rect.height()),
		QPoint(rect.left(), rect.top() + rect.height())
	}));
	_closePolygon = true;
}

void SelectionItem::setPolygon(const QPolygon &polygon)
{
	prepareGeometryChange();
	_polygon = polygon;
	savePolygonShape();
}

void SelectionItem::translate(const QPoint &offset)
{
	prepareGeometryChange();
	_polygon.translate(offset);
}

void SelectionItem::scale(qreal x, qreal y)
{
	QPointF center = _polygon.boundingRect().center();
	for(QPointF &p : _polygon) {
		QPointF sp = (p - center);
		sp.rx() *= x;
		sp.ry() *= y;
		p = sp + center;
	}
}

void SelectionItem::addPointToPolygon(const QPoint &point)
{
	prepareGeometryChange();
	_polygon << point;
}

void SelectionItem::closePolygon()
{
	if(!_closePolygon) {
		_closePolygon = true;
		savePolygonShape();
	}
}

bool SelectionItem::isAxisAlignedRectangle() const
{
	if(_polygon.size() != 4)
		return false;

	// When we create a rectangular polygon (see above), the points
	// are in clockwise order, starting from the top left.
	// 0-----1
	// |     |
	// 3-----2
	// This can be rotated 90, 180 or 210 degrees and still remain an
	// axis aligned rectangle, so we need to check:
	// 0==1 and 2==3 (X plane) and 0==3 and 1==2 (Y plane)
	// OR
	// 0==3 and 1==2 (X plane) and 0==1 and 2==3 (Y plane)

	QPolygon p = polygon();

	return
		(
			// case 1
			p.at(0).y() == p.at(1).y() &&
			p.at(2).y() == p.at(3).y() &&
			p.at(0).x() == p.at(3).x() &&
			p.at(1).x() == p.at(2).x()
		) || (
			// case 2
			p.at(0).y() == p.at(3).y() &&
			p.at(1).y() == p.at(2).y() &&
			p.at(0).x() == p.at(1).x() &&
			p.at(2).x() == p.at(3).x()
		);
}

QPair<QPoint,QImage> SelectionItem::polygonMask(const QColor &color) const
{
	QRect b = polygonRect();
	QPolygon p = polygon().translated(-b.topLeft());

	QImage mask(b.size(), QImage::Format_ARGB32);
	mask.fill(0);
	QPainter painter(&mask);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawPolygon(p);

	return QPair<QPoint,QImage>(b.topLeft(), mask);
}

SelectionItem::Handle SelectionItem::handleAt(const QPoint &point, float zoom) const
{
	const qreal H = HANDLE / (zoom/100.0f);

	const QRect rect = _polygon.boundingRect().toRect();

	const QRectF R = rect.adjusted(-H/2, -H/2, H/2, H/2);

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


void SelectionItem::adjustGeometry(Handle handle, const QPoint &delta, bool keepAspect)
{
	prepareGeometryChange();
	if(keepAspect) {
		const int dxy = (qAbs(delta.x()) > qAbs(delta.y())) ? delta.x() : delta.y();
		const int dxy2 = (qAbs(delta.x()) > qAbs(-delta.y())) ? delta.x() : -delta.y();
		switch(handle) {
		case OUTSIDE: return;
		case TRANSLATE: _polygon.translate(delta); break;

		case RS_TOPLEFT: adjust(dxy, dxy, 0, 0); break;
		case RS_TOPRIGHT: adjust(0, -dxy2, dxy2, 0); break;
		case RS_BOTTOMRIGHT: adjust(0, 0, dxy, dxy); break;
		case RS_BOTTOMLEFT: adjust(dxy2, 0, 0, -dxy2); break;

		case RS_TOP: adjust(delta.y(), delta.y(), -delta.y(), -delta.y()); break;
		case RS_RIGHT: adjust(-delta.x(), -delta.x(), delta.x(), delta.x()); break;
		case RS_BOTTOM: adjust(-delta.y(), -delta.y(), delta.y(), delta.y()); break;
		case RS_LEFT: adjust(delta.x(), delta.x(), -delta.x(), -delta.x()); break;
		}
	} else {
		switch(handle) {
		case OUTSIDE: return;
		case TRANSLATE: _polygon.translate(delta); break;
		case RS_TOPLEFT: adjust(delta.x(), delta.y(), 0, 0); break;
		case RS_TOPRIGHT: adjust(0, delta.y(), delta.x(), 0); break;
		case RS_BOTTOMRIGHT: adjust(0, 0, delta.x(), delta.y()); break;
		case RS_BOTTOMLEFT: adjust(delta.x(), 0, 0, delta.y()); break;
		case RS_TOP: adjust(0, delta.y(), 0, 0); break;
		case RS_RIGHT: adjust(0, 0, delta.x(), 0); break;
		case RS_BOTTOM: adjust(0, 0, 0, delta.y()); break;
		case RS_LEFT: adjust(delta.x(), 0, 0, 0); break;
		}
	}
}

void SelectionItem::adjust(int dx1, int dy1, int dx2, int dy2)
{
	const QRectF bounds = _polygon.boundingRect();

	const qreal sx = (bounds.width() - dx1 + dx2) / bounds.width();
	const qreal sy = (bounds.height() - dy1 + dy2) / bounds.height();

	for(int i=0;i<_polygon.size();++i) {
		_polygon[i] = QPointF(
			bounds.x() + (_polygon[i].x() - bounds.x()) * sx + dx1,
			bounds.y() + (_polygon[i].y() - bounds.y()) * sy + dy1
		);
	}
}

void SelectionItem::rotate(float angle)
{
	if(qAbs(angle) < 0.0001)
		return;

	const QPointF origin = _polygon.boundingRect().center();
	QTransform t;
	t.translate(origin.x(), origin.y());
	t.rotateRadians(angle);

	prepareGeometryChange();
	for(int i=0;i<_polygon.size();++i) {
		QPointF p = _polygon[i] - origin;
		_polygon[i] = t.map(p);
	}
}

void SelectionItem::setPasteImage(const QImage &image)
{
	const QRect selectionBounds = _polygon.boundingRect().toRect();
	if(selectionBounds.size() != image.size() || !isAxisAlignedRectangle()) {
		const QPoint c = selectionBounds.center();
		setRect(QRect(c.x() - image.width()/2, c.y()-image.height()/2, image.width(), image.height()));
	}
	_pasteimg = image;
	_movedFromCanvas = false;
	update();
}

QRectF SelectionItem::boundingRect() const
{
	return _polygon.boundingRect().adjusted(-HANDLE/2, -HANDLE/2, HANDLE/2, HANDLE/2);
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	if(!_pasteimg.isNull()) {
		if(_polygon.size() == 4) {
			QPolygonF src({
				QPointF(0, 0),
				QPointF(_pasteimg.width(), 0),
				QPointF(_pasteimg.width(), _pasteimg.height()),
				QPointF(0, _pasteimg.height())
			});

			QTransform t;
			if(QTransform::quadToQuad(src, _polygon, t)) {
				painter->save();
				painter->setTransform(t, true);
				painter->drawImage(0, 0, _pasteimg);
				painter->restore();
			} else
				qWarning("Couldn't transform pasted image!");

		} else {
			qWarning("Pasted selection item with non-rectangular polygon!");
		}
	}

	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	QPen pen;
	pen.setWidth(qApp->devicePixelRatio());
	pen.setColor(Qt::white);
	pen.setStyle(Qt::DashLine);
	pen.setDashOffset(_marchingants);
	pen.setCosmetic(true);
	painter->setPen(pen);
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);

	if(_closePolygon)
		painter->drawPolygon(_polygon);
	else
		painter->drawPolyline(_polygon);

	// Draw resizing handles
	if(_closePolygon) {
		QRect rect = _polygon.boundingRect().toRect();

		pen.setWidth(HANDLE);
		pen.setStyle(Qt::SolidLine);
		painter->setPen(pen);
		painter->drawPoint(rect.topLeft());
		painter->drawPoint(rect.topLeft() + QPointF(rect.width()/2, 0));
		painter->drawPoint(rect.topRight());

		painter->drawPoint(rect.topLeft() + QPointF(0, rect.height()/2));
		painter->drawPoint(rect.topRight() + QPointF(0, rect.height()/2));

		painter->drawPoint(rect.bottomLeft());
		painter->drawPoint(rect.bottomLeft() + QPointF(rect.width()/2, 0));
		painter->drawPoint(rect.bottomRight());
	}
}

void SelectionItem::marchingAnts()
{
	_marchingants += 1;
	update();
}

QRect SelectionItem::canvasRect() const
{
	Q_ASSERT(qobject_cast<CanvasScene*>(scene()));

	return QRect(QPoint(), static_cast<CanvasScene*>(scene())->imageSize());
}

void SelectionItem::pasteToCanvas(net::Client *client, int layer) const
{
	if(_pasteimg.isNull())
		return;

	const QRect rect = _polygon.boundingRect().toRect();

	if(_polygon.size()!=4) {
		qWarning("Paste selection is not a quad!");
		return;
	}

	// Transform image to selection rectangle
	QPolygonF src({
		QPointF(0, 0),
		QPointF(_pasteimg.width(), 0),
		QPointF(_pasteimg.width(), _pasteimg.height()),
		QPointF(0, _pasteimg.height())
	});

	QPolygonF target = _polygon.translated(-_polygon.boundingRect().topLeft());

	QTransform transform;
	if(!QTransform::quadToQuad(src, target, transform)) {
		qWarning("Couldn't transform pasted image!");
		return;
	}

	// Paint transformed image
	QImage image(rect.size(), QImage::Format_ARGB32);
	image.fill(0);
	{
		QPainter imagep(&image);
		imagep.setRenderHint(QPainter::SmoothPixmapTransform);
		imagep.setTransform(transform);
		imagep.drawImage(0, 0, _pasteimg);
	}


	// Clip image to scene
	const QRect scenerect = canvasRect();
	QRect intersection = rect & scenerect;
	if(!intersection.isEmpty()) {
		int xoff=0, yoff=0;
		if(intersection != rect) {
			if(rect.x() < 0)
				xoff = -rect.x();
			if(rect.y() < 0)
				yoff = -rect.y();

			intersection.moveLeft(xoff);
			intersection.moveTop(yoff);
			image = image.copy(intersection);
		}

		// Merge image
		client->sendUndopoint();
		client->sendImage(layer, rect.x() + xoff, rect.y() + yoff, image);
	}
}

void SelectionItem::fillCanvas(const QColor &color, paintcore::BlendMode::Mode mode, net::Client *client, int layer) const
{
	const QRect bounds = canvasRect();
	QRect area;
	QImage mask;
	QPoint maskOffset;

	if(isAxisAlignedRectangle()) {
		area = polygonRect().intersected(bounds);

	} else {
		QPair<QPoint,QImage> m = polygonMask(color);
		maskOffset = m.first;
		mask = m.second;
	}

	if(!area.isEmpty() || !mask.isNull()) {
		client->sendUndopoint();

		if(mask.isNull())
			client->sendFillRect(layer, area, color, mode);
		else
			client->sendImage(layer, maskOffset.x(), maskOffset.y(), mask, mode);
	}
}

}

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "selection.h"
#include "net/commands.h"

#include "../shared/net/undo.h"
#include "../shared/net/image.h"

#include <QPainter>

namespace canvas {

Selection::Selection(QObject *parent)
	: QObject(parent), m_closedPolygon(false), m_movedFromCanvas(false)
{

}

void Selection::saveShape()
{
	const QPointF center = m_shape.boundingRect().center();
	m_originalShape.clear();
	for(const QPointF &p : m_shape)
		m_originalShape << p - center;
}

void Selection::resetShape()
{
	const QPointF center = m_shape.boundingRect().center();
	m_shape.clear();
	for(const QPointF &p : m_originalShape)
		m_shape << p + center;

	emit shapeChanged(m_shape);
}

void Selection::setShape(const QPolygonF &shape)
{
	m_shape = shape;
	emit shapeChanged(shape);
}

void Selection::setShapeRect(const QRect &rect)
{
	setShape(QPolygonF({
		rect.topLeft(),
		QPointF(rect.left() + rect.width(), rect.top()),
		QPointF(rect.left() + rect.width(), rect.top() + rect.height()),
		QPointF(rect.left(), rect.top() + rect.height())
	}));
	m_closedPolygon = true;
}

void Selection::translate(const QPoint &offset)
{
	if(offset.x() == 0 && offset.y() == 0)
		return;

	m_shape.translate(offset);
	emit shapeChanged(m_shape);
}

void Selection::scale(qreal x, qreal y)
{
	if(x==1.0 && y==1.0)
		return;

	QPointF center = m_shape.boundingRect().center();
	for(QPointF &p : m_shape) {
		QPointF sp = (p - center);
		sp.rx() *= x;
		sp.ry() *= y;
		p = sp + center;
	}
	emit shapeChanged(m_shape);
}

void Selection::rotate(float angle)
{
	if(qAbs(angle) < 0.0001)
		return;

	const QPointF origin = m_shape.boundingRect().center();
	QTransform t;
	t.translate(origin.x(), origin.y());
	t.rotateRadians(angle);

	for(int i=0;i<m_shape.size();++i) {
		QPointF p = m_shape[i] - origin;
		m_shape[i] = t.map(p);
	}

	emit shapeChanged(m_shape);
}

Selection::Handle Selection::handleAt(const QPointF &point, float zoom) const
{
	const qreal H = handleSize() / zoom;

	const QRectF R = m_shape.boundingRect();

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


void Selection::adjustGeometry(Handle handle, const QPoint &delta, bool keepAspect)
{
	if(keepAspect) {
		const int dxy = (qAbs(delta.x()) > qAbs(delta.y())) ? delta.x() : delta.y();
		const int dxy2 = (qAbs(delta.x()) > qAbs(-delta.y())) ? delta.x() : -delta.y();
		switch(handle) {
		case OUTSIDE: return;
		case TRANSLATE: m_shape.translate(delta); break;

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
		case TRANSLATE: m_shape.translate(delta); break;
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

	emit shapeChanged(m_shape);
}

void Selection::adjust(int dx1, int dy1, int dx2, int dy2)
{
	const QRectF bounds = m_shape.boundingRect();

	const qreal sx = (bounds.width() - dx1 + dx2) / bounds.width();
	const qreal sy = (bounds.height() - dy1 + dy2) / bounds.height();

	for(int i=0;i<m_shape.size();++i) {
		m_shape[i] = QPointF(
			bounds.x() + (m_shape[i].x() - bounds.x()) * sx + dx1,
			bounds.y() + (m_shape[i].y() - bounds.y()) * sy + dy1
		);
	}
	emit shapeChanged(m_shape);
}

void Selection::addPointToShape(const QPointF &point)
{
	if(m_closedPolygon) {
		qWarning("Selection::addPointToShape: shape is closed!");

	} else {
		m_shape << point;
		emit shapeChanged(m_shape);
	}
}

void Selection::closeShape()
{
	if(!m_closedPolygon) {
		m_closedPolygon = true;
		saveShape();
		emit closed();
	}
}

bool Selection::isAxisAlignedRectangle() const
{
	if(m_shape.size() != 4)
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

	QPolygon p = m_shape.toPolygon();

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

QImage Selection::shapeMask(const QColor &color, QPoint *offset) const
{
	const QRect b = boundingRect();
	QPolygon p = m_shape.toPolygon();
	p.translate(-b.topLeft());

	QImage mask(b.size(), QImage::Format_ARGB32);
	mask.fill(0);
	QPainter painter(&mask);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawPolygon(p);

	if(offset)
		*offset = b.topLeft();

	return mask;
}

void Selection::setPasteImage(const QImage &image)
{
	const QRect selectionBounds = m_shape.boundingRect().toRect();
	if(selectionBounds.size() != image.size() || !isAxisAlignedRectangle()) {
		const QPoint c = selectionBounds.center();
		setShapeRect(QRect(c.x() - image.width()/2, c.y()-image.height()/2, image.width(), image.height()));
	}

	m_pasteImage = image;
	m_movedFromCanvas = false;

	emit pasteImageChanged(image);
}

QList<protocol::MessagePtr> Selection::pasteToCanvas(uint8_t contextId, int layer) const
{
	QList<protocol::MessagePtr> msgs;

	if(m_pasteImage.isNull()) {
		qWarning("Selection::pasteToCanvas: nothing to paste");
		return msgs;
	}

	if(m_shape.size()!=4) {
		qWarning("Paste selection is not a quad!");
		return msgs;
	}

	const QRect rect = boundingRect();

	// Transform image to selection rectangle
	QPolygonF src({
		QPointF(0, 0),
		QPointF(m_pasteImage.width(), 0),
		QPointF(m_pasteImage.width(), m_pasteImage.height()),
		QPointF(0, m_pasteImage.height())
	});

	QPolygonF target = m_shape.translated(-m_shape.boundingRect().topLeft());

	QTransform transform;
	if(!QTransform::quadToQuad(src, target, transform)) {
		qWarning("Couldn't transform pasted image!");
		return msgs;
	}

	// Paint transformed image
	QImage image(rect.size(), QImage::Format_ARGB32);
	image.fill(0);
	{
		QPainter imagep(&image);
		imagep.setRenderHint(QPainter::SmoothPixmapTransform);
		imagep.setTransform(transform);
		imagep.drawImage(0, 0, m_pasteImage);
	}

	// Merge image
	msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));
	msgs << net::command::putQImage(contextId, layer, rect.x(), rect.y(), image, paintcore::BlendMode::MODE_NORMAL);
	return msgs;
}

QList<protocol::MessagePtr> Selection::fillCanvas(uint8_t contextId, const QColor &color, paintcore::BlendMode::Mode mode, int layer) const
{
	QRect area;
	QImage mask;
	QPoint maskOffset;

	if(isAxisAlignedRectangle())
		area = boundingRect();
	else
		mask = shapeMask(color, &maskOffset);

	QList<protocol::MessagePtr> msgs;

	if(!area.isEmpty() || !mask.isNull()) {
		if(mask.isNull())
			msgs << protocol::MessagePtr(new protocol::FillRect(contextId, layer, int(mode), area.x(), area.y(), area.width(), area.height(), color.rgba()));
		else
			msgs << net::command::putQImage(contextId, layer, maskOffset.x(), maskOffset.y(), mask, mode);
	}

	if(!msgs.isEmpty())
		msgs.prepend(protocol::MessagePtr(new protocol::UndoPoint(contextId)));

	return msgs;
}

}

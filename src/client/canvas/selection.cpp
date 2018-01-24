/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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
#include "../tools/selection.h" // for selection utilities

#include "../shared/net/undo.h"
#include "../shared/net/image.h"

#include <QPainter>
#include <cmath>

namespace canvas {

Selection::Selection(QObject *parent)
	: QObject(parent), m_closedPolygon(false)
{

}

void Selection::saveShape()
{
	m_originalCenter = m_shape.boundingRect().center();
	m_originalShape = m_shape;
}


bool Selection::isTransformed() const
{
	return m_shape != m_originalShape;
}

void Selection::reset()
{
	m_shape = m_originalShape;
	emit shapeChanged(m_shape);
}

void Selection::resetShape()
{
	const QPointF center = m_shape.boundingRect().center();
	m_shape.clear();
	for(const QPointF &p : m_originalShape)
		m_shape << p + center - m_originalCenter;

	emit shapeChanged(m_shape);
}

void Selection::setShape(const QPolygonF &shape)
{
	m_shape = shape;
	beginAdjustment(OUTSIDE);
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
	saveShape();
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

Selection::Handle Selection::handleAt(const QPointF &point, float zoom) const
{
	const qreal H = handleSize() / zoom;

	const QRectF R = m_shape.boundingRect().adjusted(-H/2, -H/2, H/2, H/2);

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

void Selection::beginAdjustment(Handle handle)
{
	m_adjustmentHandle = handle;
	m_preAdjustmentShape = m_shape;
}

void Selection::adjustGeometry(const QPoint &delta, bool keepAspect)
{
	if(keepAspect) {
		const int dxy = (qAbs(delta.x()) > qAbs(delta.y())) ? delta.x() : delta.y();

		const QRectF bounds = m_preAdjustmentShape.boundingRect();
		const qreal aspect = bounds.width() / bounds.height();
		const int dx = dxy * aspect;
		const int dy = dxy;

		switch(m_adjustmentHandle) {
		case OUTSIDE: return;
		case TRANSLATE: m_shape = m_preAdjustmentShape.translated(delta); break;

		case RS_TOPLEFT: adjustScale(dx, dy, 0, 0); break;
		case RS_TOPRIGHT: adjustScale(0, -dx, dy, 0); break;
		case RS_BOTTOMRIGHT: adjustScale(0, 0, dx, dy); break;
		case RS_BOTTOMLEFT: adjustScale(dx, 0, 0, -dy); break;

		case RS_TOP:
		case RS_LEFT: adjustScale(dx, dy, -dx, -dy); break;
		case RS_RIGHT:
		case RS_BOTTOM: adjustScale(-dx, -dy, dx, dy); break;
		}
	} else {
		switch(m_adjustmentHandle) {
		case OUTSIDE: return;
		case TRANSLATE: m_shape = m_preAdjustmentShape.translated(delta); break;
		case RS_TOPLEFT: adjustScale(delta.x(), delta.y(), 0, 0); break;
		case RS_TOPRIGHT: adjustScale(0, delta.y(), delta.x(), 0); break;
		case RS_BOTTOMRIGHT: adjustScale(0, 0, delta.x(), delta.y()); break;
		case RS_BOTTOMLEFT: adjustScale(delta.x(), 0, 0, delta.y()); break;
		case RS_TOP: adjustScale(0, delta.y(), 0, 0); break;
		case RS_RIGHT: adjustScale(0, 0, delta.x(), 0); break;
		case RS_BOTTOM: adjustScale(0, 0, 0, delta.y()); break;
		case RS_LEFT: adjustScale(delta.x(), 0, 0, 0); break;
		}
	}

	emit shapeChanged(m_shape);
}

void Selection::adjustScale(int dx1, int dy1, int dx2, int dy2)
{
	Q_ASSERT(m_preAdjustmentShape.size() == m_shape.size());

	const QRectF bounds = m_preAdjustmentShape.boundingRect();

	const qreal sx = (bounds.width() - dx1 + dx2) / bounds.width();
	const qreal sy = (bounds.height() - dy1 + dy2) / bounds.height();

	for(int i=0;i<m_preAdjustmentShape.size();++i) {
		m_shape[i] = QPointF(
			bounds.x() + (m_preAdjustmentShape[i].x() - bounds.x()) * sx + dx1,
			bounds.y() + (m_preAdjustmentShape[i].y() - bounds.y()) * sy + dy1
		);
		if(std::isnan(m_shape[i].x()) || std::isnan(m_shape[i].y())) {
			qWarning("Selection shape[%d] is Not a Number!", i);
			m_shape = m_preAdjustmentShape;
			return;
		}
	}
}

void Selection::adjustRotation(float angle)
{
	Q_ASSERT(m_preAdjustmentShape.size() == m_shape.size());

	if(qAbs(angle) < 0.0001)
		return;

	const QPointF origin = m_preAdjustmentShape.boundingRect().center();
	QTransform t;
	t.translate(origin.x(), origin.y());
	t.rotateRadians(angle);

	for(int i=0;i<m_shape.size();++i) {
		const QPointF p = m_preAdjustmentShape[i] - origin;
		m_shape[i] = t.map(p);
	}

	emit shapeChanged(m_shape);
}

void Selection::adjustShear(float sh, float sv)
{
	Q_ASSERT(m_preAdjustmentShape.size() == m_shape.size());

	if(qAbs(sh) < 0.0001 && qAbs(sv) < 0.0001)
		return;

	const QPointF origin = m_preAdjustmentShape.boundingRect().center();
	QTransform t;
	t.translate(origin.x(), origin.y());
	t.shear(sh, sv);

	for(int i=0;i<m_shape.size();++i) {
		const QPointF p = m_preAdjustmentShape[i] - origin;
		m_shape[i] = t.map(p);
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

static bool isAxisAlignedRectangle(const QPolygon &p)
{
	if(p.size() != 4)
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

bool Selection::isAxisAlignedRectangle() const
{
	if(m_shape.size() != 4)
		return false;

	return canvas::isAxisAlignedRectangle(m_shape.toPolygon());
}

QImage Selection::shapeMask(const QColor &color, QRect *maskBounds) const
{
	return tools::SelectionTool::shapeMask(color, m_shape, maskBounds);
}

void Selection::setPasteImage(const QImage &image)
{
	m_moveRegion = QPolygonF();
	setPasteOrMoveImage(image);
}

void Selection::setMoveImage(const QImage &image, const QSize &canvasSize)
{
	m_moveRegion = m_shape;

	for(QPointF &p : m_moveRegion) {
		p.setX(qBound(0.0, p.x(), qreal(canvasSize.width())));
		p.setY(qBound(0.0, p.y(), qreal(canvasSize.height())));
	}

	setPasteOrMoveImage(image);
}

void Selection::setPasteOrMoveImage(const QImage &image)
{
	const QRect selectionBounds = m_shape.boundingRect().toRect();
	if(selectionBounds.size() != image.size() || !isAxisAlignedRectangle()) {
		const QPoint c = selectionBounds.center();
		setShapeRect(QRect(c.x() - image.width()/2, c.y()-image.height()/2, image.width(), image.height()));
	}

	m_pasteImage = image;
	emit pasteImageChanged(image);
}

QList<protocol::MessagePtr> Selection::pasteOrMoveToCanvas(uint8_t contextId, int layer) const
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

	// Merge image
	msgs << protocol::MessagePtr(new protocol::UndoPoint(contextId));

	if(!m_moveRegion.isEmpty()) {
		qDebug("Moving instead of pasting");
		// Get source pixel mask
		QRect moveBounds;
		QByteArray mask;

		if(!canvas::isAxisAlignedRectangle(m_moveRegion.toPolygon())) {
			QImage maskimg = tools::SelectionTool::shapeMask(Qt::white, m_moveRegion, &moveBounds, true);
			mask = qCompress(maskimg.constBits(), maskimg.byteCount());
		} else {
			moveBounds = m_moveRegion.boundingRect().toRect();
		}

		// Send move command
		QPolygon s = m_shape.toPolygon();
		msgs << protocol::MessagePtr(new protocol::MoveRegion(contextId, layer,
			moveBounds.x(), moveBounds.y(), moveBounds.width(), moveBounds.height(),
			s[0].x(), s[0].y(),
			s[1].x(), s[1].y(),
			s[2].x(), s[2].y(),
			s[3].x(), s[3].y(),
			mask
		));

	} else {
		QImage image;
		QPoint offset;

		image = tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), &offset);

		msgs << net::command::putQImage(contextId, layer, offset.x(), offset.y(), image, paintcore::BlendMode::MODE_NORMAL);
	}
	return msgs;
}

QList<protocol::MessagePtr> Selection::fillCanvas(uint8_t contextId, const QColor &color, paintcore::BlendMode::Mode mode, int layer) const
{
	QRect area;
	QImage mask;
	QRect maskBounds;

	if(isAxisAlignedRectangle())
		area = boundingRect();
	else
		mask = shapeMask(color, &maskBounds);

	QList<protocol::MessagePtr> msgs;

	if(!area.isEmpty() || !mask.isNull()) {
		if(mask.isNull())
			msgs << protocol::MessagePtr(new protocol::FillRect(contextId, layer, int(mode), area.x(), area.y(), area.width(), area.height(), color.rgba()));
		else
			msgs << net::command::putQImage(contextId, layer, maskBounds.left(), maskBounds.top(), mask, mode);
	}

	if(!msgs.isEmpty())
		msgs.prepend(protocol::MessagePtr(new protocol::UndoPoint(contextId)));

	return msgs;
}

}

/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

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

#include "libclient/canvas/selection.h"
#include "libclient/net/envelopebuilder.h"
#include "libclient/net/envelope.h"
#include "libclient/tools/selection.h" // for selection utilities
#include "rustpile/rustpile.h"

#include <QPainter>
#include <QtMath>

namespace canvas {

Selection::Selection(QObject *parent)
	: QObject(parent)
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

bool Selection::isOnlyTranslated() const
{
	if(m_shape.isEmpty() || m_shape.length() != m_originalShape.length())
		return false;

	const QPointF delta = m_shape.first() - m_originalShape.first();
	for(int i=1;i<m_shape.size();++i) {
		const QPointF d = m_shape[i] - m_originalShape[i];

		if(!qFuzzyCompare(d.x(), delta.x()) || !qFuzzyCompare(d.y(), delta.y()))
			return false;
	}

	return true;
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
	for(const QPointF &p : qAsConst(m_originalShape))
		m_shape << p + center - m_originalCenter;

	emit shapeChanged(m_shape);
}

void Selection::setShape(const QPolygonF &shape)
{
	m_shape = shape;
	m_preAdjustmentShape = shape;
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

Selection::Handle Selection::handleAt(const QPointF &point, qreal zoom) const
{
	const qreal H = handleSize() / zoom;

	const QRectF R = m_shape.boundingRect().adjusted(-H/2, -H/2, H/2, H/2);

	if(!R.contains(point))
		return Handle::Outside;

	const QPointF p = point - R.topLeft();

	if(p.x() < H) {
		if(p.y() < H)
			return Handle::TopLeft;
		else if(p.y() > R.height()-H)
			return Handle::BottomLeft;
		return Handle::Left;
	} else if(p.x() > R.width() - H) {
		if(p.y() < H)
			return Handle::TopRight;
		else if(p.y() > R.height()-H)
			return Handle::BottomRight;
		return Handle::Right;
	} else if(p.y() < H)
		return Handle::Top;
	else if(p.y() > R.height()-H)
		return Handle::Bottom;

	return Handle::Center;
}

void Selection::setAdjustmentMode(AdjustmentMode mode)
{
	if(m_adjustmentMode != mode) {
		m_adjustmentMode = mode;
		emit adjustmentModeChanged(mode);
	}
}

void Selection::beginAdjustment(Handle handle)
{
	m_adjustmentHandle = handle;
	m_preAdjustmentShape = m_shape;
}

void Selection::adjustGeometry(const QPointF &start, const QPointF &point, bool constrain)
{
	switch(m_adjustmentMode) {
	case AdjustmentMode::Scale:
		adjustGeometryScale((point - start).toPoint(), constrain);
		break;
	case AdjustmentMode::Rotate:
		adjustGeometryRotate(start, point, constrain);
		break;
	case AdjustmentMode::Distort:
		adjustGeometryDistort((point - start).toPoint());
		break;
	}
}

void Selection::adjustGeometryScale(const QPoint &delta, bool keepAspect)
{
	if(keepAspect) {
		const int dxy = (qAbs(delta.x()) > qAbs(delta.y())) ? delta.x() : delta.y();

		const QRectF bounds = m_preAdjustmentShape.boundingRect();
		const qreal aspect = bounds.width() / bounds.height();
		const qreal dx = dxy * aspect;
		const qreal dy = dxy;

		switch(m_adjustmentHandle) {
		case Handle::Outside: return;
		case Handle::Center: adjustTranslation(delta); break;

		case Handle::TopLeft: adjustScale(dx, dy, 0, 0); break;
		case Handle::TopRight: adjustScale(0, -dy, dx, 0); break;
		case Handle::BottomRight: adjustScale(0, 0, dx, dy); break;
		case Handle::BottomLeft: adjustScale(dx, 0, 0, -dy); break;

		case Handle::Top:
		case Handle::Left: adjustScale(dx, dy, -dx, -dy); break;
		case Handle::Right:
		case Handle::Bottom: adjustScale(-dx, -dy, dx, dy); break;
		}
	} else {
		switch(m_adjustmentHandle) {
		case Handle::Outside: return;
		case Handle::Center: adjustTranslation(delta); break;
		case Handle::TopLeft: adjustScale(delta.x(), delta.y(), 0, 0); break;
		case Handle::TopRight: adjustScale(0, delta.y(), delta.x(), 0); break;
		case Handle::BottomRight: adjustScale(0, 0, delta.x(), delta.y()); break;
		case Handle::BottomLeft: adjustScale(delta.x(), 0, 0, delta.y()); break;
		case Handle::Top: adjustScale(0, delta.y(), 0, 0); break;
		case Handle::Right: adjustScale(0, 0, delta.x(), 0); break;
		case Handle::Bottom: adjustScale(0, 0, 0, delta.y()); break;
		case Handle::Left: adjustScale(delta.x(), 0, 0, 0); break;
		}
	}
}

void Selection::adjustGeometryRotate(const QPointF &start, const QPointF &point, bool constrain)
{
	switch(m_adjustmentHandle) {
	case Handle::Outside: return;
	case Handle::Center: adjustTranslation(point - start); break;
	case Handle::TopLeft:
	case Handle::TopRight:
	case Handle::BottomRight:
	case Handle::BottomLeft: {
		const QPointF center = boundingRect().center();
		const qreal a0 = atan2(start.y() - center.y(), start.x() - center.x());
		const qreal a1 = atan2(point.y() - center.y(), point.x() - center.x());
		qreal a = a1 - a0;
		if(constrain) {
			const auto STEP = M_PI / 12;
			a = qRound(a / STEP) * STEP;
		}

		adjustRotation(a);
		break;
		}
	case Handle::Top: adjustShear((start.x() - point.x()) / 100.0, 0); break;
	case Handle::Bottom: adjustShear((point.x() - start.x()) / 100.0, 0); break;
	case Handle::Right: adjustShear(0, (point.y() - start.y()) / 100.0); break;
	case Handle::Left: adjustShear(0, (start.y() - point.y()) / 100.0); break;
	}
}

void Selection::adjustGeometryDistort(const QPointF &delta)
{
	switch(m_adjustmentHandle) {
	case Handle::Outside: return;
	case Handle::Center: adjustTranslation(delta); break;
	case Handle::TopLeft: adjustDistort(delta, 0); break;
	case Handle::Top: adjustDistort(delta, 0, 1); break;
	case Handle::TopRight: adjustDistort(delta, 1); break;
	case Handle::Right: adjustDistort(delta, 1, 2); break;
	case Handle::BottomRight: adjustDistort(delta, 2); break;
	case Handle::Bottom: adjustDistort(delta, 2, 3); break;
	case Handle::BottomLeft: adjustDistort(delta, 3); break;
	case Handle::Left: adjustDistort(delta, 3, 0); break;
	}
}

void Selection::adjustTranslation(const QPointF &start, const QPointF &point)
{
	adjustTranslation(point - start);
}

void Selection::adjustTranslation(const QPointF &delta)
{
	m_shape = m_preAdjustmentShape.translated(delta);
	emit shapeChanged(m_shape);
}

void Selection::adjustScale(qreal dx1, qreal dy1, qreal dx2, qreal dy2)
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
			break;
		}
	}

	emit shapeChanged(m_shape);
}

void Selection::adjustRotation(qreal angle)
{
	Q_ASSERT(m_preAdjustmentShape.size() == m_shape.size());

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

void Selection::adjustShear(qreal sh, qreal sv)
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

void Selection::adjustDistort(const QPointF &delta, int corner1, int corner2)
{
	Q_ASSERT(m_preAdjustmentShape.size() == m_shape.size());

	if(corner1 < 0 || corner1 > 3 || corner2 > 3) {
		qWarning("Selection::adjustDistort: corner index out of bounds");
		return;
	}

	// There exists a constructor to create a QPolygonF from a QRectF, but it
	// creates a closed polygon with 5 points. QTransform::quadToQuad takes an
	// open polygon with 4 points though, so we're initializing it like this.
	auto bounds = m_preAdjustmentShape.boundingRect();
	auto source = QPolygonF{QVector<QPointF>{{
		bounds.topLeft(),
		bounds.topRight(),
		bounds.bottomRight(),
		bounds.bottomLeft(),
	}}};

	auto target = QPolygonF{source};
	target[corner1] += delta;
	if (corner1 != corner2 && corner2 >= 0) {
		target[corner2] += delta;
	}

	QTransform t;
	if(!QTransform::quadToQuad(source, target, t)) {
		qDebug("Selection::adjustDistort: no transformation possible");
		return;
	}

	for(int i=0;i<m_shape.size();++i) {
		// No need to adjust the origin like in the other transformation
		// functions, the quadToQuad call deals with that already.
		m_shape[i] = t.map(m_preAdjustmentShape[i]);
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

bool Selection::closeShape(const QRectF &clipRect)
{
	if(!m_closedPolygon) {
		QPolygonF intersection = m_shape.intersected(clipRect);
		if(intersection.isEmpty())
			return false;
		intersection.pop_back(); // We use implicitly closed polygons
		m_shape = intersection;

		m_closedPolygon = true;
		saveShape();
		emit closed();
	}

	return true;
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

	const QRect selectionBounds = m_shape.boundingRect().toRect();
	if(selectionBounds.size() != image.size() || !isAxisAlignedRectangle()) {
		const QPoint c = selectionBounds.center();
		setShapeRect(QRect(c.x() - image.width()/2, c.y()-image.height()/2, image.width(), image.height()));
	}

	closeShape();
	m_pasteImage = image;
	emit pasteImageChanged(image);
}

void Selection::setMoveImage(const QImage &image, const QRect &imageRect, const QSize &canvasSize, int sourceLayerId)
{
	m_moveRegion = m_shape;
	m_sourceLayerId = sourceLayerId;
	m_pasteImage = image;

	setShapeRect(imageRect);

	for(QPointF &p : m_moveRegion) {
		p.setX(qBound(0.0, p.x(), qreal(canvasSize.width())));
		p.setY(qBound(0.0, p.y(), qreal(canvasSize.height())));
	}

	emit pasteImageChanged(image);
}

net::Envelope Selection::pasteOrMoveToCanvas(uint8_t contextId, int layer) const
{
	if(m_pasteImage.isNull()) {
		qWarning("Selection::pasteToCanvas: nothing to paste");
		return net::Envelope();
	}

	if(m_shape.size()!=4) {
		qWarning("Paste selection is not a quad!");
		return net::Envelope();
	}

	// Merge image
	net::EnvelopeBuilder writer;

	rustpile::write_undopoint(writer, contextId);

	if(!m_moveRegion.isEmpty()) {
		// Get source pixel mask
		QRect moveBounds;
		QImage mask;

		if(!canvas::isAxisAlignedRectangle(m_moveRegion.toPolygon())) {
			mask = tools::SelectionTool::shapeMask(Qt::white, m_moveRegion, &moveBounds);

		} else {
			moveBounds = m_moveRegion.boundingRect().toRect();
		}

		// Send move command
		if(isOnlyTranslated()) {
			// If we've only moved the selection without scaling, rotating or distorting it,
			// we can use the fast MoveRect command.
			QByteArray compressedMask;
			if(!mask.isNull()) {
				compressedMask = qCompress(
					mask.constBits(),
					mask.sizeInBytes()
				);
			}

			rustpile::write_moverect(
				writer,
				contextId,
				layer,
				moveBounds.x(),
				moveBounds.y(),
				m_shape.at(0).x(),
				m_shape.at(0).y(),
				moveBounds.width(),
				moveBounds.height(),
				reinterpret_cast<const uint8_t*>(compressedMask.constData()),
				compressedMask.length()
			);
		} else {
			// Version 2.1 MoveRegion is presently not implemented in the Rustpile
			// engine, so a transformed selection must be processed clientside and sent
			// as an image.

			QPoint offset;
			const QImage image = tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), &offset);

			if(mask.isNull()) {
				rustpile::write_fillrect(writer, contextId, layer, rustpile::Blendmode::Erase, moveBounds.x(), moveBounds.y(), moveBounds.width(), moveBounds.height(), 0xffffffff);
			} else {
				writer.buildPutQImage(contextId, layer, moveBounds.x(), moveBounds.y(), mask, rustpile::Blendmode::Erase);
			}

			writer.buildPutQImage(contextId, layer, offset.x(), offset.y(), image, rustpile::Blendmode::Normal);
		}

	} else {
		// A pasted image
		QPoint offset;
		const QImage image = tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), &offset);

		writer.buildPutQImage(contextId, layer, offset.x(), offset.y(), image, rustpile::Blendmode::Normal);
	}

	return writer.toEnvelope();
}

QImage Selection::transformedPasteImage() const
{
	return tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), nullptr);
}

net::Envelope Selection::fillCanvas(uint8_t contextId, const QColor &color, rustpile::Blendmode mode, int layer) const
{
	QRect area;
	QImage mask;
	QRect maskBounds;

	if(isAxisAlignedRectangle())
		area = boundingRect();
	else
		mask = shapeMask(color, &maskBounds);

	if(!area.isEmpty() || !mask.isNull()) {
		net::EnvelopeBuilder writer;
		rustpile::write_undopoint(writer, contextId);

		if(mask.isNull())
			rustpile::write_fillrect(writer, contextId, layer, mode, area.x(), area.y(), area.width(), area.height(), color.rgba());
		else
			writer.buildPutQImage(contextId, layer, maskBounds.left(), maskBounds.top(), mask, mode);

		return writer.toEnvelope();
	} else {
		return net::Envelope();
	}
}

}

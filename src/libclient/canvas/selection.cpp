// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpmsg/message.h>
}

#include "libclient/canvas/selection.h"
#include "libclient/tools/selection.h" // for selection utilities
#include "libclient/drawdance/message.h"

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
		const QRectF bounds = m_preAdjustmentShape.boundingRect();
		QPointF handle;
		QPointF anchor;
		bool swap = false;
		int top = 0;
		int left = 0;
		int bottom = 0;
		int right = 0;

		switch(m_adjustmentHandle) {
		case Handle::Outside: return;
		case Handle::Center: adjustTranslation(delta); return;
		case Handle::BottomRight:
			swap = true;
			[[fallthrough]];
		case Handle::TopLeft:
			handle = bounds.topLeft();
			anchor = bounds.bottomRight();
			top = 1;
			left = 1;
			break;
		case Handle::BottomLeft:
			swap = true;
			[[fallthrough]];
		case Handle::TopRight:
			handle = bounds.topRight();
			anchor = bounds.bottomLeft();
			top = 1;
			right = 1;
			break;
		case Handle::Bottom:
			swap = true;
			[[fallthrough]];
		case Handle::Top:
			handle = QPointF(bounds.left() + bounds.width() / 2, bounds.top());
			anchor = handle + QPointF(0, bounds.height());
			top = 1;
			left = 1;
			right = -1;
			break;
		case Handle::Right:
			swap = true;
			[[fallthrough]];
		case Handle::Left:
			handle = QPointF(bounds.left(), bounds.top() + bounds.height() / 2);
			anchor = handle + QPointF(bounds.width(), 0);
			top = 1;
			left = 1;
			bottom = -1;
			break;
		}

		if(swap) {
			std::swap(handle, anchor);
			std::swap(top, bottom);
			std::swap(left, right);
		}

		const QPointF sizeWithSign = handle - anchor;
		const qreal zoom = QPointF::dotProduct(delta + sizeWithSign, sizeWithSign) / QPointF::dotProduct(sizeWithSign, sizeWithSign);
		const QPoint snapped = (anchor + sizeWithSign * zoom - handle).toPoint();
		const qreal a = bounds.width() / bounds.height();
		const int dx = sizeWithSign.x() == 0 ? (snapped.y() * a / 2) : snapped.x();
		const int dy = sizeWithSign.y() == 0 ? (snapped.x() / a / 2) : snapped.y();
		adjustScale(left * dx, top * dy, right * dx, bottom * dy);
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

bool Selection::isAxisAlignedRectangle(bool source) const
{
	const QPolygonF &shape = source ? m_moveRegion : m_shape;
	return shape.size() == 4 && canvas::isAxisAlignedRectangle(shape.toPolygon());
}

QRect Selection::boundingRect(bool source) const
{
	const QPolygonF &shape = source ? m_moveRegion : m_shape;
	return shape.boundingRect().toRect();
}

QImage Selection::shapeMask(
	const QColor &color, QRect *maskBounds, bool source) const
{
	const QPolygonF &shape = source ? m_moveRegion : m_shape;
	return tools::SelectionTool::shapeMask(color, shape, maskBounds, false);
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

static void appendPutImage(drawdance::MessageList &buffer, uint8_t contextId, uint16_t layer, int x, int y, const QImage &image, DP_BlendMode mode)
{
	drawdance::Message::makePutImages(buffer, contextId, layer, mode, x, y, image);
}

bool Selection::pasteOrMoveToCanvas(drawdance::MessageList &buffer, uint8_t contextId, int layer, int interpolation, bool compatibilityMode) const
{
	if(m_pasteImage.isNull()) {
		qWarning("Selection::pasteToCanvas: nothing to paste");
		return false;
	}

	if(m_shape.size()!=4) {
		qWarning("Paste selection is not a quad!");
		return false;
	}

	// Merge image

	if(!m_moveRegion.isEmpty()) {
		// Get source pixel mask
		QRect moveBounds;
		QImage mask;

		if(!canvas::isAxisAlignedRectangle(m_moveRegion.toPolygon())) {
			mask = tools::SelectionTool::shapeMask(
				Qt::white, m_moveRegion, &moveBounds, compatibilityMode);
		} else {
			moveBounds = m_moveRegion.boundingRect().toRect();
		}

		// If we've only moved the selection without scaling, rotating or
		// distorting it, we can use the faster MoveRect, otherwise MoveRegion.
		if(compatibilityMode) {
			QPolygon s = m_shape.toPolygon();
			// TODO: moving between different layers via putimage?
			drawdance::Message msg = drawdance::Message::makeMoveRegion(
				contextId, layer, moveBounds.x(), moveBounds.y(),
				moveBounds.width(), moveBounds.height(), s[0].x(), s[0].y(),
				s[1].x(), s[1].y(), s[2].x(), s[2].y(), s[3].x(), s[3].y(),
				mask);
			if(msg.isNull()) {
				qWarning("Transform: mask too large");
				return false;
			} else {
				buffer.append(drawdance::Message::makeUndoPoint(contextId));
				buffer.append(msg);
			}
		} else if(isOnlyTranslated()) {
			drawdance::Message msg = drawdance::Message::makeMoveRect(
				contextId, layer, m_sourceLayerId, moveBounds.x(), moveBounds.y(),
				m_shape.at(0).x(), m_shape.at(0).y(), moveBounds.width(),
				moveBounds.height(), mask);
			if(msg.isNull()) {
				qWarning("Translate: mask too large");
				return false;
			} else {
				buffer.append(drawdance::Message::makeUndoPoint(contextId));
				buffer.append(msg);
			}
		} else {
			QPolygon s = m_shape.toPolygon();
			drawdance::Message msg = drawdance::Message::makeTransformRegion(
				contextId, layer, m_sourceLayerId, moveBounds.x(), moveBounds.y(),
				moveBounds.width(), moveBounds.height(), s[0].x(), s[0].y(),
				s[1].x(), s[1].y(), s[2].x(), s[2].y(), s[3].x(), s[3].y(),
				interpolation, mask);
			if(msg.isNull()) {
				qWarning("Transform: mask too large");
				return false;
			} else {
				buffer.append(drawdance::Message::makeUndoPoint(contextId));
				buffer.append(msg);
			}
		}

	} else {
		// A pasted image
		QPoint offset;
		QImage image = tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), &offset);
		buffer.append(drawdance::Message::makeUndoPoint(contextId));
		appendPutImage(buffer, contextId, layer, offset.x(), offset.y(), image, DP_BLEND_MODE_NORMAL);
	}

	return true;
}

QImage Selection::transformedPasteImage() const
{
	return tools::SelectionTool::transformSelectionImage(m_pasteImage, m_shape.toPolygon(), nullptr);
}

QPolygon Selection::destinationQuad() const
{
	return tools::SelectionTool::destinationQuad(m_pasteImage, m_shape.toPolygon());
}

bool Selection::fillCanvas(
	drawdance::MessageList &buffer, uint8_t contextId, const QColor &color,
	DP_BlendMode mode, int layer, bool source) const
{
	QRect area;
	QImage mask;
	QRect maskBounds;

	if(isAxisAlignedRectangle(source)) {
		area = boundingRect(source);
	} else {
		mask = shapeMask(color, &maskBounds, source);
	}

	if(!area.isEmpty() || !mask.isNull()) {
		buffer.append(drawdance::Message::makeUndoPoint(contextId));

		if(mask.isNull()) {
			buffer.append(drawdance::Message::makeFillRect(
				contextId, layer, mode, area.x(), area.y(), area.width(), area.height(), color));
		} else {
			appendPutImage(buffer, contextId, layer, maskBounds.left(), maskBounds.top(), mask, mode);
		}

		return true;
	} else {
		return false;
	}
}

}

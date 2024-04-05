// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/selection.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/image.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/utils.h"
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QTransform>
#include <QtMath>

namespace tools {

void SelectionTool::begin(const canvas::Point &point, bool right, float zoom)
{
	if(right) {
		return;
	}

	canvas::Selection *sel =
		m_allowTransform ? m_owner.model()->selection() : nullptr;
	if(sel)
		m_handle = sel->handleAt(point, zoom);
	else
		m_handle = canvas::Selection::Handle::Outside;

	m_start = point;
	m_p1 = point;
	m_end = point;

	if(m_handle == canvas::Selection::Handle::Outside) {
		net::Client *client = m_owner.client();
		if(sel &&
		   sel->pasteOrMoveToCanvas(
			   m_messages, client->myId(), m_owner.activeLayer(),
			   m_owner.selectInterpolation(), client->isCompatibilityMode())) {
			client->sendMessages(m_messages.count(), m_messages.constData());
			m_messages.clear();
		}

		sel = new canvas::Selection;
		initSelection(sel);
		m_owner.model()->setSelection(sel);
	} else {
		sel->beginAdjustment(m_handle);
	}
}

void SelectionTool::motion(
	const canvas::Point &point, bool constrain, bool center)
{
	canvas::Selection *sel = m_owner.model()->selection();
	if(!sel)
		return;

	m_end = point;

	if(m_handle == canvas::Selection::Handle::Outside) {
		newSelectionMotion(point, constrain, center);

	} else {
		if(sel->pasteImage().isNull() &&
		   !m_owner.model()->aclState()->isLayerLocked(m_owner.activeLayer())) {
			startMove();
		}

		sel->adjustGeometry(m_start, point, constrain);
	}
}

void SelectionTool::end()
{
	canvas::Selection *sel = m_owner.model()->selection();
	if(!sel)
		return;

	// The shape must be closed after the end of the selection operation
	if(!m_owner.model()->selection()->closeShape(
		   QRectF(QPointF(), m_owner.model()->size()))) {
		// Clear selection if it was entirely outside the canvas
		m_owner.model()->setSelection(nullptr);
		return;
	}

	// Remove tiny selections
	QRectF selrect = sel->boundingRect();
	if(selrect.width() * selrect.height() <= 2) {
		m_owner.model()->setSelection(nullptr);
		return;
	}

	// Toggle adjustment mode if just clicked
	if((m_end - m_start).manhattanLength() < 2.0) {
		canvas::Selection::AdjustmentMode nextMode;
		switch(sel->adjustmentMode()) {
		case canvas::Selection::AdjustmentMode::Scale:
			nextMode = canvas::Selection::AdjustmentMode::Rotate;
			break;
		case canvas::Selection::AdjustmentMode::Rotate:
			nextMode = canvas::Selection::AdjustmentMode::Distort;
			break;
		default:
			nextMode = canvas::Selection::AdjustmentMode::Scale;
			break;
		}
		sel->setAdjustmentMode(nextMode);
	}
}

void SelectionTool::finishMultipart()
{
	canvas::Selection *sel = m_owner.model()->selection();
	net::Client *client = m_owner.client();
	if(sel &&
	   sel->pasteOrMoveToCanvas(
		   m_messages, client->myId(), m_owner.activeLayer(),
		   m_owner.selectInterpolation(), client->isCompatibilityMode())) {
		m_owner.client()->sendMessages(
			m_messages.count(), m_messages.constData());
		m_messages.clear();
		m_owner.model()->setSelection(nullptr);
	}
}

void SelectionTool::cancelMultipart()
{
	m_owner.model()->setSelection(nullptr);
}


void SelectionTool::undoMultipart()
{
	canvas::Selection *sel = m_owner.model()->selection();
	if(sel) {
		if(sel->isTransformed())
			sel->reset();
		else
			cancelMultipart();
	}
}

bool SelectionTool::isMultipart() const
{
	return m_owner.model()->selection() != nullptr;
}

void SelectionTool::offsetActiveTool(int x, int y)
{
	canvas::Selection *sel = m_owner.model()->selection();
	if(sel) {
		sel->offsetBy(x, y);
	}
}

void SelectionTool::startMove()
{
	canvas::CanvasModel *model = m_owner.model();
	canvas::Selection *sel = model->selection();
	Q_ASSERT(sel);

	// Get the selection shape mask (needs to be done before the shape is
	// overwritten by setMoveImage)
	QRect maskBounds;
	QImage eraseMask = sel->shapeMask(Qt::white, &maskBounds);

	// Copy layer content into move preview buffer.
	int layerId = m_owner.activeLayer();
	const QImage img = model->selectionToImage(layerId);
	sel->setMoveImage(img, maskBounds, model->size(), layerId);

	// The actual canvas pixels aren't touch yet, so we create a temporary
	// sublayer to erase the selected region.
	model->paintEngine()->previewCut(layerId, maskBounds, eraseMask);
}

RectangleSelection::RectangleSelection(ToolController &owner)
	: SelectionTool(
		  owner, SELECTION,
		  QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
}

void RectangleSelection::initSelection(canvas::Selection *selection)
{
	QPoint p = m_start.toPoint();
	selection->setShapeRect(QRect(p, p));
}

void RectangleSelection::newSelectionMotion(
	const canvas::Point &point, bool constrain, bool center)
{
	QPointF p;
	if(constrain)
		p = constraints::square(m_start, point);
	else
		p = point;

	if(center)
		m_p1 = m_start - (p.toPoint() - m_start);
	else
		m_p1 = m_start;

	m_owner.model()->selection()->setShapeRect(
		QRectF(m_p1, p).normalized().toRect());
}

PolygonSelection::PolygonSelection(ToolController &owner)
	: SelectionTool(
		  owner, POLYGONSELECTION,
		  QCursor(QPixmap(":cursors/select-lasso.png"), 2, 29))
{
}

void PolygonSelection::initSelection(canvas::Selection *selection)
{
	selection->setShape(QPolygonF({m_start}));
}

void PolygonSelection::newSelectionMotion(
	const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	Q_ASSERT(m_owner.model()->selection());
	m_owner.model()->selection()->addPointToShape(point);
}

QImage SelectionTool::transformSelectionImage(
	const QImage &source, const QPolygon &target, int interpolation,
	QPoint *offset)
{
	Q_ASSERT(!source.isNull());
	Q_ASSERT(target.size() == 4);
	QRect bounds;
	QPolygon dstQuad = destinationQuad(source, target, &bounds);
	QImage img = drawdance::transformImage(
		source, dstQuad,
		getEffectiveInterpolation(source.rect(), dstQuad, interpolation),
		offset);
	if(img.isNull()) {
		qWarning("Couldn't transform selection image: %s", DP_error());
		return QImage();
	} else {
		if(offset) {
			*offset += bounds.topLeft();
		}
		return img;
	}
}

QPolygon SelectionTool::destinationQuad(
	const QImage &source, const QPolygon &target, QRect *outBounds)
{
	Q_ASSERT(!source.isNull());
	Q_ASSERT(target.size() == 4);

	const QRect bounds = target.boundingRect();

	if(outBounds) {
		*outBounds = bounds;
	}
	return target.translated(-bounds.topLeft());
}

QImage SelectionTool::shapeMask(
	const QColor &color, const QPolygonF &selection, QRect *maskBounds,
	bool mono)
{
	const QRectF bf = selection.boundingRect();
	const QRect b = bf.toRect();
	const QPolygonF p = selection.translated(-bf.topLeft());

	QImage mask(
		b.size(),
		mono ? QImage::Format_Mono : QImage::Format_ARGB32_Premultiplied);
	mask.fill(0);

	QPainter painter(&mask);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawPolygon(p);

	if(maskBounds)
		*maskBounds = b;

	return mask;
}

int SelectionTool::getEffectiveInterpolation(
	const QRect &srcRect, const QPolygon &dstQuad, int requestedInterpolation)
{
	if(requestedInterpolation != DP_MSG_TRANSFORM_REGION_MODE_NEAREST &&
	   dstQuad.size() == 4) {
		QPolygon srcQuad(
			{QPoint(0, 0), QPoint(srcRect.width(), 0),
			 QPoint(srcRect.width(), srcRect.height()),
			 QPoint(0, srcRect.height())});
		QTransform tf;
		if(QTransform::quadToQuad(srcQuad, dstQuad, tf)) {
			tf.setMatrix( // Remove translation.
				tf.m11(), tf.m12(), tf.m13(), tf.m21(), tf.m22(), tf.m23(), 0.0,
				0.0, tf.m33());
			if(isRightAngleRotationOrReflection(tf)) {
				return DP_MSG_TRANSFORM_REGION_MODE_NEAREST;
			}
		}
	}
	return requestedInterpolation;
}

bool SelectionTool::isRightAngleRotationOrReflection(const QTransform &t1)
{
	QPoint scales[] = {
		QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1)};
	for(int angle = 0; angle <= 270; angle += 90) {
		for(const QPointF scale : scales) {
			QTransform t2;
			if(angle != 0) {
				t2.rotate(angle);
			}
			if(scale.x() != 1 || scale.y() != 1) {
				t2.scale(scale.x(), scale.y());
			}
			if(qFuzzyCompare(t1, t2)) {
				return true;
			}
		}
	}
	return false;
}

}

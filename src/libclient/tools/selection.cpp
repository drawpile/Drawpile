// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"

#include "libclient/tools/selection.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/utils.h"

#include <QPixmap>
#include <QtMath>
#include <QPolygonF>
#include <QTransform>
#include <QPainter>

namespace tools {

void SelectionTool::begin(const canvas::Point &point, bool right, float zoom)
{
	if(right) {
		return;
	}

	canvas::Selection *sel = m_allowTransform ? m_owner.model()->selection() : nullptr;
	if(sel)
		m_handle = sel->handleAt(point, zoom);
	else
		m_handle = canvas::Selection::Handle::Outside;

	m_start = point;
	m_p1 = point;
	m_end = point;

	if(m_handle == canvas::Selection::Handle::Outside) {
		net::Client *client = m_owner.client();
		if(sel && sel->pasteOrMoveToCanvas(m_messages, m_owner.client()->myId(), m_owner.activeLayer(), m_owner.selectInterpolation())) {
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

void SelectionTool::motion(const canvas::Point &point, bool constrain, bool center)
{
	canvas::Selection *sel = m_owner.model()->selection();
	if(!sel)
		return;

	m_end = point;

	if(m_handle==canvas::Selection::Handle::Outside) {
		newSelectionMotion(point, constrain, center);

	} else {
		if(sel->pasteImage().isNull() && !m_owner.model()->aclState()->isLayerLocked(m_owner.activeLayer())) {
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
	if(!m_owner.model()->selection()->closeShape(QRectF(QPointF(), m_owner.model()->size()))) {
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
	if(sel && sel->pasteOrMoveToCanvas(m_messages, client->myId(), m_owner.activeLayer(), m_owner.selectInterpolation())) {
		m_owner.client()->sendMessages(m_messages.count(), m_messages.constData());
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

void SelectionTool::startMove()
{
	canvas::CanvasModel *model = m_owner.model();
	canvas::Selection *sel = model->selection();
	Q_ASSERT(sel);

	// Get the selection shape mask (needs to be done before the shape is overwritten by setMoveImage)
	QRect maskBounds;
	QImage eraseMask = sel->shapeMask(Qt::white, &maskBounds);

	// Copy layer content into move preview buffer.
	int layerId = m_owner.activeLayer();
	const QImage img = model->selectionToImage(layerId);
	sel->setMoveImage(img, maskBounds, model->size(), layerId);

	// The actual canvas pixels aren't touch yet, so we create a temporary sublayer
	// to erase the selected region.
	model->paintEngine()->previewCut(layerId, maskBounds, eraseMask);
}

RectangleSelection::RectangleSelection(ToolController &owner)
	: SelectionTool(owner, SELECTION, QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
}

void RectangleSelection::initSelection(canvas::Selection *selection)
{
	QPoint p = m_start.toPoint();
	selection->setShapeRect(QRect(p, p));
}

void RectangleSelection::newSelectionMotion(const canvas::Point &point, bool constrain, bool center)
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

	m_owner.model()->selection()->setShapeRect(QRectF(m_p1, p).normalized().toRect());
}

PolygonSelection::PolygonSelection(ToolController &owner)
	: SelectionTool(owner, POLYGONSELECTION, QCursor(QPixmap(":cursors/select-lasso.png"), 2, 29))
{
}

void PolygonSelection::initSelection(canvas::Selection *selection)
{
	selection->setShape(QPolygonF({ m_start }));
}

void PolygonSelection::newSelectionMotion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	Q_ASSERT(m_owner.model()->selection());
	m_owner.model()->selection()->addPointToShape(point);
}

QImage SelectionTool::transformSelectionImage(const QImage &source, const QPolygon &target, QPoint *offset)
{
	Q_ASSERT(!source.isNull());
	Q_ASSERT(target.size() == 4);

	QRect bounds;
	QPolygonF srcPolygon;
	QPolygon xTarget = destinationQuad(source, target, &bounds, &srcPolygon);
	QTransform transform;
	if(!QTransform::quadToQuad(srcPolygon, xTarget, transform)) {
		qWarning("Couldn't transform selection image!");
		return QImage();
	}

	if(offset)
		*offset = bounds.topLeft();

	QImage out(bounds.size(), QImage::Format_ARGB32_Premultiplied);
	out.fill(0);
	QPainter painter(&out);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setTransform(transform);
	painter.drawImage(0, 0, source);

	return out;
}

QPolygon SelectionTool::destinationQuad(const QImage &source, const QPolygon &target, QRect *outBounds, QPolygonF *outSrcPolygon)
{
	Q_ASSERT(!source.isNull());
	Q_ASSERT(target.size() == 4);

	const QRect bounds = target.boundingRect();
	const QPolygonF srcPolygon({
		QPointF(0, 0),
		QPointF(source.width(), 0),
		QPointF(source.width(), source.height()),
		QPointF(0, source.height())
	});

	if(outBounds) {
		*outBounds = bounds;
	}
	if(outSrcPolygon) {
		*outSrcPolygon = srcPolygon;
	}
	return target.translated(-bounds.topLeft());
}

QImage SelectionTool::shapeMask(const QColor &color, const QPolygonF &selection, QRect *maskBounds)
{
	const QRectF bf = selection.boundingRect();
	const QRect b = bf.toRect();
	const QPolygonF p = selection.translated(-bf.topLeft());

	QImage mask(b.size(), QImage::Format_ARGB32_Premultiplied);
	mask.fill(0);

	QPainter painter(&mask);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawPolygon(p);

	if(maskBounds)
		*maskBounds = b;

	return mask;
}

}

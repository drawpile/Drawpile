/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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

#include "canvas/canvasmodel.h"
#include "canvas/aclfilter.h"
#include "core/layer.h"
#include "net/client.h"

#include "tools/selection.h"
#include "tools/toolcontroller.h"
#include "tools/utils.h"

#include <QPixmap>
#include <QtMath>
#include <QPolygonF>
#include <QTransform>
#include <QPainter>

namespace tools {

void SelectionTool::begin(const paintcore::Point &point, bool right, float zoom)
{
	Q_UNUSED(right);

	canvas::Selection *sel = m_allowTransform ? owner.model()->selection() : nullptr;
	if(sel)
		m_handle = sel->handleAt(point, zoom);
	else
		m_handle = canvas::Selection::Handle::Outside;

	m_start = point;
	m_p1 = point;
	m_end = point;

	if(m_handle == canvas::Selection::Handle::Outside) {
		if(sel) {
			owner.client()->sendMessages(sel->pasteOrMoveToCanvas(owner.client()->myId(), owner.activeLayer()));
			sel->detachMove();
		}

		sel = new canvas::Selection;
		initSelection(sel);
		owner.model()->setSelection(sel);
	} else {
		sel->beginAdjustment(m_handle);
	}
}

void SelectionTool::motion(const paintcore::Point &point, bool constrain, bool center)
{
	canvas::Selection *sel = owner.model()->selection();
	if(!sel)
		return;

	m_end = point;

	if(m_handle==canvas::Selection::Handle::Outside) {
		newSelectionMotion(point, constrain, center);

	} else {
		if(sel->pasteImage().isNull() && !owner.model()->aclFilter()->isLayerLocked(owner.activeLayer())) {
			startMove();
		}

		sel->adjustGeometry(m_start, point, constrain);
	}
}

void SelectionTool::end()
{
	canvas::Selection *sel = owner.model()->selection();
	if(!sel)
		return;

	// The shape must be closed after the end of the selection operation
	if(!owner.model()->selection()->closeShape(QRectF(QPointF(), owner.model()->layerStack()->size()))) {
		// Clear selection if it was entirely outside the canvas
		owner.model()->setSelection(nullptr);
		return;
	}

	// Remove tiny selections
	QRectF selrect = sel->boundingRect();
	if(selrect.width() * selrect.height() <= 2) {
		owner.model()->setSelection(nullptr);
		return;
	}

	// Toggle adjustment mode if just clicked
	if((m_end - m_start).manhattanLength() < 2.0) {
		sel->setAdjustmentMode(
			sel->adjustmentMode() == canvas::Selection::AdjustmentMode::Scale
				? canvas::Selection::AdjustmentMode::Rotate
				: canvas::Selection::AdjustmentMode::Scale
		);
	}
}

void SelectionTool::finishMultipart()
{
	canvas::Selection *sel = owner.model()->selection();
	if(sel && !sel->pasteImage().isNull()) {
		owner.client()->sendMessages(sel->pasteOrMoveToCanvas(owner.client()->myId(), owner.activeLayer()));
		owner.model()->setSelection(nullptr);

	}
}

void SelectionTool::cancelMultipart()
{
	owner.model()->setSelection(nullptr);
}


void SelectionTool::undoMultipart()
{
	canvas::Selection *sel = owner.model()->selection();
	if(sel) {
		if(sel->isTransformed())
			sel->reset();
		else
			cancelMultipart();
	}
}

bool SelectionTool::isMultipart() const
{
	return owner.model()->selection() != nullptr;
}

void SelectionTool::startMove()
{
	canvas::Selection *sel = owner.model()->selection();
	auto layers = owner.model()->layerStack()->editor(owner.client()->myId());

	paintcore::EditableLayer layer = layers.getEditableLayer(owner.activeLayer());
	if(sel && !layer.isNull()) {
		// Get the selection shape mask (needs to be done before the shape is overwritten by setMoveImage)
		QRect maskBounds;
		QImage eraseMask = sel->shapeMask(Qt::white, &maskBounds);

		// Copy layer content into move preview buffer.
		QImage img = owner.model()->selectionToImage(owner.activeLayer());
		sel->setMoveImage(img, maskBounds, owner.model()->layerStack()->size(), owner.activeLayer());

		// The actual canvas pixels aren't touch yet, so we create a temporary sublayer
		// to erase the selected region.
		layer.removeSublayer(-1);
		auto tmplayer = layer.getEditableSubLayer(-1, paintcore::BlendMode::MODE_ERASE, 255);
		tmplayer.putImage(maskBounds.left(), maskBounds.top(), eraseMask, paintcore::BlendMode::MODE_REPLACE);
	}
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

void RectangleSelection::newSelectionMotion(const paintcore::Point& point, bool constrain, bool center)
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

	owner.model()->selection()->setShapeRect(QRectF(m_p1, p).normalized().toRect());
}

PolygonSelection::PolygonSelection(ToolController &owner)
	: SelectionTool(owner, POLYGONSELECTION, QCursor(QPixmap(":cursors/select-lasso.png"), 2, 29))
{
}

void PolygonSelection::initSelection(canvas::Selection *selection)
{
	selection->setShape(QPolygonF({ m_start }));
}

void PolygonSelection::newSelectionMotion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	Q_ASSERT(owner.model()->selection());
	owner.model()->selection()->addPointToShape(point);
}

QImage SelectionTool::transformSelectionImage(const QImage &source, const QPolygon &target, QPoint *offset)
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

	const QPolygon xTarget = target.translated(-bounds.topLeft());
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

QImage SelectionTool::shapeMask(const QColor &color, const QPolygonF &selection, QRect *maskBounds, bool mono)
{
	const QRectF bf = selection.boundingRect();
	const QRect b = bf.toRect();
	const QPolygonF p = selection.translated(-bf.topLeft());

	QImage mask(b.size(), mono ? QImage::Format_Mono : QImage::Format_ARGB32_Premultiplied);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
	memset(mask.bits(), 0, mask.byteCount()); // note: apparently image.fill() does not set every byte for monochrome images
#else
	memset(mask.bits(), 0, mask.sizeInBytes()); // note: apparently image.fill() does not set every byte for monochrome images
#endif
	QPainter painter(&mask);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawPolygon(p);

	if(maskBounds)
		*maskBounds = b;

	return mask;
}

}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/pivotitem.h"
#include "libclient/tools/transform.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

PivotItem::PivotItem(bool valid, qreal zoom, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_handleSize(calculateHandleSize(zoom))
	, m_boundingRect(calculateBoundingRect())
	, m_valid(valid)
{
	updateVisibility(shouldShow());
}

void PivotItem::setZoom(qreal zoom)
{
	int handleSize = calculateHandleSize(zoom);
	if(handleSize != m_handleSize) {
		refreshGeometry();
		m_handleSize = handleSize;
		m_boundingRect = calculateBoundingRect();
	}
}

void PivotItem::setQuadValid(bool valid)
{
	if(valid != m_valid) {
		m_valid = valid;
		setVisible(shouldShow());
	}
}

void PivotItem::setToolState(int handle, bool dragging, bool pivotShown)
{
	bool active = handle == int(tools::TransformTool::Handle::Pivot);
	if(active != m_active || dragging != m_dragging ||
	   pivotShown != m_pivotShown) {
		m_active = active;
		m_dragging = dragging;
		m_pivotShown = pivotShown;
		if(shouldShow()) {
			setVisible(true);
			refresh();
		} else {
			setVisible(false);
		}
	}
}

void PivotItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	qreal dpr = painter->device()->devicePixelRatioF();
	if(m_handlePixmap.isNull() || !qFuzzyCompare(dpr, m_dpr)) {
		m_handlePixmap = paintHandlePixmap(dpr);
		m_dpr = dpr;
	}

	QRectF rect;
	if(m_active) {
		painter->setOpacity(1.0);
		rect = m_boundingRect;
	} else {
		painter->setOpacity(0.5);
		qreal mx = m_boundingRect.width() * 0.1;
		qreal my = m_boundingRect.height() * 0.1;
		rect = m_boundingRect.marginsRemoved(QMarginsF(mx, my, mx, my));
	}

	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	painter->drawPixmap(rect, m_handlePixmap, m_handlePixmap.rect());
}

int PivotItem::calculateHandleSize(qreal zoom)
{
	return zoom > 0.0 ? qRound(tools::TransformTool::HANDLE_SIZE / zoom) : 0;
}

QRectF PivotItem::calculateBoundingRect() const
{
	return QRectF(
		m_handleSize * -0.5, m_handleSize * -0.5, m_handleSize, m_handleSize);
}

QPixmap PivotItem::paintHandlePixmap(qreal dpr)
{
	int size = qRound(qreal(tools::TransformTool::HANDLE_SIZE) * dpr);
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);

	QRectF r(pixmap.rect());
	painter.setBrush(Qt::white);
	painter.drawEllipse(r.marginsRemoved(handleMargins(dpr, 1.0)));

	painter.setBrush(Qt::black);
	painter.drawEllipse(r.marginsRemoved(handleMargins(dpr, 2.5)));

	painter.setBrush(Qt::white);
	painter.drawEllipse(r.marginsRemoved(handleMargins(dpr, 5.0)));

	painter.setCompositionMode(QPainter::CompositionMode_Clear);
	painter.drawEllipse(r.marginsRemoved(handleMargins(dpr, 6.5)));

	return pixmap;
}

}

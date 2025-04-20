// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/selectionitem.h"
#include "libclient/utils/selectionoutlinegenerator.h"
#include <QMarginsF>
#include <QPainter>
#include <QThreadPool>
#include <cmath>

namespace drawingboard {

SelectionItem::SelectionItem(
	bool ignored, bool showMask, qreal zoom, QGraphicsItem *parent)
	: BaseObject(parent)
	, m_zoom(zoom)
	, m_ignored(ignored)
	, m_showMask(showMask)
{
}

QRectF SelectionItem::boundingRect() const
{
	return m_boundingRect;
}

void SelectionItem::setModel(const QSharedPointer<canvas::SelectionMask> &mask)
{
	++m_executionId;
	emit outlineRegenerating();
	m_bounds = mask ? mask->bounds() : QRect();
	m_path.clear();
	updateBoundingRectFromBounds();
	setPos(m_bounds.topLeft());
	if(m_bounds.isEmpty()) {
		qWarning("Selection mask is empty");
		m_mask.clear();
	} else if(m_showMask) {
		m_mask = mask;
	} else {
		m_mask.clear();
		generateOutline(mask->image());
	}
}

void SelectionItem::setTransparentDelay(qreal transparentDelay)
{
	if(m_transparentDelay != transparentDelay) {
		m_transparentDelay = transparentDelay;
		refresh();
	}
}

void SelectionItem::setIgnored(bool ignored)
{
	if(m_ignored != ignored) {
		m_ignored = ignored;
		refresh();
	}
}

void SelectionItem::setShowMask(bool showMask)
{
	if(m_showMask != showMask) {
		m_showMask = showMask;
		if(!showMask && m_mask) {
			generateOutline(m_mask->image());
			m_mask.clear();
		}
		refresh();
	}
}

void SelectionItem::setZoom(qreal zoom)
{
	if(zoom != m_zoom) {
		m_zoom = zoom;
		updateBoundingRectFromBounds();
	}
}

void SelectionItem::animationStep(qreal dt)
{
	bool havePath = !m_path.isEmpty();
	bool needsRefresh = false;

	if(havePath && !m_ignored && !m_showMask) {
		qreal prevMarchingAnts = std::floor(m_marchingAnts);
		m_marchingAnts += dt * 5.0;
		if(prevMarchingAnts != std::floor(m_marchingAnts)) {
			needsRefresh = true;
		}
	}

	if(m_transparentDelay > 0.0) {
		m_transparentDelay -= dt;
		if(m_transparentDelay <= 0.0) {
			m_transparentDelay = 0.0;
			needsRefresh = true;
		}
	}

	if(needsRefresh) {
		refresh();
	}
}

void SelectionItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *opt, QWidget *widget)
{
	Q_UNUSED(opt);
	Q_UNUSED(widget);
	if(m_transparentDelay <= 0.0 && !m_showMask && !m_path.isEmpty()) {
		if(!m_path.isEmpty()) {
			QPen pen;
			pen.setWidth(painter->device()->devicePixelRatioF());
			pen.setCosmetic(true);
			pen.setColor(m_ignored ? Qt::darkGray : Qt::black);
			painter->setPen(pen);
			painter->drawPath(m_path);
			pen.setDashPattern({4.0, 4.0});
			if(m_ignored) {
				pen.setColor(Qt::lightGray);
			} else {
				pen.setColor(Qt::white);
				pen.setDashOffset(m_marchingAnts);
			}
			painter->setPen(pen);
			painter->drawPath(m_path);
		}
	}
}

void SelectionItem::generateOutline(const QImage &mask)
{
	SelectionOutlineGenerator *gen =
		new SelectionOutlineGenerator(m_executionId, mask, false, 0, 0);
	connect(
		this, &SelectionItem::outlineRegenerating, gen,
		&SelectionOutlineGenerator::cancel, Qt::DirectConnection);
	connect(
		gen, &SelectionOutlineGenerator::outlineGenerated, this,
		&SelectionItem::setOutline, Qt::QueuedConnection);
	gen->setAutoDelete(true);
	QThreadPool::globalInstance()->start(gen);
}

void SelectionItem::setOutline(
	unsigned int executionId, const SelectionOutlinePath &path)
{
	if(m_executionId == executionId) {
		m_path = path.value;
		refresh();
	}
}

void SelectionItem::updateBoundingRectFromBounds()
{
	refreshGeometry();
	qreal m = m_zoom > 0.0 ? std::ceil(1.0 / m_zoom) : 1.0;
	m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_bounds.size()))
						 .marginsAdded(QMarginsF(m, m, m, m));
}

}

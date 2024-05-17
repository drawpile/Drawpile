// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/selectionitem.h"
#include "libclient/utils/selectionoutlinegenerator.h"
#include <QPainter>
#include <QThreadPool>
#include <cmath>

namespace drawingboard {

SelectionItem::SelectionItem(bool ignored, QGraphicsItem *parent)
	: BaseObject(parent)
	, m_ignored(ignored)
{
}

QRectF SelectionItem::boundingRect() const
{
	return m_boundingRect;
}

void SelectionItem::setModel(const QRect &bounds, const QImage &mask)
{
	++m_executionId;
	emit outlineRegenerating();
	m_bounds = bounds;
	m_mask = mask;
	m_path.clear();
	m_maskOpacity = 0.0;
	updateBoundingRectFromBounds();
	if(m_bounds.isEmpty()) {
		qWarning("Selection mask is empty");
	} else {
		SelectionOutlineGenerator *gen =
			new SelectionOutlineGenerator(m_executionId, m_mask, false, 0, 0);
		connect(
			this, &SelectionItem::outlineRegenerating, gen,
			&SelectionOutlineGenerator::cancel, Qt::DirectConnection);
		connect(
			gen, &SelectionOutlineGenerator::outlineGenerated, this,
			&SelectionItem::setOutline, Qt::QueuedConnection);
		gen->setAutoDelete(true);
		QThreadPool::globalInstance()->start(gen);
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

void SelectionItem::animationStep(qreal dt)
{
	bool havePath = !m_path.isEmpty();
	bool haveMask = !m_mask.isNull();
	bool needsRefresh = false;

	if(haveMask) {
		if(havePath) {
			m_maskOpacity -= dt * 5.0;
			needsRefresh = true;
			if(m_maskOpacity <= 0.0) {
				m_mask = QImage();
			}
		} else if(m_maskOpacity < 1.0) {
			// Generating a mask for a large selection can take a while, which
			// leads to the animation lurching ahead. We mitigate that by
			// checking if we're at the start of the fade-in.
			m_maskOpacity = m_maskOpacity == 0.0
								? 0.001
								: qMin(1.0, m_maskOpacity + dt * 5.0);
			needsRefresh = true;
		}
	}

	if(havePath && !m_ignored) {
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
	if(m_transparentDelay <= 0.0) {
		if(m_maskOpacity > 0.01) {
			qreal opa = m_maskOpacity * m_maskOpacity;
			painter->setOpacity(opa * 0.5);
			painter->drawImage(QPointF(0.0, 0.0), m_mask);
			painter->setOpacity((1.0 - opa));
		}

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
	m_boundingRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_bounds.size()));
	setPos(m_bounds.topLeft());
}

}

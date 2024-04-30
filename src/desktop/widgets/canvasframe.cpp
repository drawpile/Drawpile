// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/canvasframe.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/rulerwidget.h"
#include <QAbstractScrollArea>
#include <QGridLayout>
#include <cmath>

namespace widgets {

CanvasFrame::CanvasFrame(QAbstractScrollArea *canvasView, QWidget *parent)
	: QWidget(parent)
	, m_canvasView(canvasView)
{
	m_hRuler = new widgets::RulerWidget(this);
	m_vRuler = new widgets::RulerWidget(this);
	m_hRuler->setFixedHeight(RULER_WIDTH);
	m_vRuler->setFixedWidth(RULER_WIDTH);
	m_hRuler->hide();
	m_vRuler->hide();
	QGridLayout *grid = new QGridLayout(this);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(0);
	grid->addWidget(m_hRuler, 0, 1);
	grid->addWidget(m_vRuler, 1, 0);
	grid->addWidget(m_canvasView, 1, 1);
}

void CanvasFrame::setShowRulers(bool showRulers)
{
	utils::ScopedUpdateDisabler disabler(this);
	m_hRuler->setVisible(showRulers);
	m_vRuler->setVisible(showRulers);
}

void CanvasFrame::setTransform(qreal zoom, qreal angle)
{
	Q_UNUSED(zoom);
	bool isRotated = std::abs(angle) > 0.001;
	m_hRuler->setIsRotated(isRotated);
	m_vRuler->setIsRotated(isRotated);
}

void CanvasFrame::setView(const QPolygonF &view)
{
	QRectF viewBounds = view.boundingRect();
	QSize viewportSize = m_canvasView->viewport()->size();
	m_hRuler->setCanvasToRulerTransform(
		transformScale(
			viewBounds.left(), viewBounds.right(), viewportSize.width()),
		viewBounds);
	m_vRuler->setCanvasToRulerTransform(
		transformScale(
			viewBounds.top(), viewBounds.bottom(), viewportSize.height()),
		viewBounds);
}

qreal CanvasFrame::transformScale(qreal min, qreal max, int viewWidth)
{
	qreal dist = std::abs(max - min);
	return viewWidth / dist;
}

}

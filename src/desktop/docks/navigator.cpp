/*
   Copyright (C) 2008-2019 Calle Laakkonen, 2007 M.K.A.

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

#include "navigator.h"
using docks::NavigatorView;
#include "ui_navigator.h"
#include "docks/utils.h"
#include "scene/canvasscene.h"

#include <QMouseEvent>
#include <QWindow>

namespace docks {

NavigatorView::NavigatorView(QWidget *parent)
	: QGraphicsView(parent), m_zoomWheelDelta(0), m_dragging(false)
{
	viewport()->setMouseTracking(true);
	setInteractive(false);

	setResizeAnchor(QGraphicsView::AnchorViewCenter);
	setAlignment(Qt::AlignCenter);
	
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	
	setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

	setBackgroundBrush(QColor(100, 100, 100));
}


void NavigatorView::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);
	rescale();
}

/**
 * Start dragging the view focus
 */
void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	emit focusMoved(mapToScene(event->pos()).toPoint());
	m_dragging = true;
}

/**
 * Drag the view focus
 */
void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	if(m_dragging)
		emit focusMoved(mapToScene(event->pos()).toPoint());
}

/**
 * Stop dragging the view focus
 */
void NavigatorView::mouseReleaseEvent(QMouseEvent *event)
{
	Q_UNUSED(event);
	m_dragging = false;
}

void NavigatorView::wheelEvent(QWheelEvent *event)
{
	// Use scroll wheel for zooming
	m_zoomWheelDelta += event->angleDelta().y();
	const int steps = m_zoomWheelDelta / 120;
	m_zoomWheelDelta -= steps * 120;

	if(steps != 0) {
		emit wheelZoom(steps);
	}
}

/**
 * The focus rectangle represents the visible area in the
 * main viewport.
 */
void NavigatorView::setViewFocus(const QPolygonF& rect)
{
	QRegion up = mapFromScene(rect).boundingRect().adjusted(-1,-1,1,1);
	up |= mapFromScene(m_focusRect).boundingRect().adjusted(-1,-1,1,1);
	
	m_focusRect = rect;
	
	viewport()->update(up);
}

/**
 * Reset the navigator view scale
 */
void NavigatorView::rescale()
{
	resetTransform();

	const qreal padding = drawingboard::CanvasScene::MARGIN; // ignore scene padding
	const QRectF ss = scene()->sceneRect().adjusted(padding, padding, -padding, -padding);

	const qreal x = qreal(width()) / ss.width();
	const qreal y = qreal(height()-5) / ss.height();
	const qreal min = qMin(x, y);

	scale(min, min);
	centerOn(scene()->sceneRect().center());
	viewport()->update();
}

void NavigatorView::drawForeground(QPainter *painter, const QRectF& rect)
{
	Q_UNUSED(rect);
	QPen pen(QColor(96, 191, 96));
	pen.setCosmetic(true);
	pen.setWidth(2);
	painter->setPen(pen);
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	painter->drawPolygon(m_focusRect);
}

/**
 * Construct the navigator dock widget.
 */
Navigator::Navigator(QWidget *parent)
	: QDockWidget(tr("Navigator"), parent), m_ui(new Ui_Navigator), m_updating(false)
{
	setObjectName("navigatordock");
	setStyleSheet(defaultDockStylesheet());

	auto w = new QWidget(this);
	m_ui->setupUi(w);
	setWidget(w);

	connect(m_ui->view, &NavigatorView::focusMoved, this, &Navigator::focusMoved);
	connect(m_ui->view, &NavigatorView::wheelZoom, this, &Navigator::wheelZoom);
	connect(m_ui->zoomBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Navigator::updateZoom);
	connect(m_ui->rotationBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &Navigator::updateAngle);
	connect(m_ui->zoomReset, &QToolButton::clicked, this, [this]() { emit zoomChanged(100.0); });
	connect(m_ui->rotateReset, &QToolButton::clicked, this, [this]() { emit angleChanged(0); });
}

Navigator::~Navigator()
{
	delete m_ui;
}

void Navigator::setFlipActions(QAction *flip, QAction *mirror)
{
	m_ui->flip->setDefaultAction(flip);
	m_ui->mirror->setDefaultAction(mirror);
}

void Navigator::setScene(QGraphicsScene *scene)
{
	connect(scene, &QGraphicsScene::sceneRectChanged, m_ui->view, &NavigatorView::rescale);
	m_ui->view->setScene(scene);
	m_ui->view->rescale();
}

void Navigator::setViewFocus(const QPolygonF& rect)
{
	m_ui->view->setViewFocus(rect);
}

void Navigator::updateZoom(int value)
{
	if(!m_updating)
		emit zoomChanged(value);
}

void Navigator::updateAngle(int value)
{
	if(!m_updating)
		emit angleChanged(value);
}

void Navigator::setViewTransformation(qreal zoom, qreal angle)
{
	m_updating = true;
	m_ui->zoomBox->setValue(zoom);
	m_ui->rotationBox->setValue(angle);
	m_updating = false;
}

}


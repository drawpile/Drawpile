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

#include "core/layerstackpixmapcacheobserver.h"
#include "core/layerstack.h"

#include <QMouseEvent>
#include <QTimer>
#include <QPainter>
#include <QDebug>

namespace docks {

NavigatorView::NavigatorView(QWidget *parent)
	: QWidget(parent), m_observer(nullptr), m_zoomWheelDelta(0)
{
	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	m_refreshTimer->setInterval(500);
	connect(m_refreshTimer, &QTimer::timeout, this, &NavigatorView::refreshCache);
}

void NavigatorView::setLayerStackObserver(paintcore::LayerStackPixmapCacheObserver *observer)
{
	m_observer = observer;
	connect(m_observer, &paintcore::LayerStackPixmapCacheObserver::areaChanged, this, &NavigatorView::onChange);
	connect(m_observer, &paintcore::LayerStackPixmapCacheObserver::areaChanged, this, &NavigatorView::onChange);
	refreshCache();
}


void NavigatorView::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	onChange();
}

/**
 * Start dragging the view focus
 */
void NavigatorView::mousePressEvent(QMouseEvent *event)
{
	if(m_cache.isNull())
		return;

	const QPoint p = event->pos();
	const qreal scale = height() / qreal(m_observer->layerStack()->height());

	const QSize s = m_cache.size().scaled(size(), Qt::KeepAspectRatio);
	const QPoint offset { width()/2 - s.width()/2, height()/2 - s.height()/2 };

	emit focusMoved((p - offset) / scale);
}

/**
 * Drag the view focus
 */
void NavigatorView::mouseMoveEvent(QMouseEvent *event)
{
	mousePressEvent(event);
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
	m_focusRect = rect;
	update();
}


void NavigatorView::onChange()
{
	if(!m_refreshTimer->isActive())
		m_refreshTimer->start();
}

void NavigatorView::refreshCache()
{
	if(!m_observer->layerStack())
		return;

	const QSize size = this->size();
	if(size != m_cachedSize) {
		m_cachedSize = size;
		const QSize pixmapSize = m_observer->layerStack()->size().scaled(size, Qt::KeepAspectRatio);
		m_cache = QPixmap(pixmapSize);
	}

	QPainter painter(&m_cache);
	painter.drawPixmap(m_cache.rect(), m_observer->getPixmap());

	update();
}

void NavigatorView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(rect(), QColor(100,100,100));

	// Draw downscaled canvas
	if(m_cache.isNull())
		return;
	const QSize s = m_cache.size().scaled(size(), Qt::KeepAspectRatio);
	const QRect canvasRect {
		width()/2 - s.width()/2,
		height()/2 - s.height()/2,
		s.width(),
		s.height()
	};
	painter.drawPixmap(canvasRect, m_cache);

	// Draw main viewport rectangle
	QPen pen(QColor(96, 191, 96));
	pen.setCosmetic(true);
	pen.setWidth(2);
	painter.setPen(pen);
	painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);

	const qreal scale = height() / qreal(m_observer->layerStack()->height());
	painter.translate(canvasRect.topLeft());
	painter.scale(scale, scale);
	painter.drawPolygon(m_focusRect);

	// Draw a short line to indicate the righthand side
	if(qAbs(m_focusRect[0].y() - m_focusRect[1].y()) >= 1.0 || m_focusRect[0].x() > m_focusRect[1].x()) {
		const QLineF right { m_focusRect[1], m_focusRect[2] };
		const QLineF unitVector = right.unitVector().translated(-right.p1());
		QLineF normal = unitVector.normalVector().translated(right.center());
		normal.setLength(10 / scale);
		painter.drawLine(normal);
	}
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

void Navigator::setScene(drawingboard::CanvasScene *scene)
{
	m_ui->view->setLayerStackObserver(scene->layerStackObserver());
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


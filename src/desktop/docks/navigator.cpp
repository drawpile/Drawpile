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

#include "canvas/usercursormodel.h"

#include <QMouseEvent>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QAction>
#include <QSettings>

namespace docks {

static QPixmap makeCursorBackground(const int avatarSize)
{
	const int PADDING = 4;
	const int ARROW = 4;
	const QSize s { avatarSize + PADDING*2, avatarSize + PADDING*2 + ARROW };

	QPixmap pixmap(s);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);

	QPainterPath p(QPointF(s.width()/2, s.height()));
	p.lineTo(s.width()/2 + ARROW, s.height() - ARROW);
	p.lineTo(s.width() - PADDING, s.height() - ARROW);
	p.quadTo(s.width(), s.height() - ARROW, s.width(), s.height() - ARROW - PADDING);
	p.lineTo(s.width(), PADDING);
	p.quadTo(s.width(), 0, s.width() - PADDING, 0);
	p.lineTo(PADDING, 0);
	p.quadTo(0, 0, 0, PADDING);
	p.lineTo(0, s.height() - PADDING - ARROW);
	p.quadTo(0, s.height() - ARROW, PADDING, s.height() - ARROW);
	p.lineTo(s.width()/2 - ARROW, s.height() - ARROW);
	p.closeSubpath();

	painter.fillPath(p, Qt::black);

	return pixmap;
}

NavigatorView::NavigatorView(QWidget *parent)
	: QWidget(parent), m_observer(nullptr), m_cursors(nullptr), m_zoomWheelDelta(0),
	  m_showCursors(true)
{
	m_refreshTimer = new QTimer(this);
	m_refreshTimer->setSingleShot(true);
	setRealtimeUpdate(false);
	connect(m_refreshTimer, &QTimer::timeout, this, &NavigatorView::refreshCache);

	// Draw the marker background
	m_cursorBackground = makeCursorBackground(16);
}

void NavigatorView::setLayerStackObserver(paintcore::LayerStackPixmapCacheObserver *observer)
{
	m_observer = observer;
	connect(m_observer, &paintcore::LayerStackPixmapCacheObserver::areaChanged, this, &NavigatorView::onChange);
	connect(m_observer, &paintcore::LayerStackPixmapCacheObserver::areaChanged, this, &NavigatorView::onChange);
	refreshCache();
}

void NavigatorView::setShowCursors(bool show)
{
	m_showCursors = show;
	update();
}

void NavigatorView::setRealtimeUpdate(bool realtime)
{
	m_refreshTimer->setInterval(realtime ? 1 : 500);
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
	if(event->button() == Qt::RightButton)
		return;

	if(m_cache.isNull())
		return;

	const QPoint p = event->pos();

	const QSize s = m_cache.size().scaled(size(), Qt::KeepAspectRatio);

	const qreal xscale = s.width() / qreal(m_observer->layerStack()->width());
	const qreal yscale = s.height() / qreal(m_observer->layerStack()->height());

	const QPoint offset { width()/2 - s.width()/2, height()/2 - s.height()/2 };

	emit focusMoved(QPoint(
		(p.x() - offset.x()) / xscale,
		(p.y() - offset.y()) / yscale
	));
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
	if(isVisible() && !m_refreshTimer->isActive())
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
	painter.save();

	QPen pen(QColor(96, 191, 96));
	pen.setCosmetic(true);
	pen.setWidth(2);
	painter.setPen(pen);
	painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);

	const qreal xscale = s.width() / qreal(m_observer->layerStack()->width());
	const qreal yscale = s.height() / qreal(m_observer->layerStack()->height());
	painter.translate(canvasRect.topLeft());
	painter.scale(xscale, yscale);
	painter.drawPolygon(m_focusRect);

	// Draw a short line to indicate the righthand side
	if(qAbs(m_focusRect[0].y() - m_focusRect[1].y()) >= 1.0 || m_focusRect[0].x() > m_focusRect[1].x()) {
		const QLineF right { m_focusRect[1], m_focusRect[2] };
		const QLineF unitVector = right.unitVector().translated(-right.p1());
		QLineF normal = unitVector.normalVector().translated(right.center());
		normal.setLength(10 / xscale);
		painter.drawLine(normal);
	}

	painter.restore();
	// Draw user cursors
	if(m_cursors && m_showCursors) {
		const int cursorCount = m_cursors->rowCount();
		for(int i=0;i<cursorCount;++i) {
			const QModelIndex idx = m_cursors->index(i);
			if(!idx.data(canvas::UserCursorModel::VisibleRole).toBool())
				continue;

			const QPixmap avatar = idx.data(Qt::DecorationRole).value<QPixmap>();
			const QPoint pos = idx.data(canvas::UserCursorModel::PositionRole).toPoint();
			const QPoint viewPoint = QPoint(
				pos.x() * xscale + canvasRect.x() - m_cursorBackground.width() / 2,
				pos.y() * yscale + canvasRect.y() - m_cursorBackground.height()
			);

			painter.drawPixmap(viewPoint, m_cursorBackground);
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
			painter.drawPixmap(
				QRect(
					viewPoint + QPoint(
						m_cursorBackground.width()/2 - avatar.width()/4,
						m_cursorBackground.width()/2 - avatar.height()/4
					),
					avatar.size() / 2
				),
				avatar
			);
		}
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
	connect(m_ui->zoomReset, &QToolButton::clicked, this, [this]() { emit zoomChanged(100.0); });
	connect(m_ui->zoom, &QSlider::valueChanged, this, &Navigator::updateZoom);

	QAction *showCursorsAction = new QAction(tr("Show Cursors"), m_ui->view);
	showCursorsAction->setCheckable(true);
	m_ui->view->addAction(showCursorsAction);

	QAction *realtimeUpdateAction = new QAction(tr("Realtime Update"), m_ui->view);
	realtimeUpdateAction->setCheckable(true);
	m_ui->view->addAction(realtimeUpdateAction);

	m_ui->view->setContextMenuPolicy(Qt::ActionsContextMenu);

	QSettings cfg;
	cfg.beginGroup("navigator");

	showCursorsAction->setChecked(cfg.value("showcursors", true).toBool());
	m_ui->view->setShowCursors(showCursorsAction->isChecked());

	realtimeUpdateAction->setChecked(cfg.value("realtime", false).toBool());
	m_ui->view->setRealtimeUpdate(realtimeUpdateAction->isChecked());

	connect(showCursorsAction, &QAction::triggered, this, [this](bool show) {
		QSettings().setValue("navigator/showcursors", show);
		m_ui->view->setShowCursors(show);
	});
	connect(realtimeUpdateAction, &QAction::triggered, this, [this](bool realtime) {
		QSettings().setValue("navigator/realtime", realtime);
		m_ui->view->setRealtimeUpdate(realtime);
	});
}

Navigator::~Navigator()
{
	QSettings cfg;
	delete m_ui;
}

void Navigator::setScene(drawingboard::CanvasScene *scene)
{
	m_ui->view->setLayerStackObserver(scene->layerStackObserver());
}

void Navigator::setUserCursors(canvas::UserCursorModel *cursors)
{
	m_ui->view->setUserCursors(cursors);
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

void Navigator::setViewTransformation(qreal zoom, qreal angle)
{
	m_updating = true;
	m_ui->zoom->setValue(zoom);
	m_updating = false;
}

}


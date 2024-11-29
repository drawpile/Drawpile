// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/referenceview.h"
#include "libclient/settings.h"
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPixmap>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QTransform>

namespace widgets {

ReferenceView::ReferenceView(QWidget *parent)
	: QGraphicsView(parent)
{
	QGraphicsScene *s = new QGraphicsScene(this);
	setScene(s);
	setEnabled(false);
}

void ReferenceView::setImage(const QImage &img)
{
	if(m_item) {
		scene()->removeItem(m_item);
		delete m_item;
		m_item = nullptr;
	}

	QPixmap pixmap;
	if(!img.isNull() && !(pixmap = QPixmap::fromImage(img)).isNull()) {
		m_item = new QGraphicsPixmapItem(pixmap);
		m_item->setTransformationMode(Qt::SmoothTransformation);
		scene()->addItem(m_item);
	}

	zoomToFit();
}

void ReferenceView::scrollBy(int x, int y)
{
	scrollByF(x, y);
}

void ReferenceView::scrollByF(qreal x, qreal y)
{
	m_pos.setX(m_pos.x() + x);
	m_pos.setY(m_pos.y() + y);
	updateTransform();
}

void ReferenceView::setZoom(qreal zoom)
{
	if(m_item && m_zoom != zoom) {
		m_zoom = zoom;
		updateTransform();
		emit zoomChanged(zoom);
	}
}

void ReferenceView::resetZoom()
{
	if(m_item) {
		qreal dpr = devicePixelRatioF();
		m_pos =
			QRectF(QPointF(), QSizeF(m_item->pixmap().size()) / dpr).center();
		m_zoom = 1.0;
		updateTransform();
		emit zoomChanged(1.0);
	} else {
		m_zoom = 1.0;
		emit zoomChanged(1.0);
	}
}

void ReferenceView::zoomToFit()
{
	if(m_item) {
		QWidget *vp = viewport();
		qreal dpr = devicePixelRatioF();
		QRectF r = QRectF(QPointF(), QSizeF(m_item->pixmap().size()) / dpr);
		qreal xScale = qreal(vp->width()) / r.width();
		qreal yScale = qreal(vp->height()) / r.height();
		qreal scale = qMin(xScale, yScale);
		m_pos = r.center();
		m_zoom = 1.0;
		updateZoomAt(scale, m_pos * dpr);
		updateTransform();
		emit zoomChanged(scale);
	} else {
		m_zoom = 1.0;
		emit zoomChanged(1.0);
	}
}

qreal ReferenceView::zoomMin()
{
	return libclient::settings::zoomMin;
}

qreal ReferenceView::zoomMax()
{
	return libclient::settings::zoomMax;
}

void ReferenceView::scrollContentsBy(int dx, int dy)
{
	if(!m_scrollBarsAdjusting) {
		scrollBy(-dx, -dy);
	}
}

void ReferenceView::updateZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(zoomMin(), zoom, zoomMax());
	m_pos += point * (actualZoomFor(newZoom) - actualZoom());
	m_zoom = newZoom;
}

void ReferenceView::updateTransform()
{
	if(m_item) {
		updatePosBounds();
		QTransform tf = calculateTransform();
		m_item->setTransform(tf);
		updateScrollBars();
	}
}

QTransform ReferenceView::calculateTransform() const
{
	return calculateTransformFrom(m_pos, m_zoom);
}

QTransform
ReferenceView::calculateTransformFrom(const QPointF &pos, qreal zoom) const
{
	QTransform matrix;
	QPointF p = mapToScene(QPoint(0, 0));
	matrix.translate(-pos.x(), -pos.y());
	qreal scale = actualZoomFor(zoom);
	matrix.scale(scale, scale);
	matrix.translate(p.x(), p.y());
	return matrix;
}

void ReferenceView::updatePosBounds()
{
	if(m_item) {
		QTransform matrix = calculateTransformFrom(QPointF(), m_zoom);
		QRectF cr(QPointF(), QSizeF(m_item->pixmap().size()));
		QRectF vr(viewport()->rect());
		m_posBounds = matrix.map(cr)
						  .boundingRect()
						  .translated(-mapToScene(QPoint(0, 0)))
						  .adjusted(-vr.width(), -vr.height(), 0.0, 0.0)
						  .marginsRemoved(QMarginsF(64.0, 64.0, 64.0, 64.0));
		clampPos();
	}
}

void ReferenceView::clampPos()
{
	if(m_posBounds.isValid()) {
		m_pos.setX(qBound(m_posBounds.left(), m_pos.x(), m_posBounds.right()));
		m_pos.setY(qBound(m_posBounds.top(), m_pos.y(), m_posBounds.bottom()));
	}
}

void ReferenceView::updateScrollBars()
{
	QScopedValueRollback<bool> guard(m_scrollBarsAdjusting, true);
	QScrollBar *hbar = horizontalScrollBar();
	QScrollBar *vbar = verticalScrollBar();
	if(m_item && m_posBounds.isValid()) {
		QRect page = m_item->transform()
						 .inverted()
						 .mapToPolygon(viewport()->rect())
						 .boundingRect();

		hbar->setRange(m_posBounds.left(), m_posBounds.right());
		hbar->setValue(m_pos.x());
		hbar->setPageStep(page.width() * m_zoom);
		hbar->setSingleStep(qMax(1, hbar->pageStep() / 20));

		vbar->setRange(m_posBounds.top(), m_posBounds.bottom());
		vbar->setValue(m_pos.y());
		vbar->setPageStep(page.height() * m_zoom);
		vbar->setSingleStep(qMax(1, vbar->pageStep() / 20));
	} else {
		hbar->setRange(0, 0);
		vbar->setRange(0, 0);
	}
}

}

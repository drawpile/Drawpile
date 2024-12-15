// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/referenceview.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/settings.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QWheelEvent>

namespace widgets {

ReferenceView::ReferenceView(QWidget *parent)
	: QGraphicsView(parent)
#ifndef HAVE_EMULATED_BITMAP_CURSOR
	, m_colorpickcursor(
		  QPixmap(QStringLiteral(":/cursors/colorpicker.png")), 2, 29)
#endif
{
	setAcceptDrops(true);
	setFrameShape(NoFrame);
	viewport()->setAcceptDrops(true);
	viewport()->setMouseTracking(true);
	QGraphicsScene *s = new QGraphicsScene(this);
	s->setItemIndexMethod(QGraphicsScene::NoIndex);
	s->setSceneRect(QRectF(0.0, 0.0, 1.0, 1.0));
	setScene(s);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	updateCursor(Qt::KeyboardModifiers());
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
		m_item->setTransformOriginPoint(pixmap.rect().center());
		scene()->addItem(m_item);
	}

	zoomToFit();
	updateCursor(QGuiApplication::keyboardModifiers());
}

void ReferenceView::scrollBy(int x, int y)
{
	scrollByF(x, y);
}

void ReferenceView::scrollByF(qreal x, qreal y)
{
	m_pos.setX(m_pos.x() + x);
	m_pos.setY(m_pos.y() + y);
	updateCanvasTransform();
}

void ReferenceView::setZoom(qreal zoom)
{
	setZoomAt(zoom, QRectF(viewport()->rect()).center());
}

void ReferenceView::setZoomAt(qreal zoom, const QPointF &point)
{
	if(m_item && zoom != m_zoom) {
		updateZoomAt(zoom, mapToCanvasF(point));
		updateCanvasTransform();
		emit zoomChanged(zoom);
	}
}

void ReferenceView::zoomStepsAt(int steps, const QPointF &point)
{
	constexpr qreal eps = 1e-5;
	const QVector<qreal> &zoomLevels = libclient::settings::zoomLevels();
	// This doesn't actually take the number of steps into account, it just
	// zooms by a single step. But that works really well, so I'll leave it be.
	if(steps > 0) {
		int i = 0;
		while(i < zoomLevels.size() - 1 && m_zoom > zoomLevels[i] - eps) {
			i++;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom > level - eps ? zoomMax() : qMax(m_zoom, level);
		setZoomAt(zoom, point);
	} else if(steps < 0) {
		int i = zoomLevels.size() - 1;
		while(i > 0 && m_zoom < zoomLevels[i] + eps) {
			i--;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom < level + eps ? zoomMin() : qMin(m_zoom, level);
		setZoomAt(zoom, point);
	}
}

void ReferenceView::resetZoom()
{
	if(m_item) {
		setZoom(1.0);
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
		updateCanvasTransform();
		emit zoomChanged(scale);
	} else {
		m_zoom = 1.0;
		emit zoomChanged(1.0);
	}
}

void ReferenceView::setInteractionMode(InteractionMode interactionMode)
{
	if(interactionMode != m_interactionMode) {
		m_interactionMode = interactionMode;
		updateCursor(QGuiApplication::keyboardModifiers());
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

void ReferenceView::resizeEvent(QResizeEvent *event)
{
	{
		QScopedValueRollback<bool> guard(m_transformAdjusting, true);
		QGraphicsView::resizeEvent(event);
	}
	if(!event->size().isEmpty()) {
		updateCanvasTransform();
	}
}

void ReferenceView::mouseMoveEvent(QMouseEvent *event)
{
	event->accept();
	if(m_activeInteractionMode == InteractionMode::Pan) {
		QPointF posf = compat::mousePosition(*event);
		scrollByF(
			m_dragLastPoint.x() - posf.x(), m_dragLastPoint.y() - posf.y());
		m_dragLastPoint = posf;
	} else if(m_activeInteractionMode == InteractionMode::Pick) {
		pickColorAt(compat::mousePosition(*event));
	}
	updateCursor(event->modifiers());
}

void ReferenceView::mousePressEvent(QMouseEvent *event)
{
	event->accept();
	Qt::KeyboardModifiers mods = event->modifiers();
	if(!m_item) {
		emit openRequested();
	} else if(m_activeInteractionMode == InteractionMode::None) {
		switch(event->button()) {
		case Qt::LeftButton:
			m_activeInteractionMode = getInteractionModeFor(mods, false);
			break;
		case Qt::MiddleButton:
			m_activeInteractionMode = InteractionMode::Pan;
			break;
		case Qt::RightButton:
			m_activeInteractionMode = getInteractionModeFor(mods, true);
			break;
		default:
			m_activeInteractionMode = InteractionMode::None;
			break;
		}

		switch(m_activeInteractionMode) {
		case InteractionMode::Pan:
			m_dragLastPoint = compat::mousePosition(*event);
			break;
		case InteractionMode::Pick:
			pickColorAt(compat::mousePosition(*event));
			break;
		default:
			break;
		}
	}
	updateCursor(mods);
}

void ReferenceView::mouseReleaseEvent(QMouseEvent *event)
{
	event->accept();
	m_activeInteractionMode = InteractionMode::None;
	updateCursor(event->modifiers());
}

void ReferenceView::mouseDoubleClickEvent(QMouseEvent *event)
{
	mousePressEvent(event);
}

void ReferenceView::keyPressEvent(QKeyEvent *event)
{
	QGraphicsView::keyPressEvent(event);
	updateCursor(event->modifiers());
}

void ReferenceView::keyReleaseEvent(QKeyEvent *event)
{
	QGraphicsView::keyReleaseEvent(event);
	updateCursor(event->modifiers());
}

void ReferenceView::wheelEvent(QWheelEvent *event)
{
	Qt::KeyboardModifiers mods = event->modifiers();
	if(m_item) {
		bool controlHeld = mods.testFlag(Qt::ControlModifier);
#ifdef Q_OS_MAC
		bool shouldZoom = controlHeld;
#else
		bool shouldZoom = !controlHeld;
#endif
		if(shouldZoom) {
			int dy = event->angleDelta().y();
			if(dy < 0) {
				zoomStepsAt(-1, compat::wheelPosition(*event));
			} else if(dy > 0) {
				zoomStepsAt(1, compat::wheelPosition(*event));
			}
		} else {
			if(mods.testFlag(Qt::ShiftModifier)) {
				QPoint delta = event->angleDelta();
				scrollByF(-delta.y(), -delta.x());
			} else {
				QPoint delta = event->angleDelta();
				scrollByF(-delta.x(), -delta.y());
			}
		}
	}
	updateCursor(mods);
}

void ReferenceView::dragEnterEvent(QDragEnterEvent *event)
{
	if(canHandleDrop(event->mimeData())) {
		event->acceptProposedAction();
	}
}

void ReferenceView::dragMoveEvent(QDragMoveEvent *event)
{
	if(canHandleDrop(event->mimeData())) {
		event->acceptProposedAction();
	}
}

void ReferenceView::dragLeaveEvent(QDragLeaveEvent *event)
{
	Q_UNUSED(event);
}

void ReferenceView::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	if(canHandleDrop(mimeData)) {
		if(mimeData->hasImage()) {
			emit imageDropped(mimeData->imageData().value<QImage>());
		} else if(mimeData->hasUrls()) {
			emit pathDropped(mimeData->urls().first().toLocalFile());
		}
		event->acceptProposedAction();
	}
}

void ReferenceView::scrollContentsBy(int dx, int dy)
{
	if(!m_transformAdjusting) {
		scrollBy(-dx, -dy);
	}
}
void ReferenceView::pickColorAt(const QPointF &posf)
{
	if(m_item) {
		QPoint point = mapToCanvasF(posf).toPoint();
		QPixmap pixmap = m_item->pixmap();
		if(pixmap.rect().contains(point)) {
			QColor color = pixmap.copy(QRect(point.x(), point.y(), 1, 1))
							   .toImage()
							   .pixelColor(QPoint(0, 0));
			if(color.alpha() > 0) {
				color.setAlpha(255);
				emit colorPicked(color);
			}
		}
	}
}

bool ReferenceView::canHandleDrop(const QMimeData *mimeData)
{
	if(mimeData) {
		if(mimeData->hasImage()) {
			return true;
		} else if(mimeData->hasUrls() && !mimeData->urls().isEmpty()) {
			QUrl url = mimeData->urls().first();
			if(url.isLocalFile()) {
				QFileInfo info(url.toLocalFile());
				if(QImageReader::supportedImageFormats().contains(
					   info.suffix().toCaseFolded().toUtf8())) {
					return true;
				}
			}
		}
	}
	return false;
}

void ReferenceView::updateZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(zoomMin(), zoom, zoomMax());
	m_pos += point * (actualZoomFor(newZoom) - actualZoom());
	m_zoom = newZoom;
}

void ReferenceView::updateCanvasTransform()
{
	if(m_item) {
		QScopedValueRollback<bool> guard(m_transformAdjusting, true);
		updateRenderHints();
		updatePosBounds();
		QTransform tf = calculateCanvasTransform();
		m_item->setTransform(tf);
		updateScrollBars();
	}
}

QTransform ReferenceView::calculateCanvasTransform() const
{
	return calculateCanvasTransformFrom(m_pos, m_zoom);
}

QTransform ReferenceView::calculateCanvasTransformFrom(
	const QPointF &pos, qreal zoom) const
{
	QTransform matrix;
	matrix.translate(-pos.x(), -pos.y());
	qreal scale = actualZoomFor(zoom);
	matrix.scale(scale, scale);
	return matrix;
}

QPointF ReferenceView::mapToCanvas(const QPoint &point) const
{
	return mapToCanvasF(QPointF(point));
}

QPointF ReferenceView::mapToCanvasF(const QPointF &point) const
{
	QTransform matrix = toCanvasTransform();
	return matrix.map(point + mapToScene(QPoint(0, 0)));
}

QTransform ReferenceView::toCanvasTransform() const
{
	return m_item ? m_item->transform().inverted() : QTransform();
}

void ReferenceView::updatePosBounds()
{
	if(m_item) {
		QTransform matrix = calculateCanvasTransformFrom(QPointF(), m_zoom);
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

void ReferenceView::updateRenderHints()
{
	bool smooth = m_zoom <= 1.99 || qAbs(m_zoom - 1.0) < 0.01;
	setRenderHint(QPainter::SmoothPixmapTransform, smooth);
	if(m_item) {
		m_item->setTransformationMode(
			smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
	}
}

void ReferenceView::updateScrollBars()
{
	QScrollBar *hbar = horizontalScrollBar();
	QScrollBar *vbar = verticalScrollBar();
	if(m_item && m_posBounds.isValid()) {
		QRect page =
			toCanvasTransform().mapToPolygon(viewport()->rect()).boundingRect();

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

void ReferenceView::updateCursor(Qt::KeyboardModifiers mods)
{
	if(m_item) {
		if(getEffectiveInteractionMode(mods) == InteractionMode::Pan) {
			setCursor(
				m_activeInteractionMode == InteractionMode::None
					? Qt::OpenHandCursor
					: Qt::ClosedHandCursor);
		} else {
#ifdef HAVE_EMULATED_BITMAP_CURSOR
			setCursor(Qt::CrossCursor);
#else
			setCursor(m_colorpickcursor);
#endif
		}
	} else {
		setCursor(Qt::PointingHandCursor);
	}
}

ReferenceView::InteractionMode
ReferenceView::getEffectiveInteractionMode(Qt::KeyboardModifiers mods) const
{
	if(m_item) {
		if(m_activeInteractionMode == InteractionMode::None) {
			return getInteractionModeFor(mods, false);
		} else {
			return m_activeInteractionMode;
		}
	} else {
		return InteractionMode::None;
	}
}

ReferenceView::InteractionMode ReferenceView::getInteractionModeFor(
	Qt::KeyboardModifiers mods, bool invert) const
{
	if(mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::ShiftModifier)) {
		invert = !invert;
	}
	if(invert) {
		if(m_interactionMode == InteractionMode::Pan) {
			return InteractionMode::Pick;
		} else {
			return InteractionMode::Pan;
		}
	} else {
		return m_interactionMode;
	}
}

}

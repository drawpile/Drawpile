// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/softwarecanvas.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/tilecache.h"
#include "libclient/view/softwarecanvasrenderer.h"
#include <QPaintEvent>
#include <QPainter>

Q_LOGGING_CATEGORY(
	lcDpSoftwareCanvas, "net.drawpile.view.canvas.software", QtWarningMsg)

namespace view {

struct SoftwareCanvas::Private {
	void updateOutline(qreal dpr)
	{
		view::CanvasScene *scene = controller->scene();
		scene->setOutline(
			controller->isOutlineVisibleInMode(), controller->outlinePos(),
			controller->rotation(),
			controller->outlineSize() * controller->zoom() / dpr,
			controller->outlineWidth(), controller->isSquareOutline());
	}

	QRegion getTileCacheDirtyRegion(canvas::TileCache &tileCache)
	{
		QRegion region;
		QRect canvasViewTileArea = controller->canvasViewTileArea();
		if(!canvasViewTileArea.isEmpty()) {
			const QTransform &tf = controller->transform();
			tileCache.eachDirtyTileReset(
				canvasViewTileArea,
				[&](const QRect &pixelRect, const void *pixels) {
					Q_UNUSED(pixels);
					QRect viewRect = tf.mapRect(pixelRect).marginsAdded(
						QMargins(1, 1, 1, 1));
					region |= viewRect;
				});
		}
		return region;
	}

	QRegion getDirtyRegions()
	{
		// The loop is necessary because the canvas size may change
		// again while we're refreshing the tile cache visible area.
		while(true) {
			bool wasResized;
			canvas::TileCache::Resize resize;
			QRegion region;
			controller->withTileCache([&](canvas::TileCache &tileCache) {
				wasResized = tileCache.getResizeReset(resize);
				if(!wasResized) {
					region = getTileCacheDirtyRegion(tileCache);
				}
			});
			if(wasResized) {
				controller->updateCanvasSize(
					resize.width, resize.height, resize.offsetX,
					resize.offsetY);
			} else {
				return region;
			}
		}
	}

	void paint(QPainter *painter, const QRect &rect, const QRegion &region)
	{
		qCDebug(
			lcDpSoftwareCanvas,
			"Paint region with %d rect(s), (%d, %d) to (%d, %d)",
			region.rectCount(), rect.left(), rect.top(), rect.right(),
			rect.bottom());

		painter->setClipRegion(region);
		renderer.paint(controller, painter, rect);
		controller->scene()->render(painter);
	}

	CanvasController *controller;
	SoftwareCanvasRenderer renderer;
	bool renderUpdateFull = false;
};

SoftwareCanvas::SoftwareCanvas(CanvasController *controller, QWidget *parent)
	: QWidget(parent)
	, d(new Private)
{
	d->controller = controller;
	controller->setCanvasWidget(this);

	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindCheckerColor1(this, &SoftwareCanvas::setCheckerColor1);
	settings.bindCheckerColor2(this, &SoftwareCanvas::setCheckerColor2);
	settings.bindRenderUpdateFull(this, &SoftwareCanvas::setRenderUpdateFull);

	connect(
		controller, &CanvasController::clearColorChanged, this,
		QOverload<>::of(&SoftwareCanvas::update));
	connect(
		controller, &CanvasController::renderSmoothChanged, this,
		QOverload<>::of(&SoftwareCanvas::update));
	connect(
		controller, &CanvasController::canvasVisibleChanged, this,
		QOverload<>::of(&SoftwareCanvas::update));
	connect(
		controller, &CanvasController::transformChanged, this,
		&SoftwareCanvas::onControllerTransformChanged);
	connect(
		controller, &CanvasController::pixelGridScaleChanged, this,
		QOverload<>::of(&SoftwareCanvas::update));
	connect(
		controller, &CanvasController::tileCacheDirtyCheckNeeded, this,
		&SoftwareCanvas::onControllerTileCacheDirtyCheckNeeded,
		Qt::DirectConnection);
	connect(
		controller, &CanvasController::outlineChanged, this,
		&SoftwareCanvas::onControllerOutlineChanged);
	connect(
		controller->scene(), &CanvasScene::changed, this,
		&SoftwareCanvas::onSceneChanged);
}

SoftwareCanvas::~SoftwareCanvas()
{
	delete d;
}

bool SoftwareCanvas::isHardware() const
{
	return false;
}

QWidget *SoftwareCanvas::asWidget()
{
	return this;
}

QSize SoftwareCanvas::viewSize() const
{
	return size();
}

QPointF SoftwareCanvas::viewToCanvasOffset() const
{
	return QPointF(0.0, 0.0);
}

QPointF SoftwareCanvas::viewTransformOffset() const
{
	QSize s = size();
	return QPointF(s.width() / 2.0, s.height() / 2.0);
}

void SoftwareCanvas::handleResize(QResizeEvent *event)
{
	qCDebug(
		lcDpSoftwareCanvas, "resize(%d, %d)", event->size().width(),
		event->size().height());
	resizeEvent(event);
	d->controller->updateViewSize();
}

void SoftwareCanvas::handlePaint(QPaintEvent *event)
{
	paintEvent(event);
}

void SoftwareCanvas::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	d->paint(&painter, event->rect(), event->region());
}

void SoftwareCanvas::setCheckerColor1(const QColor &color)
{
	d->renderer.setCheckerColor1(color);
	update();
}

void SoftwareCanvas::setCheckerColor2(const QColor &color)
{
	d->renderer.setCheckerColor2(color);
	update();
}

void SoftwareCanvas::setRenderUpdateFull(bool renderUpdateFull)
{
	d->renderUpdateFull = renderUpdateFull;
	update();
}

void SoftwareCanvas::onControllerTransformChanged()
{
	d->updateOutline(devicePixelRatioF());
	update();
}

void SoftwareCanvas::onControllerOutlineChanged()
{
	d->updateOutline(devicePixelRatioF());
}

void SoftwareCanvas::onControllerTileCacheDirtyCheckNeeded()
{
	QRegion region = d->getDirtyRegions();
	if(!region.isEmpty()) {
		if(d->renderUpdateFull) {
			update();
		} else {
			update(region);
		}
	}
}

void SoftwareCanvas::onSceneChanged(const QList<QRectF> &region)
{
	for(const QRectF &rect : region) {
		update(rect.toAlignedRect());
	}
}

}

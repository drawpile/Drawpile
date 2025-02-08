// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/softwarecanvas.h"
#include "desktop/main.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/tilecache.h"
#include <QPaintEngine>
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

	bool isCheckersVisible()
	{
		canvas::CanvasModel *canvasModel = controller->canvasModel();
		return canvasModel && canvasModel->paintEngine()->checkersVisible();
	}

	const QBrush &getCheckerBrush()
	{
		if(checkerBrush.style() == Qt::NoBrush) {
			QPixmap checkerTexture(64, 64);
			QPainter painter(&checkerTexture);
			painter.fillRect(0, 0, 32, 32, checkerColor1);
			painter.fillRect(32, 0, 32, 32, checkerColor2);
			painter.fillRect(0, 32, 32, 32, checkerColor2);
			painter.fillRect(32, 32, 32, 32, checkerColor1);
			checkerBrush.setTexture(checkerTexture);
		}
		return checkerBrush;
	}

	void paint(QPainter *painter, const QRect &rect, const QRegion &region)
	{
		qCDebug(
			lcDpSoftwareCanvas,
			"Paint region with %d rect(s), (%d, %d) to (%d, %d)",
			region.rectCount(), rect.left(), rect.top(), rect.right(),
			rect.bottom());

		painter->setPen(Qt::NoPen);
		painter->setBrush(Qt::NoBrush);
		painter->setClipRegion(region);
		painter->setCompositionMode(QPainter::CompositionMode_Source);
		painter->setRenderHint(QPainter::Antialiasing, false);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
		painter->fillRect(rect, controller->clearColor());

		bool shouldRenderSmooth = controller->shouldRenderSmooth();
		if(shouldRenderSmooth) {
			painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
		}

		bool checkersVisible = isCheckersVisible();
		bool pixelGridVisible = controller->pixelGridScale() > 0.0;
		controller->withTileCache([&](canvas::TileCache &tileCache) {
			const QPixmap *pixmap = tileCache.softwareCanvasPixmap();
			if(pixmap) {
				painter->save();
				const QTransform &tf = controller->transform();
				QRectF canvasRect = QRectF(pixmap->rect());

				if(checkersVisible) {
					painter->setBrush(getCheckerBrush());
					painter->drawPolygon(tf.map(canvasRect));
					painter->setCompositionMode(
						QPainter::CompositionMode_SourceOver);
					painter->setBrush(Qt::NoBrush);
				}

				painter->setTransform(tf);
				QRect exposed = controller->invertedTransform()
									.map(QRectF(rect))
									.boundingRect()
									.intersected(canvasRect)
									.toAlignedRect();
				painter->drawPixmap(exposed, *pixmap, exposed);

				if(pixelGridVisible) {
					QPen pen;
					const QPaintEngine *pe = painter->paintEngine();
					if(pe->hasFeature(QPaintEngine::BlendModes)) {
						pen.setColor(QColor(32, 32, 32));
						painter->setCompositionMode(
							QPainter::CompositionMode_Difference);
					} else {
						pen.setColor(QColor(160, 160, 160));
						painter->setCompositionMode(
							QPainter::CompositionMode_Source);
					}
					pen.setCosmetic(true);
					painter->setPen(pen);

					int left = exposed.left();
					int right = exposed.right();
					int top = exposed.top();
					int bottom = exposed.bottom();
					for(int x = left; x < right; ++x) {
						painter->drawLine(x, top, x, bottom);
					}
					for(int y = top; y < bottom; ++y) {
						painter->drawLine(left, y, right, y);
					}
				}

				painter->restore();
			}
		});

		if(shouldRenderSmooth) {
			painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
		}
		painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
		controller->scene()->render(painter);
	}

	CanvasController *controller;
	QColor checkerColor1;
	QColor checkerColor2;
	QBrush checkerBrush;
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
	d->checkerColor1 = color;
	d->checkerBrush = QBrush();
	update();
}

void SoftwareCanvas::setCheckerColor2(const QColor &color)
{
	d->checkerColor2 = color;
	d->checkerBrush = QBrush();
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

// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/view/softwarecanvasrenderer.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/tilecache.h"
#include "libclient/view/canvascontrollerbase.h"
#include <QPaintEngine>
#include <QPainter>
#include <QRect>
#include <QRegion>

namespace view {

void SoftwareCanvasRenderer::setCheckerColor1(const QColor &checkerColor1)
{
	if(checkerColor1 != m_checkerColor1) {
		m_checkerColor1 = checkerColor1;
		m_checkerBrush = QBrush();
	}
}

void SoftwareCanvasRenderer::setCheckerColor2(const QColor &checkerColor2)
{
	if(checkerColor2 != m_checkerColor2) {
		m_checkerColor2 = checkerColor2;
		m_checkerBrush = QBrush();
	}
}

void SoftwareCanvasRenderer::paint(
	CanvasControllerBase *controller, QPainter *painter, const QRect &rect)
{
	Q_ASSERT(controller);
	Q_ASSERT(painter);

	painter->setPen(Qt::NoPen);
	painter->setBrush(Qt::NoBrush);
	painter->setCompositionMode(QPainter::CompositionMode_Source);
	painter->setRenderHint(QPainter::Antialiasing, false);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

	painter->fillRect(rect, controller->clearColor());

	if(controller->isCanvasVisible()) {
		bool shouldRenderSmooth = controller->shouldRenderSmooth();
		if(shouldRenderSmooth) {
			painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
		}

		bool checkersVisible = isCheckersVisible(controller);
		bool pixelGridVisible = controller->pixelGridScale() > 0.0;
		controller->withTileCache([&](canvas::TileCache &tileCache) {
			using Cell = canvas::PixmapGrid::Cell;
			const QVector<Cell> *cells = tileCache.pixmapCells();
			if(cells && !cells->isEmpty()) {
				painter->save();
				const QTransform &tf = controller->transform();
				QRectF canvasRect =
					QRectF(QPointF(0.0, 0.0), QSizeF(tileCache.size()));

				if(checkersVisible) {
					painter->setBrush(getCheckerBrush());
					painter->drawPolygon(tf.map(canvasRect));
					painter->setCompositionMode(
						QPainter::CompositionMode_SourceOver);
					painter->setBrush(Qt::NoBrush);
				}

				painter->setTransform(tf);
				QRectF exposedBase = controller->invertedTransform()
										 .map(QRectF(rect))
										 .boundingRect();

				for(const Cell &cell : *cells) {
					QRect exposed = exposedBase.intersected(QRectF(cell.rect))
										.toAlignedRect();
					painter->drawPixmap(
						exposed, cell.pixmap,
						exposed.translated(-cell.rect.topLeft()));
				}

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

					QRect exposed =
						exposedBase.intersected(canvasRect).toAlignedRect();
					int left = exposed.left();
					int right = exposed.right() + 1;
					int top = exposed.top();
					int bottom = exposed.bottom() + 1;
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
	}

	painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

const QBrush &SoftwareCanvasRenderer::getCheckerBrush()
{
	if(m_checkerBrush.style() == Qt::NoBrush) {
		QPixmap checkerTexture(64, 64);
		QPainter painter(&checkerTexture);
		painter.fillRect(0, 0, 32, 32, m_checkerColor1);
		painter.fillRect(32, 0, 32, 32, m_checkerColor2);
		painter.fillRect(0, 32, 32, 32, m_checkerColor2);
		painter.fillRect(32, 32, 32, 32, m_checkerColor1);
		m_checkerBrush.setTexture(checkerTexture);
	}
	return m_checkerBrush;
}

bool SoftwareCanvasRenderer::isCheckersVisible(CanvasControllerBase *controller)
{
	const canvas::CanvasModel *canvasModel = controller->canvasModel();
	return canvasModel && canvasModel->paintEngine()->checkersVisible();
}

}

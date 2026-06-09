// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/lassofill.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/net/client.h"
#include "libclient/net/message.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QDateTime>
#include <QPainter>

using std::placeholders::_1;
using utils::Cursors;

namespace tools {

LassoFillTool::LassoFillTool(ToolController &owner)
	: Tool(
		  owner, LASSOFILL, Cursors::lassoFill(),
		  Capability::AllowColorPick | Capability::AllowToolAdjust2)
	, m_strokeEngine(
		  [this](DP_BrushPoint bp, const drawdance::CanvasState &) {
			  addPoint(QPointF(bp.x, bp.y));
		  },
		  std::bind(&LassoFillTool::pollControl, this, _1))
	, m_blendMode(DP_BLEND_MODE_NORMAL)
{
	m_pollTimer.setSingleShot(false);
	m_pollTimer.setTimerType(Qt::PreciseTimer);
	m_pollTimer.setInterval(15);
	QObject::connect(&m_pollTimer, &QTimer::timeout, [this]() {
		poll();
	});
	QObject::connect(
		&m_owner, &ToolController::lassoFillUpdateRequested, &m_owner,
		std::bind(&LassoFillTool::updatePendingPreview, this),
		Qt::QueuedConnection);
}

void LassoFillTool::begin(const BeginParams &params)
{
	if(params.right) {
		cancelMultipart();
	} else {
		m_drawing = true;
		finishMultipart();
		m_clickDetector.begin(params.viewPos, params.deviceType);

		QColor color = m_owner.foregroundColor();
		emit m_owner.colorUsed(color);
		color.setAlphaF(m_opacity);

		QRect selBounds;
		QImage selImage;
		if(m_owner.isSelectionMaskingEnabled()) {
			canvas::SelectionModel *sel = m_owner.model()->selection();
			if(sel->isValid()) {
				selBounds = sel->bounds();
				selImage = sel->image();
			}
		}

		m_shape.begin(
			m_antiAlias, m_fan, m_owner.activeLayer(), m_blendMode, color,
			selBounds, selImage);
		m_lastTimeMsec = params.point.timeMsec();
		m_owner.setStrokeEngineParams(
			m_strokeEngine, getEffectiveStabilizerSampleCount(),
			getEffectiveSmoothing());
		m_strokeEngine.beginStroke(params.zoom);
		m_strokeEngine.strokeTo(params.point, drawdance::CanvasState::null());
	}
}

void LassoFillTool::motion(const MotionParams &params)
{
	if(m_drawing) {
		m_clickDetector.motion(params.viewPos);
		m_lastTimeMsec = params.point.timeMsec();
		m_strokeEngine.strokeTo(params.point, drawdance::CanvasState::null());
	}
}

void LassoFillTool::end(const EndParams &params)
{
	Q_UNUSED(params);
	if(m_drawing) {
		m_clickDetector.end();
		m_drawing = false;
		m_strokeEngine.endStroke(
			m_lastTimeMsec, drawdance::CanvasState::null());
		if(m_clickDetector.isClick() || !m_shape.get(nullptr, nullptr)) {
			cancelMultipart();
		} else {
			setCursor(Cursors::lassoFillCheck());
			setCapability(Capability::HandlesRightClick, true);
			emit m_owner.lassoFillStateChanged(true);
		}
	}
}

void LassoFillTool::finishMultipart()
{
	if(isMultipart()) {
		QPoint pos;
		const QImage *image;
		if(m_shape.get(&pos, &image)) {
			net::MessageList msgs;
			uint8_t contextId = localUserId();
			net::makePutImageMessagesCompat(
				msgs, contextId, m_shape.layerId(), m_shape.blendMode(),
				pos.x(), pos.y(), *image, isCompatibilityMode());
			if(!msgs.isEmpty()) {
				msgs.prepend(net::makeUndoPointMessage(contextId));
				m_owner.client()->sendCommands(msgs.size(), msgs.constData());
			}
		}
		m_shape.clear();
		m_previewUpdatePending = false;
		requestPendingPreviewUpdate();
		setCursor(Cursors::lassoFill());
		setCapability(Capability::HandlesRightClick, false);
		emit m_owner.lassoFillStateChanged(false);
	}
}

void LassoFillTool::cancelMultipart()
{
	if(isMultipart()) {
		m_shape.clear();
		m_previewUpdatePending = false;
		requestPendingPreviewUpdate();
		setCursor(Cursors::lassoFill());
		setCapability(Capability::HandlesRightClick, false);
		emit m_owner.lassoFillStateChanged(false);
	}
}

void LassoFillTool::undoMultipart()
{
	cancelMultipart();
}

bool LassoFillTool::isMultipart() const
{
	return m_shape.isPending();
}

void LassoFillTool::setSelectionMaskingEnabled(bool selectionMaskingEnabled)
{
	setCapability(Capability::IgnoresSelections, !selectionMaskingEnabled);
}

void LassoFillTool::setParams(
	float opacity, int stabilizationMode, int stabilizerSampleCount,
	int smoothing, int blendMode, bool antiAlias, bool fan)
{
	m_opacity = opacity;
	m_stabilizationMode = stabilizationMode;
	m_stabilizerSampleCount = stabilizerSampleCount;
	m_smoothing = smoothing;
	m_blendMode = blendMode;
	m_antiAlias = antiAlias;
	m_fan = fan;
}

void LassoFillTool::Shape::begin(
	bool antiAlias, bool fan, int layerId, int blendMode, const QColor &color,
	const QRect &selBounds, const QImage &selImage)
{
	clear();
	m_pending = true;
	m_antiAlias = antiAlias;
	m_fan = fan;
	m_layerId = layerId;
	m_blendMode = blendMode;
	m_color = color;
	m_selBounds = selBounds;
	m_selImage = selImage;
}

void LassoFillTool::Shape::clear()
{
	m_pending = false;
	m_imageValid = false;
	m_lastCount = 0;
	m_polygon.clear();
	m_polygonF.clear();
	m_lastBounds = QRect();
	m_selBounds = QRect();
	m_selImage = QImage();
	m_image = QImage();
	m_fanImage = QImage();
}

int LassoFillTool::Shape::pointCount() const
{
	return m_antiAlias ? m_polygonF.size() : m_polygon.size();
}

void LassoFillTool::Shape::addPoint(const QPointF &point)
{
	if(m_antiAlias) {
		if(m_polygonF.isEmpty() ||
		   !canvas::Point::intSame(point, m_polygonF.last())) {
			m_polygonF.append(point);
			m_imageValid = false;
		}
	} else {
		QPoint p = point.toPoint();
		if(m_polygon.isEmpty() || p != m_polygon.last()) {
			m_polygon.append(p);
			m_imageValid = false;
		}
	}
}

bool LassoFillTool::Shape::get(QPoint *outPos, const QImage **outImage)
{
	updateImage();
	if(m_imageValid) {
		if(outPos) {
			*outPos = m_pos;
		}
		if(outImage) {
			*outImage = &m_image;
		}
		return true;
	} else {
		return false;
	}
}

void LassoFillTool::Shape::updateImage()
{
	constexpr QImage::Format FORMAT = QImage::Format_ARGB32_Premultiplied;

	int count;
	if(m_pending && !m_imageValid && (count = pointCount()) > 2) {
		QRect bounds = m_antiAlias ? m_polygonF.boundingRect().toAlignedRect()
								   : m_polygon.boundingRect();

		bool haveSel = !m_selBounds.isEmpty();
		if(haveSel) {
			bounds &= m_selBounds;
		}

		if(!bounds.isEmpty()) {
			m_imageValid = true;
			m_pos = bounds.topLeft();

			QSize imageSize = bounds.size();
			if(m_fan) {
				// The fan fill draws a bunch of separate triangles, which means
				// we can't use regular anti-aliasing or else we get gaps in the
				// middle of the fill. We instead use 4x super-sampling, that
				// seems to give the same end result as anti-aliased painting.
				int fanScale;
				if(m_antiAlias) {
					fanScale = 4;
				} else {
					fanScale = 1;
				}

				// The fan fill just needs to add the new triangle, it never
				// removes anything from the fill, so we can re-use the image.
				QSize fanImageSize = imageSize * fanScale;
				if(m_fanImage.isNull() || m_lastBounds.isEmpty()) {
					m_fanImage = QImage(fanImageSize, FORMAT);
					m_fanImage.fill(0);

				} else if(m_lastBounds != bounds) {
					// QImage::copy doesn't accept negative indexes, so we have
					// to make a new image and paint the previous one to it.
					QImage newImage(fanImageSize, FORMAT);
					newImage.fill(0);
					{
						QPainter newImagePainter(&newImage);
						newImagePainter.setCompositionMode(
							QPainter::CompositionMode_Source);
						newImagePainter.drawImage(
							(m_lastBounds.topLeft() - m_pos) * fanScale,
							m_fanImage);
					}
					m_fanImage = std::move(newImage);

				} else {
					// Bounds are still the same, no need for a new image.
				}

				{
					QPainter fanPainter(&m_fanImage);
					fanPainter.setPen(Qt::NoPen);
					fanPainter.setBrush(m_color);
					fanPainter.setRenderHint(QPainter::Antialiasing, false);
					if(fanScale != 1) {
						fanPainter.scale(qreal(fanScale), qreal(fanScale));
					}
					fanPainter.translate(-m_pos);

					int firstIndex = qMax(1, m_lastCount - 2);
					if(m_antiAlias) {
						for(int i = firstIndex; i < count - 1; ++i) {
							QPolygonF triangle(
								{m_polygonF[0], m_polygonF[i],
								 m_polygonF[i + 1]});
							fanPainter.drawPolygon(triangle);
						}
						m_image = m_fanImage.scaled(
							imageSize, Qt::IgnoreAspectRatio,
							Qt::SmoothTransformation);
					} else {
						for(int i = firstIndex; i < count - 1; ++i) {
							QPolygon triangle(
								{m_polygon[0], m_polygon[i], m_polygon[i + 1]});
							fanPainter.drawPolygon(triangle);
						}
						m_image = m_fanImage;
					}
				}

				if(haveSel) {
					QPainter painter(&m_image);
					applySelection(painter);
				}

			} else {
				// The regular lasso fill needs to repaint everything.
				if(m_image.isNull() || m_image.size() != imageSize) {
					m_image = QImage(imageSize, FORMAT);
				}
				m_image.fill(0);

				QPainter painter(&m_image);
				painter.setPen(Qt::NoPen);
				painter.setBrush(m_color);
				painter.setRenderHint(QPainter::Antialiasing, m_antiAlias);
				painter.translate(-m_pos);
				if(m_antiAlias) {
					painter.drawPolygon(m_polygonF);
				} else {
					painter.drawPolygon(m_polygon);
				}

				if(haveSel) {
					applySelection(painter);
				}
			}
		}

		m_lastBounds = bounds;
		m_lastCount = count;
	}
}

void LassoFillTool::Shape::applySelection(QPainter &painter)
{
	painter.resetTransform();
	painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	painter.drawImage(m_selBounds.topLeft() - m_pos, m_selImage);
}

int LassoFillTool::getEffectiveStabilizerSampleCount() const
{
	return m_stabilizationMode == int(brushes::Stabilizer)
			   ? m_stabilizerSampleCount
			   : 0;
}

int LassoFillTool::getEffectiveSmoothing() const
{
	return m_stabilizationMode == int(brushes::Smoothing) ? m_smoothing : 0;
}

void LassoFillTool::addPoint(const QPointF &point)
{
	if(m_drawing) {
		m_shape.addPoint(point);
		requestPendingPreviewUpdate();
	}
}

void LassoFillTool::pollControl(bool enable)
{
	if(enable) {
		m_pollTimer.start();
	} else {
		m_pollTimer.stop();
	}
}

void LassoFillTool::poll()
{
	m_strokeEngine.poll(
		QDateTime::currentMSecsSinceEpoch(), drawdance::CanvasState::null());
}


void LassoFillTool::requestPendingPreviewUpdate()
{
	if(!m_previewUpdatePending) {
		m_previewUpdatePending = true;
		emit m_owner.lassoFillUpdateRequested();
	}
}

void LassoFillTool::updatePendingPreview()
{
	if(m_previewUpdatePending) {
		m_previewUpdatePending = false;
		canvas::CanvasModel *canvas = m_owner.model();
		if(canvas) {
			QPoint pos;
			const QImage *image;
			if(m_shape.get(&pos, &image)) {
				canvas->paintEngine()->previewFill(
					m_shape.layerId(), m_shape.blendMode(), 1.0f, pos.x(),
					pos.y(), *image);
			} else {
				canvas->paintEngine()->clearFillPreview();
			}
		}
	}
}

}

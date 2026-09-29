// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/image.h>
#include <dpengine/layer_props.h>
#include <dpengine/preview.h>
#include <dpmsg/blend_mode.h>
#include <dpmsg/ids.h>
}
#include "libclient/canvas/acl.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/drawdance/image.h"
#include "libclient/utils/asynctaskrunnable.h"
#include "libclient/utils/scopedoverridecursor.h"
#include <QPainter>
#include <QPointer>
#include <QThreadPool>

namespace canvas {

namespace {
class LiquifyPreviewTask : public AsyncTask {
public:
	explicit LiquifyPreviewTask(
		const drawdance::LiquifyState &liquifyState, int interpolation,
		const std::function<QImage(QPointF &)> &getMergedImage)
		: m_liquifyState(liquifyState)
		, m_getMergedImage(getMergedImage)
		, m_interpolation(interpolation)
	{
	}

	const QImage &dstImage() const { return m_dstImage; }
	QPoint dstOffset() const { return m_dstOffset; }

	AsyncTaskResult run() override
	{
		if(isCancelled()) {
			return AsyncTaskResult::cancelled();
		}

		QPointF srcOffset;
		QImage srcImage = m_getMergedImage(srcOffset);
		if(srcImage.isNull()) {
			return AsyncTaskResult::errorProceed(
				QStringLiteral("No source image"));
		}

		// TODO identity
		// TODO re-use transformer

		drawdance::LiquifyTransformer liquifyTransformer =
			drawdance::LiquifyTransformer::init(
				srcOffset.x(), srcOffset.y(), srcImage);
		liquifyTransformer.apply(m_liquifyState, m_interpolation);

		if(isCancelled()) {
			return AsyncTaskResult::cancelled();
		}

		m_dstImage = liquifyTransformer.targetImage(m_dstOffset);
		if(m_dstImage.isNull()) {
			return AsyncTaskResult::errorProceed(
				QStringLiteral("No destination image"));
		}

		return AsyncTaskResult::ok();
	}

private:
	drawdance::LiquifyState m_liquifyState;
	std::function<QImage(QPointF &)> m_getMergedImage;
	QImage m_dstImage;
	QPoint m_dstOffset;
	int m_interpolation;
};
}

TransformModel::TransformModel(CanvasModel *canvas)
	: QObject(canvas)
	, m_canvas(canvas)
	, m_liquifyInterpolation(DP_MSG_TRANSFORM_REGION_MODE_NEAREST)
	, m_blendMode(DP_BLEND_MODE_NORMAL)
{
	LayerListModel *layerlist = canvas->layerlist();
	connect(
		layerlist, &LayerListModel::modelReset, this,
		&TransformModel::updateLayerIds);
	connect(
		layerlist, &LayerListModel::layerCheckStateToggled, this,
		&TransformModel::updateLayerIds);
	connect(
		this, &TransformModel::liquifyPreviewRequested, this,
		&TransformModel::requestLiquifyPreviewUpdate, Qt::QueuedConnection);
}

bool TransformModel::isPreviewAccurate() const
{
	return m_previewAccurate && canPreviewAccurate();
}

bool TransformModel::canPreviewAccurate() const
{
	return !isLiquify() &&
		   (m_pasted || m_layerIds.size() <= DP_PREVIEW_TRANSFORM_COUNT);
}

const QImage &TransformModel::floatingImage()
{
	if(!m_pasted && m_floatingImage.isNull()) {
		m_floatingImageOffset = m_srcBounds.topLeft();
		switch(m_layerIds.size()) {
		case 0:
			break;
		case 1:
			m_floatingImage = getLayerImage(*m_layerIds.constBegin());
			break;
		default:
			m_floatingImage = getMergedImage();
			break;
		}
	}
	return m_floatingImage;
}

QImage TransformModel::layerImage(int layerId)
{
	if(m_layerIds.contains(layerId)) {
		QHash<int, QImage>::iterator it = m_layerImages.find(layerId);
		if(it == m_layerImages.end()) {
			QImage img = getLayerImage(layerId);
			m_layerImages.insert(layerId, img);
			return img;
		} else {
			return it.value();
		}
	} else {
		return QImage();
	}
}

void TransformModel::beginFromCanvas(
	const QRect &srcBounds, const QImage &mask, const QSet<int> &sourceLayerIds)
{
	clear();
	LayerListModel *layerlist = m_canvas->layerlist();
	layerlist->initCheckedLayers(sourceLayerIds);
	m_active = true;
	m_justApplied = false;
	m_srcBounds = srcBounds;
	m_dstQuad = TransformQuad(srcBounds);
	m_dstQuadValid = isQuadValid(m_dstQuad);
	m_mask = isMaskRelevant(mask) ? mask : QImage();
	setLayers(layerlist->checkedLayers());
}

void TransformModel::beginFloating(const QRect &srcBounds, const QImage &image)
{
	clear();
	m_active = true;
	m_pasted = true;
	m_justApplied = false;
	m_srcBounds = srcBounds;
	m_dstQuad = TransformQuad(srcBounds);
	m_dstQuadValid = isQuadValid(m_dstQuad);
	m_floatingImage = image;
	m_floatingImageOffset = srcBounds.topLeft();
	m_canvas->layerlist()->clearCheckedLayers();
	emit transformChanged();
}

void TransformModel::beginLiquifyFromCanvas(
	const QRect &srcBounds, const QImage &mask, const QSet<int> &sourceLayerIds,
	int liquifyInterpolation)
{
	LayerListModel *layerlist = m_canvas->layerlist();
	layerlist->initCheckedLayers(sourceLayerIds);
	m_active = true;
	m_justApplied = false;
	m_srcBounds = srcBounds;
	m_dstQuad = TransformQuad(srcBounds);
	m_dstQuadValid = isQuadValid(m_dstQuad);
	m_mask = isMaskRelevant(mask) ? mask : QImage();
	m_liquify = drawdance::Liquify::init(srcBounds, m_mask);
	m_liquifyInterpolation = liquifyInterpolation;
	setLayers(layerlist->checkedLayers());
}

void TransformModel::setDeselectOnApply(bool deselectOnApply)
{
	if(m_active) {
		m_deselectOnApply = deselectOnApply;
	}
}

void TransformModel::setDstQuad(const TransformQuad &dstQuad)
{
	if(m_active && !isLiquify()) {
		TransformQuad result = dstQuad.round();
		if(result != m_dstQuad && !result.boundingRect().isEmpty()) {
			m_dstQuad = result;
			m_dstQuadValid = isQuadValid(result);
			m_justApplied = false;
			emit transformChanged();
		}
	} else {
		qWarning("TransformModel::setDstQuad: transform not active");
	}
}

void TransformModel::setPreviewAccurate(bool previewAccurate)
{
	if(previewAccurate != m_previewAccurate) {
		m_previewAccurate = previewAccurate;
		if(isTransformActive()) {
			emit transformChanged();
		}
	}
}

void TransformModel::setLiquifyInterpolation(int liquifyInterpolation)
{
	if(liquifyInterpolation != m_liquifyInterpolation) {
		m_liquifyInterpolation = liquifyInterpolation;
		if(isLiquifyActive()) {
			requestLiquifyPreviewUpdate();
		}
	}
}

void TransformModel::setBlendMode(int blendMode)
{
	if(blendMode != m_blendMode) {
		m_blendMode = blendMode;
		emit transformChanged();
	}
}

void TransformModel::setOpacity(qreal opacity)
{
	if(opacity != m_opacity) {
		m_opacity = opacity;
		emit transformChanged();
	}
}

void TransformModel::liquify(
	const std::function<void(drawdance::Liquify &)> &fn)
{
	if(isLiquifyActive()) {
		fn(m_liquify);
		requestLiquifyPreviewUpdate();
	} else {
		qWarning("TransformModel::liquify: liquify not active");
	}
}

void TransformModel::applyOffset(int x, int y)
{
	if(m_active) {
		m_srcBounds.translate(x, y);
		m_dstQuad.applyOffset(x, y);
		emit transformChanged();
	}
}

QVector<net::Message> TransformModel::applyActiveTransform(
	uint8_t contextId, int layerId, int interpolation, bool stamp,
	bool *outMovedSelection)
{
	if(m_active && m_dstQuadValid) {
		utils::ScopedOverrideCursor waitCursor;
		if(isLiquify()) {
			return applyLiquify(contextId, layerId, interpolation);
		} else if(m_pasted) {
			return applyFloating(
				contextId, layerId, interpolation, stamp, outMovedSelection);
		} else {
			if(stamp) {
				if(isStampable()) {
					m_stamped = true;
				} else {
					qWarning("Attempt to stamp unstampable transform");
					return {};
				}
			}
			return applyFromCanvas(
				contextId, layerId, interpolation, outMovedSelection);
		}
	} else {
		return {};
	}
}

bool TransformModel::isAllowedToApplyActiveTransform() const
{
	if(m_active && m_dstQuadValid) {
		const AclState *aclState = m_canvas->aclState();
		if(m_pasted) {
			return aclState->canUseFeature(DP_FEATURE_PUT_IMAGE);
		} else if(aclState->canUseFeature(DP_FEATURE_REGION_MOVE)) {
			bool needsPutImage =
				isDstQuadBoundingRectAreaSizeOutOfBounds() ||
				(m_canvas->isCompatibilityMode() &&
				 (m_blendMode != int(DP_BLEND_MODE_NORMAL) || m_opacity < 1.0));
			return !needsPutImage ||
				   aclState->canUseFeature(DP_FEATURE_PUT_IMAGE);
		} else {
			return false;
		}
	} else {
		return true;
	}
}

void TransformModel::endActiveTransform(bool applied)
{
	if(m_active) {
		if(!m_pasted) {
			emit transformCutCleared();
		}
		clear();
		m_justApplied = applied;
		m_canvas->layerlist()->clearCheckedLayers();
		emit transformChanged();
	}
}

int TransformModel::getEffectiveInterpolation(int interpolation) const
{
	if(interpolation != DP_MSG_TRANSFORM_REGION_MODE_NEAREST &&
	   m_dstQuadValid) {
		QTransform t;
		if(QTransform::quadToQuad(srcPolygon(), m_dstQuad.polygon(), t)) {
			t.setMatrix(
				t.m11(), t.m12(), t.m13(), t.m21(), t.m22(), t.m23(), 0.0, 0.0,
				t.m33());
			if(isRightAngleRotationOrReflection(t)) {
				return DP_MSG_TRANSFORM_REGION_MODE_NEAREST;
			}
		}
	}
	return interpolation;
}

int TransformModel::getSingleLayerMoveId(int layerId) const
{
	if(m_layerIds.size() == 1) {
		QModelIndex idx = m_canvas->layerlist()->layerIndex(layerId);
		if(idx.isValid() && !idx.data(LayerListModel::IsGroupRole).toBool() &&
		   !idx.data(LayerListModel::IsCensoredInTreeRole).toBool() &&
		   !idx.data(LayerListModel::IsLockedRole).toBool()) {
			return *m_layerIds.constBegin();
		}
	}
	return 0;
}

QVector<net::Message> TransformModel::applyFromCanvas(
	uint8_t contextId, int layerId, int interpolation, bool *outMovedSelection)
{
	Q_ASSERT(m_active);
	Q_ASSERT(!m_pasted || m_stamped);
	bool identity = TransformQuad(m_srcBounds) == m_dstQuad;
	bool adjustsImage =
		m_blendMode != int(DP_BLEND_MODE_NORMAL) || m_opacity < 1.0;
	int singleLayerSourceId = getSingleLayerMoveId(layerId);
	if(!identity || adjustsImage ||
	   (singleLayerSourceId > 0 && singleLayerSourceId != layerId) ||
	   m_deselectOnApply) {
		int srcX = m_srcBounds.x();
		int srcY = m_srcBounds.y();
		int srcW = m_srcBounds.width();
		int srcH = m_srcBounds.height();
		int dstTopLeftX = qRound(m_dstQuad.topLeft().x());
		int dstTopLeftY = qRound(m_dstQuad.topLeft().y());
		bool moveContents = !m_pasted;
		bool alterSelection = !moveContents || !m_stamped;
		bool moveSelection = alterSelection && !m_deselectOnApply;
		bool needsMask = moveContents && !m_mask.isNull();
		bool sizeOutOfBounds = isDstQuadBoundingRectAreaSizeOutOfBounds();
		bool compatibilityMode = m_canvas->isCompatibilityMode();
		QVector<net::Message> msgs;
		msgs.reserve(1 + (moveContents ? 1 : 0) + (alterSelection ? 1 : 0));

		if(alterSelection && !moveSelection) {
			msgs.append(
				net::makeSelectionClearMessage(
					contextId, canvas::CanvasModel::MAIN_SELECTION_ID));
		}

		if(moveIsOnlyTranslated()) {
			if(moveSelection && !identity) {
				int selectionId = DP_selection_id_make(
					contextId, canvas::CanvasModel::MAIN_SELECTION_ID);
				msgs.append(
					net::makeMoveRectMessageCompat(
						contextId, selectionId, selectionId, srcX, srcY,
						dstTopLeftX, dstTopLeftY, srcW, srcH,
						uint8_t(DP_BLEND_MODE_NORMAL), 255, QImage(),
						compatibilityMode));
			}
			if(moveContents) {
				if(singleLayerSourceId > 0) {
					if(hasContentInSelection(singleLayerSourceId)) {
						applyMoveRect(
							msgs, contextId, layerId, singleLayerSourceId, srcX,
							srcY, dstTopLeftX, dstTopLeftY, srcW, srcH,
							needsMask ? m_mask : QImage(),
							compatibilityMode && adjustsImage);
					}
				} else {
					for(int layerIdToMove : m_layerIds) {
						if(hasContentInSelection(layerIdToMove)) {
							applyMoveRect(
								msgs, contextId, layerIdToMove, layerIdToMove,
								srcX, srcY, dstTopLeftX, dstTopLeftY, srcW,
								srcH, needsMask ? m_mask : QImage(),
								compatibilityMode && adjustsImage);
						}
					}
				}
			}
		} else {
			int dstTopRightX = qRound(m_dstQuad.topRight().x());
			int dstTopRightY = qRound(m_dstQuad.topRight().y());
			int dstBottomRightX = qRound(m_dstQuad.bottomRight().x());
			int dstBottomRightY = qRound(m_dstQuad.bottomRight().y());
			int dstBottomLeftX = qRound(m_dstQuad.bottomLeft().x());
			int dstBottomLeftY = qRound(m_dstQuad.bottomLeft().y());
			int effectiveInterpolation =
				getEffectiveInterpolation(interpolation);
			bool needsCutAndPaste =
				sizeOutOfBounds ||
				effectiveInterpolation == DP_MSG_TRANSFORM_REGION_MODE_BINARY;
			if(moveSelection && !identity) {
				applyTransformRegionSelection(
					msgs, contextId, srcX, srcY, srcW, srcH, dstTopLeftX,
					dstTopLeftY, dstTopRightX, dstTopRightY, dstBottomRightX,
					dstBottomRightY, dstBottomLeftX, dstBottomLeftY,
					effectiveInterpolation, needsCutAndPaste);
			}
			if(moveContents) {
				if(singleLayerSourceId > 0) {
					if(hasContentInSelection(singleLayerSourceId)) {
						applyTransformRegion(
							msgs, contextId, layerId, singleLayerSourceId, srcX,
							srcY, srcW, srcH, dstTopLeftX, dstTopLeftY,
							dstTopRightX, dstTopRightY, dstBottomRightX,
							dstBottomRightY, dstBottomLeftX, dstBottomLeftY,
							effectiveInterpolation,
							needsMask ? m_mask : QImage(),
							needsCutAndPaste ||
								(compatibilityMode && adjustsImage));
					}
				} else {
					for(int layerIdToMove : m_layerIds) {
						if(hasContentInSelection(layerIdToMove)) {
							applyTransformRegion(
								msgs, contextId, layerIdToMove, layerIdToMove,
								srcX, srcY, srcW, srcH, dstTopLeftX,
								dstTopLeftY, dstTopRightX, dstTopRightY,
								dstBottomRightX, dstBottomRightY,
								dstBottomLeftX, dstBottomLeftY,
								effectiveInterpolation,
								needsMask ? m_mask : QImage(),
								needsCutAndPaste ||
									(compatibilityMode && adjustsImage));
						}
					}
				}
			}
		}
		if(!msgs.isEmpty() && !containsNullMessages(msgs)) {
			msgs.prepend(net::makeUndoPointMessage(contextId));
			if(m_stamped && !m_pasted) {
				m_pasted = true;
				m_mask = QImage();
				QHash<int, QImage>::const_iterator imageIt =
					m_layerImages.constBegin();
				if(imageIt == m_layerImages.constEnd()) {
					QSet<int>::const_iterator idIt = m_layerIds.constBegin();
					if(idIt == m_layerIds.constEnd()) {
						m_floatingImage = QImage();
					} else {
						m_floatingImage = getLayerImage(*idIt);
					}
				} else {
					m_floatingImage = *imageIt;
				}
				m_floatingImageOffset = m_srcBounds.topLeft();
				m_layerImages.clear();
				m_layerIds.clear();
				m_canvas->layerlist()->clearCheckedLayers();
				emit transformCutCleared();
				emit transformChanged();
			}
			if(outMovedSelection) {
				*outMovedSelection = moveSelection;
			}
			return msgs;
		}
	}
	return {};
}

void TransformModel::applyMoveRect(
	QVector<net::Message> &msgs, unsigned int contextId, int layerId,
	int sourceId, int srcX, int srcY, int dstTopLeftX, int dstTopLeftY,
	int srcW, int srcH, const QImage &mask, bool needsCutAndPaste) const
{
	if(!needsCutAndPaste) {
		net::Message msg = net::makeMoveRectMessageCompat(
			contextId, layerId, sourceId, srcX, srcY, dstTopLeftX, dstTopLeftY,
			srcW, srcH, uint8_t(getEffectiveBlendModeForLayer(layerId)),
			getUint8Opacity(), mask, m_canvas->isCompatibilityMode());
		// The message may be null if it ends up too large. Fall back to cutting
		// and pasting in that case, which can be done in smaller chunks.
		if(msg.isNull()) {
			needsCutAndPaste = true;
		} else {
			msgs.append(msg);
		}
	}

	if(needsCutAndPaste) {
		QImage img = getLayerImageWithMask(sourceId, mask);
		if(img.isNull()) {
			qWarning("applyMoveRect: image from layer %d is null", layerId);
		} else {
			applyCut(msgs, contextId, sourceId, srcX, srcY, srcW, srcH, mask);
			applyOpacityToImage(img);
			net::makePutImageMessagesCompat(
				msgs, contextId, layerId,
				uint8_t(getEffectiveBlendModeForLayer(layerId)), dstTopLeftX,
				dstTopLeftY, img, m_canvas->isCompatibilityMode());
		}
	}
}

void TransformModel::applyTransformRegion(
	QVector<net::Message> &msgs, unsigned int contextId, int layerId,
	int sourceId, int srcX, int srcY, int srcW, int srcH, int dstTopLeftX,
	int dstTopLeftY, int dstTopRightX, int dstTopRightY, int dstBottomRightX,
	int dstBottomRightY, int dstBottomLeftX, int dstBottomLeftY,
	int interpolation, const QImage &mask, bool needsCutAndPaste) const
{
	if(!needsCutAndPaste) {
		net::Message msg = net::makeTransformRegionMessageCompat(
			contextId, layerId, sourceId, srcX, srcY, srcW, srcH, dstTopLeftX,
			dstTopLeftY, dstTopRightX, dstTopRightY, dstBottomRightX,
			dstBottomRightY, dstBottomLeftX, dstBottomLeftY,
			getEffectiveInterpolation(interpolation),
			uint8_t(getEffectiveBlendModeForLayer(layerId)), getUint8Opacity(),
			mask, m_canvas->isCompatibilityMode());
		// The message may be null if it ends up too large. Fall back to cutting
		// and pasting in that case, which can be done in smaller chunks.
		if(msg.isNull()) {
			needsCutAndPaste = true;
		} else {
			msgs.append(msg);
		}
	}

	if(needsCutAndPaste) {
		QPoint offset;
		QImage img = drawdance::transformImage(
			getLayerImageWithMask(sourceId, mask),
			m_dstQuad.polygon().toPolygon(),
			getEffectiveInterpolation(interpolation), false, &offset);
		if(img.isNull()) {
			qWarning(
				"TransformModel::applyTransformRegion: could not transform "
				"image: %s",
				DP_error());
		} else {
			applyCut(msgs, contextId, sourceId, srcX, srcY, srcW, srcH, mask);
			applyOpacityToImage(img);
			net::makePutImageMessagesCompat(
				msgs, contextId, layerId,
				uint8_t(getEffectiveBlendModeForLayer(layerId)), offset.x(),
				offset.y(), img, m_canvas->isCompatibilityMode());
		}
	}
}

void TransformModel::applyTransformRegionSelection(
	QVector<net::Message> &msgs, unsigned int contextId, int srcX, int srcY,
	int srcW, int srcH, int dstTopLeftX, int dstTopLeftY, int dstTopRightX,
	int dstTopRightY, int dstBottomRightX, int dstBottomRightY,
	int dstBottomLeftX, int dstBottomLeftY, int interpolation,
	bool sizeOutOfBounds) const
{
	if(sizeOutOfBounds) {
		canvas::SelectionModel *selection = m_canvas->selection();
		if(selection->isValid()) {
			QPoint offset;
			QImage img = drawdance::transformImage(
				selection->image(), m_dstQuad.polygon().toPolygon(),
				getEffectiveInterpolation(interpolation), false, &offset);
			if(img.isNull()) {
				qWarning(
					"TransformModel::applyTransformRegionSelection: could not "
					"transform image: %s",
					DP_error());
			} else {
				net::makeSelectionPutMessages(
					msgs, contextId, CanvasModel::MAIN_SELECTION_ID,
					DP_MSG_SELECTION_PUT_OP_REPLACE, offset.x(), offset.y(),
					img.width(), img.height(),
					isImageOpaque(img) ? QImage() : img);
			}
		} else {
			qWarning(
				"TransformModel::applyTransformRegionSelection: no valid "
				"selection");
		}
	} else {
		int selectionId = DP_selection_id_make(
			contextId, canvas::CanvasModel::MAIN_SELECTION_ID);
		msgs.append(
			net::makeTransformRegionMessageCompat(
				contextId, selectionId, selectionId, srcX, srcY, srcW, srcH,
				dstTopLeftX, dstTopLeftY, dstTopRightX, dstTopRightY,
				dstBottomRightX, dstBottomRightY, dstBottomLeftX,
				dstBottomLeftY, getEffectiveInterpolation(interpolation),
				uint8_t(DP_BLEND_MODE_NORMAL), 255, QImage(),
				m_canvas->isCompatibilityMode()));
	}
}

bool TransformModel::isDstQuadBoundingRectAreaSizeOutOfBounds() const
{
	QRect rect = m_dstQuad.boundingRect().toAlignedRect();
	// Some extra padding to make sure we're not hitting some edge case.
	long long width = rect.width() + 2LL;
	long long height = rect.height() + 2LL;
	return width * height > DP_IMAGE_TRANSFORM_MAX_AREA;
}

void TransformModel::applyCut(
	QVector<net::Message> &msgs, unsigned int contextId, int sourceId, int srcX,
	int srcY, int srcW, int srcH, const QImage &mask) const
{
	if(mask.isNull()) {
		msgs.append(
			net::makeFillRectMessage(
				contextId, sourceId, DP_BLEND_MODE_ERASE, srcX, srcY, srcW,
				srcH, Qt::black));
	} else {
		net::makePutImageMessagesCompat(
			msgs, contextId, sourceId, DP_BLEND_MODE_ERASE, srcX, srcY, mask,
			m_canvas->isCompatibilityMode());
	}
}

QVector<net::Message> TransformModel::applyFloating(
	uint8_t contextId, int layerId, int interpolation, bool stamp,
	bool *outMovedSelection)
{
	Q_ASSERT(m_active);
	Q_ASSERT(m_pasted);
	QPoint offset;
	QImage transformedImage = drawdance::transformImage(
		m_floatingImage, m_dstQuad.polygon().toPolygon(),
		getEffectiveInterpolation(interpolation), false, &offset);
	if(transformedImage.isNull()) {
		qWarning(
			"TransformModel::applyFloating: could not transform image: %s",
			DP_error());
		return {};
	}

	applyOpacityToImage(transformedImage);

	QVector<net::Message> msgs;
	if(m_stamped && !stamp) {
		msgs = applyFromCanvas(
			contextId, layerId, interpolation, outMovedSelection);
	}

	// May be empty either if the transform wasn't stamped or if there was
	// nothing to modify about the selection. In both cases we still paste.
	if(msgs.isEmpty()) {
		msgs.reserve(2);
		msgs.append(net::makeUndoPointMessage(contextId));
	}

	net::makePutImageMessagesCompat(
		msgs, contextId, layerId, getEffectiveBlendModeForLayer(layerId),
		offset.x(), offset.y(), transformedImage,
		m_canvas->isCompatibilityMode());

	return msgs;
}

QVector<net::Message>
TransformModel::applyLiquify(uint8_t contextId, int layerId, int interpolation)
{
	if(m_liquify.isNull()) {
		return {};
	}

	drawdance::LiquifyState liquifyState = m_liquify.currentState();
	if(liquifyState.isNull()) {
		return {};
	}

	int srcX = m_srcBounds.x();
	int srcY = m_srcBounds.y();
	int srcW = m_srcBounds.width();
	int srcH = m_srcBounds.height();

	QVector<net::Message> msgs;
	msgs.reserve(m_layerIds.size() * 2 + 4);
	msgs.append(net::makeUndoPointMessage(contextId));

	int singleLayerSourceId = getSingleLayerMoveId(layerId);
	if(singleLayerSourceId > 0) {
		if(hasContentInSelection(singleLayerSourceId)) {
			applyLiquifyCutAndPaste(
				msgs, liquifyState, contextId, layerId, singleLayerSourceId,
				srcX, srcY, srcW, srcH, m_mask, interpolation);
		}
	} else {
		for(int layerIdToLiquify : m_layerIds) {
			if(hasContentInSelection(layerIdToLiquify)) {
				applyLiquifyCutAndPaste(
					msgs, liquifyState, contextId, layerIdToLiquify,
					layerIdToLiquify, srcX, srcY, srcW, srcH, m_mask,
					interpolation);
			}
		}
	}

	canvas::SelectionModel *selection = m_canvas->selection();
	if(selection->isValid()) {
		drawdance::LiquifyTransformer liquifyTransformer =
			drawdance::LiquifyTransformer::init(srcX, srcY, selection->image());
		liquifyTransformer.apply(liquifyState, interpolation);

		QPoint dstPos;
		QImage dstImage = liquifyTransformer.targetImage(dstPos);
		if(dstImage.isNull()) {
			qWarning("TransformModel::applyLiquify: null selection image");
		} else {
			net::makeSelectionPutMessages(
				msgs, contextId, CanvasModel::MAIN_SELECTION_ID,
				DP_MSG_SELECTION_PUT_OP_REPLACE, dstPos.x(), dstPos.y(),
				dstImage.width(), dstImage.height(),
				isImageOpaque(dstImage) ? QImage() : dstImage);
		}
	}

	return msgs;
}

void TransformModel::applyLiquifyCutAndPaste(
	QVector<net::Message> &msgs, const drawdance::LiquifyState &liquifyState,
	unsigned int contextId, int layerId, int sourceId, int srcX, int srcY,
	int srcW, int srcH, const QImage &mask, int interpolation)
{
	QImage srcImage = getLayerImageWithMask(sourceId, mask);
	if(srcImage.isNull()) {
		qWarning("Source image is null");
		return;
	}

	drawdance::LiquifyTransformer liquifyTransformer =
		drawdance::LiquifyTransformer::init(srcX, srcY, srcImage);
	liquifyTransformer.apply(liquifyState, interpolation);

	QPoint dstPos;
	QImage dstImage = liquifyTransformer.targetImage(dstPos);
	if(dstImage.isNull()) {
		qWarning("TransformModel::applyLiquifyCutAndPaste: null image");
	} else {
		applyCut(msgs, contextId, sourceId, srcX, srcY, srcW, srcH, mask);
		net::makePutImageMessagesCompat(
			msgs, contextId, layerId, DP_BLEND_MODE_NORMAL, dstPos.x(),
			dstPos.y(), dstImage, m_canvas->isCompatibilityMode());
	}
}

void TransformModel::clear()
{
	m_active = false;
	m_pasted = false;
	m_deselectOnApply = false;
	m_stamped = false;
	m_dstQuadValid = false;
	m_layerIds.clear();
	m_srcBounds = QRect();
	m_dstQuad.clear();
	m_mask = QImage();
	m_floatingImage = QImage();
	m_floatingImageOffset = QPoint();
	m_layerImages.clear();
	m_blendMode = DP_BLEND_MODE_NORMAL;
	m_opacity = 1.0;
	m_liquify = drawdance::Liquify::null();
}

void TransformModel::updateLayerIds()
{
	if(isMovedFromCanvas()) {
		QSet<int> layerIds = m_canvas->layerlist()->checkedLayers();
		if(layerIds != m_layerIds) {
			setLayers(layerIds);
		}
	}
}

void TransformModel::setLayers(const QSet<int> &layerIds)
{
	m_layerIds = layerIds;
	m_floatingImage = QImage();
	m_floatingImageOffset = m_srcBounds.topLeft();

	// Don't try replacing this with `erase`, that's bugged and causes botched
	// assertions in Qt. Also avoid similar functions like `erase_if`.
	QVector<int> layerIdsToRemove;
	for(QHash<int, QImage>::key_iterator it = m_layerImages.keyBegin(),
										 end = m_layerImages.keyEnd();
		it != end; ++it) {
		int layerId = *it;
		if(!layerIds.contains(layerId)) {
			layerIdsToRemove.append(layerId);
		}
	}
	for(int layerId : layerIdsToRemove) {
		m_layerImages.remove(layerId);
	}

	emit transformCut(m_layerIds, m_srcBounds, m_mask);
	emit transformChanged();
}

QImage TransformModel::getLayerImage(int layerId) const
{
	return getLayerImageWithMask(layerId, m_mask);
}

QImage
TransformModel::getLayerImageWithMask(int layerId, const QImage &mask) const
{
	drawdance::CanvasState canvasState =
		m_canvas->paintEngine()->viewCanvasState();
	QRect rect = QRect(QPoint(), canvasState.size()).intersected(m_srcBounds);
	if(!rect.isEmpty()) {
		drawdance::LayerSearchResult lsr =
			canvasState.searchLayer(layerId, false);
		if(drawdance::LayerContent *layerContent =
			   std::get_if<drawdance::LayerContent>(&lsr.data)) {
			QImage img = layerContent->toImage(rect);
			applyMaskToImage(img, mask);
			if(!isImageBlank(img)) {
				return img;
			}
		}
	}
	return QImage();
}

QImage TransformModel::getMergedImage() const
{
	if(m_layerIds.isEmpty()) {
		return QImage();
	} else {
		return getMergedImageFrom(
			m_canvas->paintEngine()->viewCanvasState(), m_mask, m_layerIds,
			m_srcBounds);
	}
	return QImage();
}

QImage TransformModel::getMergedImageFrom(
	const drawdance::CanvasState &canvasState, const QImage &mask,
	const QSet<int> &layerIds, QRect srcBounds)
{
	QRect rect = QRect(QPoint(), canvasState.size()).intersected(srcBounds);
	if(rect.isEmpty()) {
		return QImage();
	} else {
		DP_ViewModeCallback callback = {
			&TransformModel::isVisibleInViewModeCallback,
			const_cast<QSet<int> *>(&layerIds),
		};
		DP_ViewModeFilter vmf = DP_view_mode_filter_make_callback(&callback);
		QImage img = canvasState.toFlatImage(false, false, &rect, &vmf);
		applyMaskToImage(img, mask);
		return img;
	}
}

void TransformModel::applyMaskToImage(QImage &img, const QImage &mask)
{
	if(!mask.isNull()) {
		QPainter mp(&img);
		mp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		mp.drawImage(0, 0, mask);
	}
}

void TransformModel::applyOpacityToImage(QImage &img) const
{
	if(m_opacity < 1.0) {
		QPainter p(&img);
		p.setOpacity(m_opacity);
		p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		p.fillRect(img.rect(), Qt::black);
	}
}

bool TransformModel::isImageBlank(const QImage &img)
{
	const uint32_t *pixels =
		reinterpret_cast<const uint32_t *>(img.constBits());
	size_t size = size_t(img.width()) * size_t(img.height());
	for(size_t i = 0; i < size; ++i) {
		if(pixels[i] != 0) {
			return false;
		}
	}
	return true;
}

bool TransformModel::isImageOpaque(const QImage &img)
{
	const uint32_t *pixels =
		reinterpret_cast<const uint32_t *>(img.constBits());
	size_t size = size_t(img.width()) * size_t(img.height());
	for(size_t i = 0; i < size; ++i) {
		if(qAlpha(pixels[i]) < 255) {
			return false;
		}
	}
	return true;
}

bool TransformModel::isMaskRelevant(const QImage &mask)
{
	Q_ASSERT(mask.format() == QImage::Format_ARGB32_Premultiplied);
	size_t count = size_t(mask.width()) * size_t(mask.height());
	const uint32_t *pixels = reinterpret_cast<const uint32_t *>(mask.bits());
	for(size_t i = 0; i < count; ++i) {
		if(qAlpha(pixels[i]) != 255) {
			return true;
		}
	}
	return false;
}

bool TransformModel::moveIsOnlyTranslated() const
{
	TransformQuad src(m_srcBounds);
	QPointF topLeftDelta = m_dstQuad.topLeft() - src.topLeft();
	for(int i = 1; i < 4; ++i) {
		QPointF delta = m_dstQuad.at(i) - src.at(i);
		if(!qFuzzyCompare(delta.x(), topLeftDelta.x()) ||
		   !qFuzzyCompare(delta.y(), topLeftDelta.y())) {
			return false;
		}
	}
	return true;
}

bool TransformModel::hasContentInSelection(int layerId) const
{
	return !m_canvas->paintEngine()->historyCanvasState().isBlankIn(
		layerId, m_srcBounds, m_mask);
}

QImage TransformModel::convertMaskToMono() const
{
	Q_ASSERT(!m_mask.isNull());
	int width = m_mask.width();
	int height = m_mask.height();
	QImage mono(width, height, QImage::Format_Mono);
	mono.fill(0);
	const uint32_t *src =
		reinterpret_cast<const uint32_t *>(m_mask.constBits());
	for(int y = 0; y < height; ++y) {
		for(int x = 0; x < width; ++x) {
			if(qAlpha(src[y * width + x]) != 0) {
				mono.setPixel(x, y, 1);
			}
		}
	}
	return mono;
}

bool TransformModel::containsNullMessages(const QVector<net::Message> &msgs)
{
	for(const net::Message &msg : msgs) {
		if(msg.isNull()) {
			return true;
		}
	}
	return false;
}

QPolygonF TransformModel::srcPolygon() const
{
	// QRectF::toPolygon returns an excess point, so we can't use that here.
	// Also, using width and height is correct! Don't use bottom or right.
	qreal x = m_srcBounds.x();
	qreal y = m_srcBounds.y();
	qreal w = m_srcBounds.width();
	qreal h = m_srcBounds.height();
	return QPolygonF({
		QPointF(x, y),
		QPointF(x + w, y),
		QPointF(x + w, y + h),
		QPointF(x, y + h),
	});
}

bool TransformModel::isRightAngleRotationOrReflection(const QTransform &t) const
{
	QPoint scales[] = {
		QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1)};
	for(int angle = 0; angle <= 270; angle += 90) {
		for(const QPointF scale : scales) {
			QTransform u;
			if(angle != 0) {
				u.rotate(angle);
			}
			if(scale.x() != 1 || scale.y() != 1) {
				u.scale(scale.x(), scale.y());
			}
			if(qFuzzyCompare(t, u)) {
				return true;
			}
		}
	}
	return false;
}

int TransformModel::getEffectiveBlendModeForLayer(int layerId) const
{
	int blendMode;
	if(isAffectedByLayerAlphaLock() &&
	   m_canvas->paintEngine()->isLayerAlphaLocked(layerId)) {
		blendMode = blendmode::toAlphaPreserving(m_blendMode);
	} else {
		blendMode = m_blendMode;
	}

	if(m_canvas->isCompatibilityMode()) {
		return blendmode::toCompatible(blendMode);
	} else {
		return blendMode;
	}
}

void TransformModel::emitLiquifyPreviewRequest()
{
	if(!m_liquifyPreviewRequested) {
		m_liquifyPreviewRequested = true;
		Q_EMIT liquifyPreviewRequested();
	}
}

void TransformModel::requestLiquifyPreviewUpdate()
{
	m_liquifyPreviewRequested = false;
	if(isLiquify() && !m_layerIds.isEmpty()) {
		if(m_liquifyPreviewInProgress) {
			m_liquifyPreviewPending = true;
		} else {
			runLiquifyPreviewUpdate();
		}
	}
}

void TransformModel::runLiquifyPreviewUpdate()
{
	m_liquifyPreviewInProgress = true;
	AsyncTaskRunnable *runnable = new AsyncTaskRunnable({new LiquifyPreviewTask(
		m_liquify.currentState(), m_liquifyInterpolation,
		[canvasState = m_canvas->paintEngine()->viewCanvasState(),
		 mask = m_mask, layerIds = m_layerIds,
		 srcBounds = m_srcBounds](QPointF &outOffset) {
			outOffset = QPointF(srcBounds.topLeft());
			return getMergedImageFrom(canvasState, mask, layerIds, srcBounds);
		})});

	connect(
		runnable, &AsyncTaskRunnable::runFinished, runnable,
		[id = ++m_liquifyPreviewId, runnable,
		 model = QPointer<TransformModel>(this)] {
			if(model) {
				LiquifyPreviewTask *task =
					static_cast<LiquifyPreviewTask *>(runnable->taskAt(0));
				model->handleLiquifyPreviewUpdate(
					id, task->dstImage(), task->dstOffset());
			}
			runnable->deleteLater();
		},
		Qt::QueuedConnection);

	runnable->setAutoDelete(false);
	QThreadPool::globalInstance()->start(runnable);
}

void TransformModel::handleLiquifyPreviewUpdate(
	unsigned int id, const QImage &img, QPoint offset)
{
	if(id == m_liquifyPreviewId) {
		m_liquifyPreviewInProgress = false;
		if(m_liquifyPreviewPending) {
			m_liquifyPreviewPending = false;
			requestLiquifyPreviewUpdate();
		}
		m_floatingImage = img;
		m_floatingImageOffset = offset;
		Q_EMIT transformChanged();
	}
}

bool TransformModel::isQuadValid(const TransformQuad &quad)
{
	const QPolygonF &polygon = quad.polygon();
	if(polygon.size() == 4 && !polygon.boundingRect().isEmpty()) {
		int sign = crossProductSign(polygon[0], polygon[1], polygon[2]);
		return crossProductSign(polygon[1], polygon[2], polygon[3]) == sign &&
			   crossProductSign(polygon[2], polygon[3], polygon[0]) == sign &&
			   crossProductSign(polygon[3], polygon[0], polygon[1]) == sign;
	} else {
		return false;
	}
}

int TransformModel::crossProductSign(
	const QPointF &a, const QPointF &b, const QPointF &c)
{
	qreal crossProduct =
		(b.x() - a.x()) * (c.y() - b.y()) - (b.y() - a.y()) * (c.x() - b.x());
	return crossProduct < 0.0 ? -1 : crossProduct > 0.0 ? 1 : 0;
}

bool TransformModel::isVisibleInViewModeCallback(void *user, DP_LayerProps *lp)
{
	return !DP_layer_props_hidden(lp) &&
		   (DP_layer_props_children_noinc(lp) ||
			static_cast<const QSet<int> *>(user)->contains(
				DP_layer_props_id(lp)));
}

}

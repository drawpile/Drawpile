// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#define LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#include "libclient/drawdance/liquify.h"
#include "libclient/net/message.h"
#include "libclient/utils/transformquad.h"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QVector>
#include <functional>

class KisLiquifyTransformWorker;

namespace drawdance {
class CanvasState;
}

namespace canvas {

class CanvasModel;

class TransformModel : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(TransformModel)
public:
	TransformModel(CanvasModel *canvas);

	bool isActive() const { return m_active; }
	bool isLiquify() const { return !m_liquify.isNull(); }
	bool isTransformActive() const { return m_active && m_liquify.isNull(); }
	bool isLiquifyActive() const { return m_active && !m_liquify.isNull(); }
	bool isMovedFromCanvas() const { return m_active && !m_pasted; }
	bool isAffectedByLayerAlphaLock() const { return m_pasted && !m_stamped; }
	bool isDstQuadValid() const { return m_dstQuadValid; }
	bool isJustApplied() const { return m_justApplied; }
	bool isPreviewAccurate() const;
	bool canPreviewAccurate() const;
	const QSet<int> &layerIds() const { return m_layerIds; }
	TransformQuad srcQuad() const { return TransformQuad(m_srcBounds); }
	const TransformQuad &dstQuad() const { return m_dstQuad; }
	const QImage &floatingImage();
	QPoint floatingImageOffset() { return m_floatingImageOffset; }
	QImage layerImage(int layerId);
	int blendMode() const { return m_blendMode; }
	qreal opacity() const { return m_opacity; }

	bool isStampable() const
	{
		return (m_pasted || m_layerIds.size() == 1) && m_liquify.isNull();
	}

	void beginFromCanvas(
		const QRect &srcBounds, const QImage &mask,
		const QSet<int> &sourceLayerIds);

	void beginFloating(const QRect &srcBounds, const QImage &image);

	void beginLiquifyFromCanvas(
		const QRect &srcBounds, const QImage &mask,
		const QSet<int> &sourceLayerIds, int liquifyInterpolation);

	void setDeselectOnApply(bool deselectOnApply);
	void setDstQuad(const TransformQuad &dstQuad);
	void setPreviewAccurate(bool previewAccurate);
	void setLiquifyInterpolation(int liquifyInterpolation);
	void setBlendMode(int blendMode);
	void setOpacity(qreal opacity);

	void liquify(const std::function<void(drawdance::Liquify &)> &fn);
	drawdance::LiquifyState liquifyState() const;
	void setLiquifyState(const drawdance::LiquifyState &state);

	void applyOffset(int x, int y);

	QVector<net::Message> applyActiveTransform(
		uint8_t contextId, int layerId, int interpolation, bool stamp,
		bool *outMovedSelection = nullptr);

	bool isAllowedToApplyActiveTransform() const;

	void endActiveTransform(bool applied);

	int getEffectiveInterpolation(int interpolation) const;

	int getSingleLayerMoveId(int layerId) const;

signals:
	void transformChanged();
	void transformCut(
		const QSet<int> &layerIds, const QRect &maskBounds, const QImage &mask);
	void transformCutCleared();
	void liquifyPreviewRequested();

private:
	QVector<net::Message> applyFromCanvas(
		uint8_t contextId, int layerId, int interpolation,
		bool *outMovedSelection);

	void applyMoveRect(
		QVector<net::Message> &msgs, unsigned int contextId, int layerId,
		int sourceId, int srcX, int srcY, int dstTopLeftX, int dstTopLeftY,
		int srcW, int srcH, const QImage &mask, bool needsCutAndPaste) const;

	void applyTransformRegion(
		QVector<net::Message> &msgs, unsigned int contextId, int layerId,
		int sourceId, int srcX, int srcY, int srcW, int srcH, int dstTopLeftX,
		int dstTopLeftY, int dstTopRightX, int dstTopRightY,
		int dstBottomRightX, int dstBottomRightY, int dstBottomLeftX,
		int dstBottomLeftY, int interpolation, const QImage &mask,
		bool needsCutAndPaste) const;

	void applyTransformRegionSelection(
		QVector<net::Message> &msgs, unsigned int contextId, int srcX, int srcY,
		int srcW, int srcH, int dstTopLeftX, int dstTopLeftY, int dstTopRightX,
		int dstTopRightY, int dstBottomRightX, int dstBottomRightY,
		int dstBottomLeftX, int dstBottomLeftY, int interpolation,
		bool sizeOutOfBounds) const;

	bool isDstQuadBoundingRectAreaSizeOutOfBounds() const;

	void applyCut(
		QVector<net::Message> &msgs, unsigned int contextId, int sourceId,
		int srcX, int srcY, int srcW, int srcH, const QImage &mask) const;

	QVector<net::Message> applyFloating(
		uint8_t contextId, int layerId, int interpolation, bool stamp,
		bool *outMovedSelection);

	QVector<net::Message>
	applyLiquify(uint8_t contextId, int layerId, int interpolation);

	void applyLiquifyCutAndPaste(
		QVector<net::Message> &msgs,
		const drawdance::LiquifyState &liquifyState, unsigned int contextId,
		int layerId, int sourceId, int srcX, int srcY, int srcW, int srcH,
		const QImage &mask, int interpolation);

	void clear();

	void updateLayerIds();
	void setLayers(const QSet<int> &layerIds);

	QImage getLayerImage(int layerId) const;
	QImage getLayerImageWithMask(int layerId, const QImage &mask) const;
	QImage getMergedImage() const;
	static QImage getMergedImageFrom(
		const drawdance::CanvasState &canvasState, const QImage &mask,
		const QSet<int> &layerIds, QRect srcBounds);
	static void applyMaskToImage(QImage &img, const QImage &mask);
	void applyOpacityToImage(QImage &img) const;
	static bool isImageBlank(const QImage &img);
	static bool isImageOpaque(const QImage &img);

	static bool isMaskRelevant(const QImage &mask);
	bool moveIsOnlyTranslated() const;
	bool hasContentInSelection(int layerId) const;
	QImage convertMaskToMono() const;
	static bool containsNullMessages(const QVector<net::Message> &msgs);

	QPolygonF srcPolygon() const;
	bool isRightAngleRotationOrReflection(const QTransform &t) const;

	uint8_t getUint8Opacity() const
	{
		return qBound(0, qRound(m_opacity * 255.0), 255);
	}

	int getEffectiveBlendModeForLayer(int layerId) const;

	void emitLiquifyPreviewRequest();
	void requestLiquifyPreviewUpdate();
	void runLiquifyPreviewUpdate();
	void handleLiquifyPreviewUpdate(
		unsigned int id,
		const drawdance::LiquifyTransformer &liquifyTransformer, bool changed);

	static bool isQuadValid(const TransformQuad &quad);

	static int
	crossProductSign(const QPointF &a, const QPointF &b, const QPointF &c);

	static bool isVisibleInViewModeCallback(void *user, DP_LayerProps *lp);

	CanvasModel *m_canvas;
	drawdance::Liquify m_liquify;
	drawdance::LiquifyTransformer m_liquifyTransformer;
	bool m_active = false;
	bool m_pasted = false;
	bool m_deselectOnApply = false;
	bool m_stamped = false;
	bool m_dstQuadValid = false;
	bool m_justApplied = false;
	bool m_previewAccurate = true;
	bool m_liquifyPreviewRequested = false;
	bool m_liquifyPreviewInProgress = false;
	bool m_liquifyPreviewPending = false;
	QSet<int> m_layerIds;
	QRect m_srcBounds;
	TransformQuad m_dstQuad;
	QImage m_mask;
	QImage m_floatingImage;
	QHash<int, QImage> m_layerImages;
	unsigned int m_liquifyPreviewId = 0u;
	QPoint m_floatingImageOffset;
	int m_liquifyInterpolation;
	int m_blendMode;
	qreal m_opacity = 1.0;
};

}

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#define LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#include "libclient/net/message.h"
#include "libclient/utils/transformquad.h"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QVector>

namespace canvas {

class CanvasModel;

class TransformModel : public QObject {
	Q_OBJECT
public:
	TransformModel(CanvasModel *canvas);

	bool isActive() const { return m_active; }
	bool isMovedFromCanvas() const { return m_active && !m_pasted; }
	bool isStampable() const { return m_pasted || m_layerIds.size() == 1; }
	bool isDstQuadValid() const { return m_dstQuadValid; }
	bool isJustApplied() const { return m_justApplied; }
	bool isPreviewAccurate() const;
	bool canPreviewAccurate() const;
	const QSet<int> &layerIds() const { return m_layerIds; }
	TransformQuad srcQuad() const { return TransformQuad(m_srcBounds); }
	const TransformQuad &dstQuad() const { return m_dstQuad; }
	const QImage &floatingImage();
	QImage layerImage(int layerId);

	void beginFromCanvas(
		const QRect &srcBounds, const QImage &mask, int sourceLayerId);

	void beginFloating(const QRect &srcBounds, const QImage &image);

	void setDeselectOnApply(bool deselectOnApply);
	void setDstQuad(const TransformQuad &dstQuad);
	void setPreviewAccurate(bool previewAccurate);

	void applyOffset(int x, int y);

	QVector<net::Message> applyActiveTransform(
		bool disguiseAsPutImage, uint8_t contextId, int layerId,
		int interpolation, bool compatibilityMode, bool stamp,
		bool *outMovedSelection = nullptr);

	void endActiveTransform(bool applied);

	int getEffectiveInterpolation(int interpolation) const;

	int getSingleLayerMoveId(int layerId) const;

signals:
	void transformChanged();
	void transformCut(
		const QSet<int> &layerIds, const QRect &maskBounds, const QImage &mask);
	void transformCutCleared();

private:
	QVector<net::Message> applyFromCanvas(
		bool disguiseAsPutImage, uint8_t contextId, int layerId,
		int interpolation, bool compatibilityMode, bool *outMovedSelection);

	QVector<net::Message> applyFloating(
		bool disguiseAsPutImage, uint8_t contextId, int layerId,
		int interpolation, bool compatibilityMode, bool stamp,
		bool *outMovedSelection);

	void clear();

	void updateLayerIds();
	void setLayers(const QSet<int> &layerIds);

	QImage getLayerImage(int layerId) const;
	QImage getMergedImage() const;
	void applyMaskToImage(QImage &img) const;

	static bool isMaskRelevant(const QImage &mask);
	bool moveIsOnlyTranslated() const;
	QImage convertMaskToMono() const;
	static bool containsNullMessages(const QVector<net::Message> &msgs);

	bool isRightAngleRotationOrReflection(const QTransform &t) const;

	static bool isQuadValid(const TransformQuad &quad);

	static int
	crossProductSign(const QPointF &a, const QPointF &b, const QPointF &c);

	static bool isVisibleInViewModeCallback(void *user, DP_LayerProps *lp);

	CanvasModel *m_canvas;
	bool m_active = false;
	bool m_pasted = false;
	bool m_deselectOnApply = false;
	bool m_stamped = false;
	bool m_dstQuadValid = false;
	bool m_justApplied = false;
	bool m_previewAccurate = true;
	QSet<int> m_layerIds;
	QRect m_srcBounds;
	TransformQuad m_dstQuad;
	QImage m_mask;
	QImage m_floatingImage;
	QHash<int, QImage> m_layerImages;
};

}

#endif

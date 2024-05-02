// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#define LIBCLIENT_CANVAS_TRANSFORMMODEL_H
#include "libclient/net/message.h"
#include "libclient/utils/transformquad.h"
#include <QImage>
#include <QObject>
#include <QVector>

namespace canvas {

class TransformModel : public QObject {
	Q_OBJECT
public:
	TransformModel(QObject *parent = nullptr);

	bool isActive() const { return m_active; }
	bool isPasted() const { return m_pasted; }
	bool isDstQuadValid() const { return m_dstQuadValid; }
	bool isJustApplied() const { return m_justApplied; }
	TransformQuad srcQuad() const { return TransformQuad(m_srcBounds); }
	const TransformQuad &dstQuad() const { return m_dstQuad; }
	QImage image() const { return m_image; }

	void beginFromCanvas(
		const QRect &srcBounds, const QImage &mask, const QImage &image,
		int sourceLayerId);

	void beginFloating(const QRect &srcBounds, const QImage &image);

	void setDstQuad(const TransformQuad &dstQuad);

	void applyOffset(int x, int y);

	QVector<net::Message> applyActiveTransform(
		uint8_t contextId, int layerId, int interpolation,
		bool compatibilityMode, bool stamp);

	void endActiveTransform(bool applied);

	int getEffectiveInterpolation(int interpolation) const;

signals:
	void transformChanged();
	void transformCut(int layerId, const QRect &maskBounds, const QImage &mask);
	void transformCutCleared();

private:
	QVector<net::Message> applyFromCanvas(
		uint8_t contextId, int layerId, int interpolation,
		bool compatibilityMode);

	QVector<net::Message> applyFloating(
		uint8_t contextId, int layerId, int interpolation,
		bool compatibilityMode, bool stamp);

	void clear();

	bool moveNeedsMask() const;
	bool moveIsOnlyTranslated() const;
	QImage convertMaskToMono() const;
	static bool containsNullMessages(const QVector<net::Message> &msgs);

	bool isRightAngleRotationOrReflection(const QTransform &t) const;

	static bool isQuadValid(const TransformQuad &quad);

	static int
	crossProductSign(const QPointF &a, const QPointF &b, const QPointF &c);

	bool m_active = false;
	bool m_pasted = false;
	bool m_stamped = false;
	bool m_dstQuadValid = false;
	bool m_justApplied = false;
	int m_sourceLayerId = 0;
	QRect m_srcBounds;
	TransformQuad m_dstQuad;
	QImage m_mask;
	QImage m_image;
};

}

#endif
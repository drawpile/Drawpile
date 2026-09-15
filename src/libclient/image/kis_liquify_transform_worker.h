/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef LIBCLIENT_IMAGE_KIS_LIQUIFY_TRANSFORM_WORKER_H
#define LIBCLIENT_IMAGE_KIS_LIQUIFY_TRANSFORM_WORKER_H
#include "libclient/image/KisSpatialContainer.h"
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <functional>

class QTransform;


class KisLiquifyTransformWorker final {
public:
	struct State {
		QVector<QPointF> transformedPoints;
		KisSpatialContainer transformedPointsContainer;
		QRectF accumulatedBrushStrokes;
	};

	KisLiquifyTransformWorker(const QRect &srcBounds, int pixelPrecision = 8);

	bool operator==(const KisLiquifyTransformWorker &other) const;

	bool isIdentity() const
	{
		return pointsFuzzyEqual(m_originalPoints, m_transformedPoints);
	}

	int pointToIndex(const QPoint &cellPt);

	QSize gridSize() const { return m_gridSize; }

	void translatePoints(
		QPointF base, QPointF offset, qreal sigma, bool wash, qreal flow);

	void
	scalePoints(QPointF base, qreal scale, qreal sigma, bool wash, qreal flow);

	void
	rotatePoints(QPointF base, qreal angle, qreal sigma, bool wash, qreal flow);

	void undoPoints(const QPointF base, qreal amount, qreal sigma);

	const QVector<QPointF> &originalPoints() const { return m_originalPoints; }
	QVector<QPointF> &transformedPoints() { return m_transformedPoints; }

	QImage runOnQImage(
		const QImage &srcImage, QPointF srcImageOffset,
		const QTransform &imageToThumbTransform, bool bilinear,
		QPointF &outNewOffset);

	void translate(const QPointF &offset);
	void translateDstSpace(const QPointF &offset);

	QRect approxChangeRect(const QRect &rc);
	QRect approxNeedRect(const QRect &rc, const QRect &fullBounds);
	QRectF accumulatedStrokesBounds() const;

	void transformSrcAndDst(const QTransform &t);

	State state() const
	{
		return {
			m_transformedPoints,
			m_transformedPointsContainer,
			m_accumulatedBrushStrokes,
		};
	}

	void setState(const State &state)
	{
		m_transformedPoints = state.transformedPoints;
		m_transformedPointsContainer = state.transformedPointsContainer;
		m_accumulatedBrushStrokes = state.accumulatedBrushStrokes;
	}

private:
	using ProcessFn = std::function<QPointF(QPointF, QPointF, qreal)>;

	static constexpr qreal MAX_DIST_COEFF = 3.0;

	void preparePoints();

	void processTransformedPixels(
		QPointF base, qreal sigma, bool wash, qreal flow, const ProcessFn &fn);

	static bool
	pointsFuzzyEqual(const QVector<QPointF> &a, const QVector<QPointF> &b);

	QVector<QPointF> m_originalPoints;
	QVector<QPointF> m_transformedPoints;
	KisSpatialContainer m_originalPointsContainer;
	KisSpatialContainer m_transformedPointsContainer;
	QRectF m_accumulatedBrushStrokes;
	QRect m_srcBounds;
	QSize m_gridSize;
	int m_pixelPrecision;
};

#endif

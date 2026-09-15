/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "libclient/image/kis_liquify_transform_worker.h"
#include "libclient/image/geom.h"
#include "libclient/image/kis_grid_interpolation_tools.h"
#include <QTransform>
#include <QtMath>

KisLiquifyTransformWorker::KisLiquifyTransformWorker(
	const QRect &srcBounds, int pixelPrecision)
	: m_originalPointsContainer(srcBounds)
	, m_transformedPointsContainer(srcBounds)
	, m_srcBounds(srcBounds)
	, m_pixelPrecision(pixelPrecision)
{
	preparePoints();
}

bool KisLiquifyTransformWorker::operator==(
	const KisLiquifyTransformWorker &other) const
{
	return this == &other ||
		   (m_srcBounds == other.m_srcBounds &&
			m_pixelPrecision == other.m_pixelPrecision &&
			m_gridSize == other.m_gridSize &&
			m_originalPoints.size() == other.m_originalPoints.size() &&
			m_transformedPoints.size() == other.m_transformedPoints.size() &&
			pointsFuzzyEqual(m_originalPoints, other.m_originalPoints) &&
			pointsFuzzyEqual(m_transformedPoints, other.m_transformedPoints));
}

int KisLiquifyTransformWorker::pointToIndex(const QPoint &cellPt)
{
	return GridIterationTools::pointToIndex(cellPt, m_gridSize);
}

struct AllPointsFetcherOp {
	AllPointsFetcherOp(QRectF srcRect)
		: m_srcRect(srcRect)
	{
	}

	inline void processPoint(
		int col, int row, int prevCol, int prevRow, int colIndex, int rowIndex)
	{

		Q_UNUSED(prevCol);
		Q_UNUSED(prevRow);
		Q_UNUSED(colIndex);
		Q_UNUSED(rowIndex);

		QPointF pt(col, row);
		m_points.append(pt);
	}

	inline void nextLine() {}

	QVector<QPointF> m_points;
	QRectF m_srcRect;
};

void KisLiquifyTransformWorker::translate(const QPointF &offset)
{
	Q_ASSERT(m_originalPoints.size() == m_transformedPoints.size());

	// TODO: make it within Spatial Container, either a hidden offset, or just
	// offsetting all points at once and benchmark
	int count = m_transformedPoints.size();
	for(int i = 0; i < count; ++i) {
		m_originalPointsContainer.movePoint(
			i, m_originalPoints[i], m_originalPoints[i] + offset);
		m_transformedPointsContainer.movePoint(
			i, m_transformedPoints[i], m_transformedPoints[i] + offset);

		m_originalPoints[i] += offset;
		m_transformedPoints[i] += offset;
	}

	m_accumulatedBrushStrokes.translate(offset);
}

void KisLiquifyTransformWorker::translateDstSpace(const QPointF &offset)
{
	// TODO: make it within Spatial Container, either a hidden offset, or just
	// offsetting all points at once and benchmark
	int count = m_transformedPoints.size();
	for(int i = 0; i < count; ++i) {
		m_transformedPointsContainer.movePoint(
			i, m_transformedPoints[i], m_transformedPoints[i] + offset);
		m_transformedPoints[i] += offset;
	}
}

void KisLiquifyTransformWorker::undoPoints(
	QPointF base, qreal amount, qreal sigma)
{
	Q_ASSERT(m_originalPoints.size() == m_transformedPoints.size());

	QVector<int> indexes;
	qreal maxDist = MAX_DIST_COEFF * sigma;
	m_transformedPointsContainer.findAllInRange(indexes, base, maxDist);
	for(int i = 0; i < indexes.count(); i++) {

		QPointF diff = m_transformedPoints[indexes[i]] - base;
		qreal dist = geom::norm(diff);
		qreal lambda = qExp(-0.5 * geom::square(dist / sigma));
		lambda *= amount;

		QPointF oldPosition = m_transformedPoints[indexes[i]];
		m_transformedPoints[indexes[i]] =
			m_originalPoints[indexes[i]] * lambda +
			m_transformedPoints[indexes[i]] * (1.0 - lambda);

		m_transformedPointsContainer.movePoint(
			indexes[i], oldPosition, m_transformedPoints[indexes[i]]);
	}
}

void KisLiquifyTransformWorker::translatePoints(
	QPointF base, QPointF offset, qreal sigma, bool wash, qreal flow)
{
	processTransformedPixels(
		base, sigma, wash, flow,
		[offset](QPointF pt, QPointF diff, qreal lambda) {
			Q_UNUSED(diff);
			return pt + lambda * offset;
		});
}

void KisLiquifyTransformWorker::scalePoints(
	QPointF base, qreal scale, qreal sigma, bool wash, qreal flow)
{
	processTransformedPixels(
		base, sigma, wash, flow,
		[base, scale](QPointF pt, QPointF diff, qreal lambda) {
			Q_UNUSED(pt);
			Q_UNUSED(diff);
			return base + (1.0 + scale * lambda) * diff;
		});
}

void KisLiquifyTransformWorker::rotatePoints(
	QPointF base, qreal angle, qreal sigma, bool wash, qreal flow)
{
	processTransformedPixels(
		base, sigma, wash, flow,
		[base, angle](QPointF pt, QPointF diff, qreal lambda) {
			Q_UNUSED(pt);
			qreal langle = angle * lambda;
			qreal sinA = qSin(langle);
			qreal cosA = qCos(langle);
			qreal x = cosA * diff.x() + sinA * diff.y();
			qreal y = -sinA * diff.x() + cosA * diff.y();
			return base + QPointF(x, y);
		});
}

QRect KisLiquifyTransformWorker::approxChangeRect(const QRect &rc)
{
	QRect resultRect = m_transformedPointsContainer.exactBounds().toRect();
	return geom::blowRect(resultRect | rc, 0.05);
}

QRect KisLiquifyTransformWorker::approxNeedRect(
	const QRect &rc, const QRect &fullBounds)
{
	Q_UNUSED(rc);
	return fullBounds;
}

QRectF KisLiquifyTransformWorker::accumulatedStrokesBounds() const
{
	return m_accumulatedBrushStrokes;
}

void KisLiquifyTransformWorker::transformSrcAndDst(const QTransform &t)
{
	Q_ASSERT(t.type() <= QTransform::TxScale);

	m_srcBounds = t.mapRect(m_srcBounds);

	// TODO: do it within Spatial Container
	for(int i = 0; i < m_transformedPoints.count(); i++) {
		m_originalPointsContainer.movePoint(
			i, m_originalPoints[i], t.map(m_originalPoints[i]));
		m_transformedPointsContainer.movePoint(
			i, m_transformedPoints[i], t.map(m_transformedPoints[i]));

		m_originalPoints[i] = t.map(m_originalPoints[i]);
		m_transformedPoints[i] = t.map(m_transformedPoints[i]);
	}

	m_accumulatedBrushStrokes = t.map(m_accumulatedBrushStrokes).boundingRect();

	if(t == QTransform::fromScale(t.m11(), t.m22()) && t.m11() == t.m22()) {
		m_pixelPrecision *= t.m11();
		Q_ASSERT(m_pixelPrecision > 0);
		Q_ASSERT(
			(QList<int>({1, 2, 4, 8, 16}).contains(m_pixelPrecision) ||
			 m_pixelPrecision % 16 == 0));
		// should check if pixelPrecision is a power of 2, but that's more
		// complicated
	}
}

QImage KisLiquifyTransformWorker::runOnQImage(
	const QImage &srcImage, QPointF srcImageOffset,
	const QTransform &imageToThumbTransform, bool bilinear,
	QPointF &outNewOffset)
{
	if(srcImage.isNull() ||
	   srcImage.format() != QImage::Format_ARGB32_Premultiplied) {
		return QImage();
	}

	int count = m_originalPoints.size();
	Q_ASSERT(count == m_transformedPoints.size());

	bool identityTransform = imageToThumbTransform.isIdentity();
	QVector<QPointF> originalPointsLocal;
	QVector<QPointF> transformedPointsLocal;
	if(identityTransform) {
		originalPointsLocal = m_originalPoints;
		transformedPointsLocal = m_transformedPoints;
	} else {
		originalPointsLocal.reserve(count);
		transformedPointsLocal.reserve(count);
		for(QPointF p : m_originalPoints) {
			originalPointsLocal.append(imageToThumbTransform.map(p));
		}
		for(QPointF p : m_transformedPoints) {
			transformedPointsLocal.append(imageToThumbTransform.map(p));
		}
	}

	QRectF dstBounds;
	for(QPointF pt : transformedPointsLocal) {
		geom::accumulateBounds(pt, dstBounds);
	}

	QRectF srcBounds(srcImageOffset, srcImage.size());
	dstBounds |= srcBounds;

	QPointF dstQImageOffset = dstBounds.topLeft();
	outNewOffset = dstQImageOffset;

	QRect dstBoundsI = dstBounds.toAlignedRect();

	QImage dstImage(dstBoundsI.size(), srcImage.format());
	dstImage.fill(0);

	GridIterationTools::QImagePolygonOp polygonOp(
		srcImage, dstImage, srcImageOffset, dstQImageOffset, bilinear);
	GridIterationTools::RegularGridIndexesOp indexesOp(m_gridSize);


	QRect correctSubGrid = GridIterationTools::calculateCorrectSubGrid(
		m_srcBounds, m_pixelPrecision, m_accumulatedBrushStrokes, m_gridSize);
	bool canMergeRects = GridIterationTools::canProcessRectsInRandomOrder(
		indexesOp, m_transformedPoints, correctSubGrid);
	polygonOp.setCanMergeRects(canMergeRects);


	GridIterationTools::iterateThroughGrid(
		polygonOp, indexesOp, m_gridSize, originalPointsLocal,
		transformedPointsLocal, correctSubGrid);


	QList<QRectF> areasToCopy = GridIterationTools::cutOutSubgridFromBounds(
		correctSubGrid, m_srcBounds, m_gridSize, m_originalPoints);
	polygonOp.setCanMergeRects(false);

	qreal eps = 0.001;
	for(const QRectF areaToCopy : areasToCopy) {
		QPolygonF polygonToCopy = QPolygonF(areaToCopy);
		if(!identityTransform) {
			polygonToCopy = imageToThumbTransform.map(polygonToCopy);
		}

		if(geom::isPolygonPixelAlignedRect(polygonToCopy, eps)) {
			polygonOp.fastCopyArea(polygonToCopy.boundingRect().toRect());
		} else {
			polygonOp(polygonToCopy, polygonToCopy);
		}
	}

	return dstImage;
}

void KisLiquifyTransformWorker::preparePoints()
{
	m_gridSize =
		GridIterationTools::calcGridSize(m_srcBounds, m_pixelPrecision);

	AllPointsFetcherOp pointsOp(m_srcBounds);
	GridIterationTools::processGrid(pointsOp, m_srcBounds, m_pixelPrecision);

	int numPoints = pointsOp.m_points.size();
	Q_ASSERT(numPoints == m_gridSize.width() * m_gridSize.height());

	m_originalPoints = pointsOp.m_points;
	m_transformedPoints = pointsOp.m_points;

	m_originalPointsContainer.initializeWithGridPoints(
		m_srcBounds, m_pixelPrecision);
	m_transformedPointsContainer.initializeWithGridPoints(
		m_srcBounds, m_pixelPrecision);
}

void KisLiquifyTransformWorker::processTransformedPixels(
	const QPointF base, qreal sigma, bool wash, qreal flow, const ProcessFn &fn)
{
	Q_ASSERT(m_originalPoints.size() == m_transformedPoints.size());
	qreal maxDist = MAX_DIST_COEFF * sigma;
	QRectF clipRect(
		base.x() - maxDist, base.y() - maxDist, 2 * maxDist, 2 * maxDist);

	m_accumulatedBrushStrokes |=
		geom::growRect(clipRect, qreal(m_pixelPrecision));

	QVector<int> indexes;
	m_originalPointsContainer.findAllInRange(indexes, base, maxDist);
	int count = indexes.size();

	if(wash) {
		for(int i = 0; i < count; ++i) {
			QPointF diff = m_originalPoints[indexes[i]] - base;
			qreal dist = geom::norm(diff);

			qreal lambda = qExp(-0.5 * geom::square(dist / sigma));
			QPointF dstPt = fn(m_originalPoints[indexes[i]], diff, lambda);

			bool needsTransform =
				geom::distance(dstPt, m_originalPoints[indexes[i]]) >
				geom::distance(
					m_transformedPoints[indexes[i]],
					m_originalPoints[indexes[i]]);
			if(needsTransform) {
				QPointF oldPosition = m_transformedPoints[indexes[i]];
				m_transformedPoints[indexes[i]] =
					(1.0 - flow) * m_transformedPoints[indexes[i]] +
					flow * dstPt;

				m_transformedPointsContainer.movePoint(
					indexes[i], oldPosition, m_transformedPoints[indexes[i]]);
			}
		}
	} else {
		for(int i = 0; i < count; ++i) {
			QPointF diff = m_transformedPoints[indexes[i]] - base;
			qreal dist = geom::norm(diff);
			if(dist <= maxDist) {
				qreal lambda = qExp(-0.5 * geom::square(dist / sigma));
				QPointF oldPosition = m_transformedPoints[indexes[i]];
				m_transformedPoints[indexes[i]] =
					fn(m_transformedPoints[indexes[i]], diff, lambda);

				m_transformedPointsContainer.movePoint(
					indexes[i], oldPosition, m_transformedPoints[indexes[i]]);
			}
		}
	}
}

bool KisLiquifyTransformWorker::pointsFuzzyEqual(
	const QVector<QPointF> &a, const QVector<QPointF> &b)
{
	return geom::pointVectorsFuzzyEqual(a, b, 1e-6);
}

/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef LIBCLIENT_KIS_GRID_INTERPOLATION_TOOLS_H
#define LIBCLIENT_KIS_GRID_INTERPOLATION_TOOLS_H
#include "libclient/image/geom.h"
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <limits>

namespace GridIterationTools {

int calcGridDimension(int start, int end, int pixelPrecision);

QSize calcGridSize(const QRect srcBounds, const int pixelPrecision);

int pointToIndex(const QPoint cellPt, const QSize gridSize);


template <class ProcessCell>
void processGrid(
	ProcessCell &cellOp, const QRect &srcBounds, const int pixelPrecision)
{
	if(srcBounds.isEmpty()) {
		return;
	}

	const int alignmentMask = ~(pixelPrecision - 1);

	int prevRow = std::numeric_limits<int>::max();
	int prevCol = std::numeric_limits<int>::max();

	int rowIndex = 0;
	int colIndex = 0;

	for(int row = srcBounds.top(); row <= srcBounds.bottom();) {
		for(int col = srcBounds.left(); col <= srcBounds.right();) {

			cellOp.processPoint(col, row, prevCol, prevRow, colIndex, rowIndex);

			prevCol = col;
			col += pixelPrecision;
			++colIndex;

			if(col > srcBounds.right() &&
			   col <= srcBounds.right() + pixelPrecision - 1) {

				col = srcBounds.right();
			} else {
				col &= alignmentMask;
			}
		}

		cellOp.nextLine();
		colIndex = 0;

		prevRow = row;
		row += pixelPrecision;
		rowIndex++;

		if(row > srcBounds.bottom() &&
		   row <= srcBounds.bottom() + pixelPrecision - 1) {

			row = srcBounds.bottom();
		} else {
			row &= alignmentMask;
		}
	}
}


struct QImagePolygonOp final {
	static constexpr qreal EPS = 0.001;

	QImagePolygonOp(
		const QImage &srcImage, QImage &dstImage, const QPointF &srcImageOffset,
		const QPointF &dstImageOffset, bool bilinear);

	~QImagePolygonOp()
	{
		/**
		 * When setCanMergeRects() is set to true, the caller should
		 * call finalize() to process all the postponed rects, which
		 * would clear the vector
		 */
		Q_ASSERT(m_rectsToCopy.isEmpty());
	}

	void fastCopyArea(QRect areaToCopy)
	{
		fastCopyArea(areaToCopy, m_canMergeRects);
	}

	void fastCopyArea(QRect areaToCopy, bool lazy);

	void operator()(const QPolygonF &srcPolygon, const QPolygonF &dstPolygon)
	{
		(*this)(srcPolygon, dstPolygon, dstPolygon);
	}

	void operator()(
		const QPolygonF &srcPolygon, const QPolygonF &dstPolygon,
		const QPolygonF &clipDstPolygon);

	void copyPreviousRects();

	void finalize() { copyPreviousRects(); }

	/**
	 * IMPORTANT: When setCanMergeRects() is set to `true`,
	 * the caller should calls finalize() in the end of
	 * the processing action to actually copy all the lazily
	 * postponed rects.
	 */
	void setCanMergeRects(bool canMergeRects)
	{
		m_canMergeRects = canMergeRects;
	}

	const QImage &m_srcImage;
	QImage &m_dstImage;
	QPointF m_srcImageOffset;
	QPointF m_dstImageOffset;

	QRect m_srcImageRect;
	QRect m_dstImageRect;

private:
	void copyPixelNearest(QPoint srcPos, QPoint dstPos);
	void copyPixelBilinear(QPointF srcPosF, QPoint dstPos);

	quint32 srcPixelAt(int px, int py) const
	{
		return reinterpret_cast<const quint32 *>(
			m_srcImage.constScanLine(py))[px];
	}

	quint32 &dstPixelAt(int px, int py)
	{
		return reinterpret_cast<quint32 *>(m_dstImage.scanLine(py))[px];
	}

	const bool m_bilinear;
	bool m_canMergeRects = false;
	QVector<QRect> m_rectsToCopy;
};


/*************************************************************/
/*      Iteration through precalculated grid                 */
/*************************************************************/

/**
 *    A-----B         The polygons will be in the following order:
 *    |     |
 *    |     |         polygon << A << B << D << C;
 *    C-----D
 */

static inline QVector<int>
calculateCellIndexes(int col, int row, const QSize &gridSize)
{
	int tl = col + row * gridSize.width();
	int tr = tl + 1;
	int bl = tl + gridSize.width();
	int br = bl + 1;
	return QVector<int>({tl, tr, br, bl});
}

QRect calculateCorrectSubGrid(
	QRect originalBoundsForGrid, int pixelPrecision, QRectF currentBounds,
	QSize gridSize);

QList<QRectF> cutOutSubgridFromBounds(
	QRect subGrid, QRect srcBounds, QSize gridSize,
	const QVector<QPointF> &originalPoints);


struct RegularGridIndexesOp {
	RegularGridIndexesOp(const QSize &gridSize)
		: m_gridSize(gridSize)
	{
	}

	inline QVector<int>
	calculateMappedIndexes(int col, int row, int *numExistingPoints) const
	{
		*numExistingPoints = 4;
		QVector<int> cellIndexes =
			GridIterationTools::calculateCellIndexes(col, row, m_gridSize);
		return cellIndexes;
	}

	inline int tryGetValidIndex(const QPoint &cellPt) const
	{
		Q_UNUSED(cellPt);
		Q_UNREACHABLE_RETURN(-1);
	}

	inline QPointF getSrcPointForce(const QPoint &cellPt) const
	{
		Q_UNUSED(cellPt);
		Q_UNREACHABLE_RETURN(QPointF());
	}

	inline const QPolygonF srcCropPolygon() const
	{
		Q_UNREACHABLE_RETURN(QPolygonF());
	}

	QSize m_gridSize;
};


/**
 * There is a weird problem in fetching correct bounds of the polygon.
 * If the rightmost (bottommost) point of the polygon is integral, then
 * QRectF() will end exactly on it, but when converting into QRect the last
 * point will not be taken into account. It happens due to the difference
 * between center-point/topleft-point point representation. In many cases
 * the latter is expected, but we don't work with it in Qt/Krita.
 */
static inline void adjustAlignedPolygon(QPolygonF &polygon)
{
	qreal eps = 1e-5;
	QPointF p1(eps, 0.0);
	QPointF p2(eps, eps);
	QPointF p3(0.0, eps);
	polygon[1] += p1;
	polygon[2] += p2;
	polygon[3] += p3;
}

template <class IndexesOp>
static bool canProcessRectsInRandomOrder(
	IndexesOp &indexesOp, const QVector<QPointF> &transformedPoints, QSize grid)
{
	return canProcessRectsInRandomOrder(
		indexesOp, transformedPoints, QRect(QPoint(0, 0), grid));
}

template <class IndexesOp>
static bool canProcessRectsInRandomOrder(
	IndexesOp &indexesOp, const QVector<QPointF> &transformedPoints,
	QRect subgrid)
{
	QVector<int> polygonPoints(4);
	QPoint startPoint = subgrid.topLeft();
	QPoint endPoint = subgrid.bottomRight();

	for(int row = startPoint.y(); row < endPoint.y(); ++row) {
		for(int col = startPoint.x(); col < endPoint.x(); ++col) {
			int numExistingPoints = 0;

			polygonPoints =
				indexesOp.calculateMappedIndexes(col, row, &numExistingPoints);

			QPolygonF dstPolygon;

			for(int i = 0; i < polygonPoints.count(); i++) {
				int index = polygonPoints[i];
				dstPolygon.append(transformedPoints[index]);
			}

			adjustAlignedPolygon(dstPolygon);

			if(!geom::isPolygonTrulyConvex(dstPolygon)) {
				return false;
			}
		}
	}

	return true;
}


template <class PolygonOp, class IndexesOp>
void iterateThroughGrid(
	PolygonOp &polygonOp, IndexesOp &indexesOp, const QSize &gridSize,
	const QVector<QPointF> &originalPoints,
	const QVector<QPointF> &transformedPoints, const QRect subGrid)
{
	QVector<int> polygonPoints(4);
	QPoint startPoint = subGrid.topLeft();
	QPoint endPoint = subGrid.bottomRight();
	// it's weird but bottomRight on QRect point does give us one unit of margin
	// on both x and y when start is on (0, 0), and size is (500, 500),
	// bottomRight is on (499, 499) but remember that it also only needs a top
	// left corner of the polygon

	if(!(startPoint.x() >= 0 && startPoint.y() >= 0 &&
		 endPoint.x() <= gridSize.width() - 1 &&
		 endPoint.y() <= gridSize.height() - 1)) {
		startPoint = QPoint(qMax(startPoint.x(), 0), qMax(startPoint.y(), 0));
		endPoint = QPoint(
			qMin(endPoint.x(), gridSize.width() - 1),
			qMin(startPoint.y(), gridSize.height() - 1));
	}

	for(int row = startPoint.y(); row < endPoint.y(); row++) {
		for(int col = startPoint.x(); col < endPoint.x(); col++) {
			int numExistingPoints = 0;

			polygonPoints =
				indexesOp.calculateMappedIndexes(col, row, &numExistingPoints);

			QPolygonF srcPolygon;
			QPolygonF dstPolygon;

			for(int i = 0; i < 4; i++) {
				int index = polygonPoints[i];
				srcPolygon.append(originalPoints[index]);
				dstPolygon.append(transformedPoints[index]);
			}

			adjustAlignedPolygon(srcPolygon);
			adjustAlignedPolygon(dstPolygon);

			polygonOp(srcPolygon, dstPolygon);
		}
	}

	polygonOp.finalize();
}

}

#endif

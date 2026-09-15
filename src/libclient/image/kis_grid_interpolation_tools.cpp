/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "libclient/image/kis_grid_interpolation_tools.h"
#include "libclient/image/geom.h"
#include "libclient/image/kis_four_point_interpolator_backward.h"
#ifdef DEBUG_PAINTING_POLYGONS
#	include <QPainter>
#endif


namespace GridIterationTools {

int calcGridDimension(int start, int end, int pixelPrecision)
{
	int alignmentMask = ~(pixelPrecision - 1);
	int alignedStart = (start + pixelPrecision - 1) & alignmentMask;
	int alignedEnd = end & alignmentMask;
	int size = 0;

	if(alignedEnd > alignedStart) {
		size = (alignedEnd - alignedStart) / pixelPrecision + 1;
		if(alignedStart != start) {
			++size;
		}
		if(alignedEnd != end) {
			++size;
		}
	} else {
		size = 2 + (end - start >= pixelPrecision);
	}

	return size;
}

QSize calcGridSize(const QRect srcBounds, const int pixelPrecision)
{
	return QSize(
		calcGridDimension(srcBounds.left(), srcBounds.right(), pixelPrecision),
		calcGridDimension(srcBounds.top(), srcBounds.bottom(), pixelPrecision));
}

int pointToIndex(const QPoint cellPt, const QSize gridSize)
{
	return cellPt.x() + cellPt.y() * gridSize.width();
}


QImagePolygonOp::QImagePolygonOp(
	const QImage &srcImage, QImage &dstImage, const QPointF &srcImageOffset,
	const QPointF &dstImageOffset, bool bilinear)
	: m_srcImage(srcImage)
	, m_dstImage(dstImage)
	, m_srcImageOffset(srcImageOffset)
	, m_dstImageOffset(dstImageOffset)
	, m_srcImageRect(m_srcImage.rect())
	, m_dstImageRect(m_dstImage.rect())
	, m_bilinear(bilinear)
{
}

void QImagePolygonOp::fastCopyArea(QRect areaToCopy, bool lazy)
{
	if(lazy) {
		m_rectsToCopy.append(areaToCopy.adjusted(0, 0, -1, -1));
		return;
	}

	// only handling saved offsets
	QRect srcArea = areaToCopy.translated(-m_srcImageOffset.toPoint());
	QRect dstArea = areaToCopy.translated(-m_dstImageOffset.toPoint());

	srcArea = srcArea.intersected(m_srcImageRect);
	dstArea = dstArea.intersected(m_dstImageRect);

	// it might look pointless but it cuts off unneeded areas on both rects
	// based on where they end up since *I know* they are the same rectangle
	// before translation
	// TODO: I'm pretty sure this logic is correct, but let's check it when
	// I'm less sleepy
	QRect srcAreaUntranslated = srcArea.translated(m_srcImageOffset.toPoint());
	QRect dstAreaUntranslated = dstArea.translated(m_dstImageOffset.toPoint());

	QRect actualCopyArea = srcAreaUntranslated.intersected(dstAreaUntranslated);
	srcArea = actualCopyArea.translated(-m_srcImageOffset.toPoint());
	dstArea = actualCopyArea.translated(-m_dstImageOffset.toPoint());

	int bytesPerPixel =
		m_srcImage.sizeInBytes() / m_srcImage.height() / m_srcImage.width();

	int srcX = srcArea.left() * bytesPerPixel;
	int dstX = dstArea.left() * bytesPerPixel;

	for(int srcY = srcArea.top(); srcY <= srcArea.bottom(); ++srcY) {
		int dstY = dstArea.top() + srcY - srcArea.top();
		const uchar *srcLine = m_srcImage.constScanLine(srcY);
		uchar *dstLine = m_dstImage.scanLine(dstY);
		memcpy(dstLine + dstX, srcLine + srcX, srcArea.width() * bytesPerPixel);
	}
}

void QImagePolygonOp::operator()(
	const QPolygonF &srcPolygon, const QPolygonF &dstPolygon,
	const QPolygonF &clipDstPolygon)
{
	bool samePolygon =
		(m_dstImage.format() == m_srcImage.format()) &&
		geom::polygonsFuzzyEqual(srcPolygon, dstPolygon, EPS) &&
		geom::polygonsFuzzyEqual(srcPolygon, clipDstPolygon, EPS);

	if(samePolygon && geom::isPolygonPixelAlignedRect(dstPolygon, EPS)) {
		QRect boundRect = dstPolygon.boundingRect().toAlignedRect();
		fastCopyArea(boundRect);
		return;
	}

	// provess previous rects so they are all processed
	// in the same order as without any performance improvements
	copyPreviousRects();

	KisFourPointInterpolatorBackward interp(srcPolygon, dstPolygon);

	QRect boundRect = clipDstPolygon.boundingRect().toAlignedRect();
	for(int y = boundRect.top(); y <= boundRect.bottom(); y++) {
		interp.setY(y);
		for(int x = boundRect.left(); x <= boundRect.right(); x++) {

			QPointF srcPoint(x, y);
			if(clipDstPolygon.containsPoint(srcPoint, Qt::OddEvenFill)) {

				interp.setX(srcPoint.x());
				QPointF dstPoint = interp.getValue();

				// about srcPoint/dstPoint hell please see a
				// comment in PaintDevicePolygonOp::operator() ()

				srcPoint -= m_dstImageOffset;
				dstPoint -= m_srcImageOffset;

				QPoint srcPointI = srcPoint.toPoint();
				QPoint dstPointI = dstPoint.toPoint();

				if(m_dstImageRect.contains(srcPointI) &&
				   m_srcImageRect.contains(dstPointI)) {
					// The naming of these points gets inverted when calling
					// these functions, since I can't get it in my head that
					// srcPoint is supposed to be in dstImage and vice-versa.
					if(m_bilinear) {
						copyPixelBilinear(dstPoint, srcPointI);
					} else {
						copyPixelNearest(dstPointI, srcPointI);
					}
				}
			}
		}
	}

#ifdef DEBUG_PAINTING_POLYGONS
	QPainter gc(&m_dstImage);
	gc.setPen(Qt::red);
	gc.setOpacity(0.5);

	gc.setBrush(Qt::green);
	gc.drawPolygon(clipDstPolygon.translated(-m_dstImageOffset));

	gc.setBrush(Qt::blue);
	// gc.drawPolygon(dstPolygon.translated(-m_dstImageOffset));

#endif /* DEBUG_PAINTING_POLYGONS */
}

void QImagePolygonOp::copyPreviousRects()
{
	QVector<QRect>::iterator end =
		geom::mergeSparseRects(m_rectsToCopy.begin(), m_rectsToCopy.end());

	for(QVector<QRect>::iterator it = m_rectsToCopy.begin(); it < end; it++) {
		QRect areaToCopy = *it;
		fastCopyArea(areaToCopy.adjusted(0, 0, 1, 1), false);
	}

	m_rectsToCopy.clear();
}

void QImagePolygonOp::copyPixelNearest(QPoint srcPos, QPoint dstPos)
{
	dstPixelAt(dstPos.x(), dstPos.y()) = srcPixelAt(srcPos.x(), srcPos.y());
}

void QImagePolygonOp::copyPixelBilinear(QPointF srcPosF, QPoint dstPos)
{
	// This fixed-point bilinear voodoo is wholly based on the Qt framework's
	// raster paint engine implementation, using it under the GNU General Public
	// License, version 3. See 3rdparty/licenses/qt/license.GPL3 for details.
	int xf = int(srcPosF.x() * 16.0);
	int yf = int(srcPosF.y() * 16.0);

	int x0 = xf >> 4;
	int y0 = yf >> 4;
	int x1 = x0 + 1;
	int y1 = y0 + 1;

	int right = m_srcImageRect.right();
	int bottom = m_srcImageRect.bottom();
	x0 = geom::bounds(0, x0, right);
	x1 = geom::bounds(0, x1, right);
	y0 = geom::bounds(0, y0, bottom);
	y1 = geom::bounds(0, y1, bottom);

	quint32 tx = xf & 0x0F;
	quint32 ty = yf & 0x0F;

	quint32 w00 = (16 - tx) * (16 - ty);
	quint32 w10 = tx * (16 - ty);
	quint32 w01 = (16 - tx) * ty;
	quint32 w11 = tx * ty;

	quint32 p00 = srcPixelAt(x0, y0);
	quint32 p10 = srcPixelAt(x1, y0);
	quint32 p01 = srcPixelAt(x0, y1);
	quint32 p11 = srcPixelAt(x1, y1);

	quint32 rb00 = p00 & 0x00ff00ff;
	quint32 rb10 = p10 & 0x00ff00ff;
	quint32 rb01 = p01 & 0x00ff00ff;
	quint32 rb11 = p11 & 0x00ff00ff;
	quint32 rb = (rb00 * w00 + rb10 * w10 + rb01 * w01 + rb11 * w11) >> 8;
	rb &= 0x00ff00ff;

	quint32 ag00 = (p00 >> 8) & 0x00ff00ff;
	quint32 ag10 = (p10 >> 8) & 0x00ff00ff;
	quint32 ag01 = (p01 >> 8) & 0x00ff00ff;
	quint32 ag11 = (p11 >> 8) & 0x00ff00ff;
	quint32 ag = (ag00 * w00 + ag10 * w10 + ag01 * w01 + ag11 * w11) >> 8;
	ag &= 0x00ff00ff;

	dstPixelAt(dstPos.x(), dstPos.y()) = rb | (ag << 8);
}

QRect calculateCorrectSubGrid(
	QRect originalBoundsForGrid, int pixelPrecision, QRectF currentBounds,
	QSize gridSize)
{
	if(!QRectF(originalBoundsForGrid).intersects(currentBounds)) {
		return QRect();
	}

	QPointF imaginaryGridStartF =
		QPoint(
			originalBoundsForGrid.x() / pixelPrecision,
			originalBoundsForGrid.y() / pixelPrecision) *
		pixelPrecision;

	QPointF startPointB = currentBounds.topLeft() - imaginaryGridStartF;
	QPoint startPointG = QPoint(
		startPointB.x() / pixelPrecision, startPointB.y() / pixelPrecision);
	startPointG = QPoint(
		geom::bounds(0, startPointG.x(), gridSize.width()),
		geom::bounds(0, startPointG.y(), gridSize.height()));

	QPointF endPointB =
		currentBounds.bottomRight() + QPoint(1, 1) - imaginaryGridStartF;
	QPoint endPointG = QPoint(
						   std::ceil(endPointB.x() / pixelPrecision),
						   std::ceil(endPointB.y() / pixelPrecision)) +
					   QPoint(1, 1);
	QPoint endPointPotential = endPointG;

	QPoint trueEndPoint = QPoint(
		geom::bounds(0, endPointPotential.x(), gridSize.width()),
		geom::bounds(0, endPointPotential.y(), gridSize.height()));

	QPoint size = trueEndPoint - startPointG;

	return QRect(startPointG, QSize(size.x(), size.y()));
}


QList<QRectF> cutOutSubgridFromBounds(
	QRect subGrid, QRect srcBounds, QSize gridSize,
	const QVector<QPointF> &originalPoints)
{
	if(subGrid.width() == 0 || subGrid.height() == 0) {
		return QList<QRectF>{srcBounds};
	}
	QPoint topLeft = subGrid.topLeft();
	QPoint bottomRight =
		subGrid.topLeft() + QPoint(subGrid.width() - 1, subGrid.height() - 1);

	int topLeftIndex = pointToIndex(topLeft, gridSize);
	int bottomRightIndex = pointToIndex(bottomRight, gridSize);

	topLeftIndex = qMax(0, qMin(topLeftIndex, originalPoints.length() - 1));
	bottomRightIndex =
		qMax(0, qMin(bottomRightIndex, originalPoints.length() - 1));

	QPointF topLeftReal = originalPoints[topLeftIndex];
	QPointF bottomRightReal = originalPoints[bottomRightIndex];
	QRectF cutOut = QRectF(topLeftReal, bottomRightReal);

	QList<QRectF> response;
	// *-----------*
	// |    top    |
	// |-----------|
	// | l |xxx| r |
	// | e |xxx| i |
	// | f |xxx| g |
	// | t |xxx| h |
	// |   |xxx| t |
	// |-----------|
	// |   bottom  |
	// *-----------*


	QRectF top = QRectF(
		srcBounds.topLeft(), QPointF(srcBounds.right() + 1, topLeftReal.y()));
	QRectF bottom = QRectF(
		QPointF(srcBounds.left(), bottomRightReal.y() + 1),
		srcBounds.bottomRight() + QPointF(1, 1));
	QRectF left = QRectF(
		QPointF(srcBounds.left(), cutOut.top()),
		QPointF(cutOut.left(), cutOut.bottom() + 1));
	QRectF right = QRectF(
		QPointF(cutOut.right() + 1, cutOut.top()),
		QPointF(srcBounds.right() + 1, cutOut.bottom() + 1));
	QList<QRectF> rects = {top, left, right, bottom};
	for(int i = 0; i < rects.length(); i++) {
		if(!rects[i].isEmpty()) {
			response.append(rects[i]);
		}
	}
	return response;
}

}

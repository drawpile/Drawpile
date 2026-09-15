/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "libclient/image/geom.h"
#include <QPolygonF>

namespace geom {

template <typename T> bool pointsFuzzyEqual(const T &a, const T &b, qreal eps)
{
	int count = a.count();
	if(count != b.count()) {
		return false;
	}

	for(int i = 0; i < count; ++i) {
		if(!pointFuzzyEqual(a[i], b[i], eps)) {
			return false;
		}
	}

	return true;
}

bool pointVectorsFuzzyEqual(
	const QVector<QPointF> &a, const QVector<QPointF> &b, qreal eps)
{
	return pointsFuzzyEqual<QVector<QPointF>>(a, b, eps);
}

bool polygonsFuzzyEqual(const QPolygonF &a, const QPolygonF &b, qreal eps)
{
	return pointsFuzzyEqual<QPolygonF>(a, b, eps);
}

bool isPolygonRect(const QPolygonF &poly, qreal tolerance)
{
	auto sameVert = [tolerance](QPointF p1, QPointF p2) {
		return qAbs(p2.x() - p1.x()) < tolerance;
	};

	auto sameHoriz = [tolerance](QPointF p1, QPointF p2) {
		return qAbs(p2.y() - p1.y()) < tolerance;
	};

	int rectCorners = 4;
	if(poly.length() != rectCorners) {
		if(poly.length() != rectCorners + 1 ||
		   !pointFuzzyEqual(poly[0], poly[rectCorners], tolerance)) {
			return false;
		}
	}

	return (sameVert(poly[0], poly[1]) && sameHoriz(poly[1], poly[2]) &&
			sameVert(poly[2], poly[3]) && sameHoriz(poly[3], poly[0])) ||
		   (sameHoriz(poly[0], poly[1]) && sameVert(poly[1], poly[2]) &&
			sameHoriz(poly[2], poly[3]) && sameVert(poly[3], poly[0]));
}

bool isPolygonPixelAlignedRect(const QPolygonF &poly, qreal tolerance)
{
	if(!isPolygonRect(poly, tolerance)) {
		return false;
	}

	for(int i = 0; i < poly.length(); i++) {
		if(!pointFuzzyEqual(poly[i], QPointF(poly[i].toPoint()), tolerance)) {
			return false;
		}
	}

	return true;
}

namespace {
static int wrapInt(int value, int wrapBounds)
{
	value %= wrapBounds;
	if(value < 0) {
		value += wrapBounds;
	}
	return value;
}
}

bool isPolygonTrulyConvex(const QPolygonF &polygon, bool ensureNoLoops)
{
	int numPoints = polygon.size();
	if(numPoints < 3) {
		return true;
	}

	if(pointFuzzyCompare(polygon[0], polygon[numPoints - 1])) {
		// common in QPainterPaths to have the startPoint and endPoint the same
		--numPoints;
	}

	int sign = 0;
	qreal angleSum = 0;

	for(int i = 0; i < numPoints; i++) {
		int a = i;
		int b = wrapInt(i + 1, numPoints);
		int c = wrapInt(i + 2, numPoints);

		qreal angle = angleBetweenVectors(
			static_cast<QPointF>(polygon[b] - polygon[a]),
			static_cast<QPointF>(polygon[c] - polygon[b]));

		if(angle < -M_PI) {
			angle += 2 * M_PI;
		}

		if(angle > M_PI) {
			angle -= 2 * M_PI;
		}

		angleSum += angle;

		if(sign == 0) {
			sign = intSignZeroF(angle);
		} else {
			int currentSign = intSignZeroF(angle);
			if(currentSign != 0 && currentSign != sign) {
				return false;
			}
		}
	}

	if(ensureNoLoops && !qFuzzyCompare(qAbs(angleSum), 2 * M_PI)) {
		return false;
	}

	return true;
}

namespace {
struct HorizontalMergePolicy {
	static int col(const QRect &rc) { return rc.x(); }

	static int nextCol(const QRect &rc) { return rc.x() + rc.width(); }

	static int rowHeight(const QRect &rc) { return rc.height(); }

	static bool rowIsLess(const QRect &lhs, const QRect &rhs)
	{
		return lhs.y() < rhs.y();
	}

	static bool elementIsLess(const QRect &lhs, const QRect &rhs)
	{
		return lhs.y() < rhs.y() || (lhs.y() == rhs.y() && lhs.x() < rhs.x());
	}
};

struct VerticalMergePolicy {
	static int col(const QRect &rc) { return rc.y(); }

	static int nextCol(const QRect &rc) { return rc.y() + rc.height(); }

	static int rowHeight(const QRect &rc) { return rc.width(); }

	static bool rowIsLess(const QRect &lhs, const QRect &rhs)
	{
		return lhs.x() < rhs.x();
	}

	static bool elementIsLess(const QRect &lhs, const QRect &rhs)
	{
		return lhs.x() < rhs.x() || (lhs.x() == rhs.x() && lhs.y() < rhs.y());
	}
};

template <typename MergePolicy>
QVector<QRect>::iterator mergeRects(
	QVector<QRect>::iterator beginIt, QVector<QRect>::iterator endIt,
	MergePolicy policy)
{
	if(beginIt == endIt) {
		return endIt;
	}

	std::sort(beginIt, endIt, MergePolicy::elementIsLess);

	auto resultIt = beginIt;
	auto it = std::next(beginIt);

	while(it != endIt) {
		auto rowEnd = std::upper_bound(it, endIt, *it, MergePolicy::rowIsLess);
		for(auto rowIt = it; rowIt != rowEnd; ++rowIt) {
			if(policy.rowHeight(*resultIt) == policy.rowHeight(*rowIt) &&
			   policy.nextCol(*resultIt) == policy.col(*rowIt)) {
				*resultIt |= *rowIt;
			} else {
				resultIt++;
				*resultIt = *rowIt;
			}
		}

		it = rowEnd;
	}

	return std::next(resultIt);
}
}

QVector<QRect>::iterator mergeSparseRects(
	QVector<QRect>::iterator beginIt, QVector<QRect>::iterator endIt)
{
	endIt = mergeRects(beginIt, endIt, HorizontalMergePolicy());
	endIt = mergeRects(beginIt, endIt, VerticalMergePolicy());
	return endIt;
}

}

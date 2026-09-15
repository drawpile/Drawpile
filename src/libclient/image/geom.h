/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef LIBCLIENT_IMAGE_GEOM_H
#define LIBCLIENT_IMAGE_GEOM_H
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <QtMath>

class QPolygonF;

namespace geom {

static constexpr inline int bounds(int min, int val, int max)
{
	return qMax(min, qMin(max, val));
}

static constexpr inline qreal boundsF(qreal min, qreal val, qreal max)
{
	return qMax(min, qMin(max, val));
}

static constexpr inline int intSignZeroF(qreal x)
{
	if(x == 0.0) {
		return 0;
	} else if(x > 0.0) {
		return 1;
	} else {
		return -1;
	}
}

static inline qreal square(qreal x)
{
	return x * x;
}

static inline qreal crossProduct(const QPointF &a, const QPointF &b)
{
	return a.x() * b.y() - a.y() * b.x();
}

static inline qreal normSquared(QPointF p)
{
	return square(p.x()) + square(p.y());
}

static inline qreal norm(QPointF p)
{
	return qSqrt(normSquared(p));
}

static inline qreal distance(QPointF p1, QPointF p2)
{
	return qSqrt(square(p1.x() - p2.x()) + square(p1.y() - p2.y()));
}

static inline qreal angleBetweenVectors(const QPointF &v1, const QPointF &v2)
{
	qreal a1 = qAtan2(v1.y(), v1.x());
	qreal a2 = qAtan2(v2.y(), v2.x());
	return a2 - a1;
}

static inline QRectF growRect(QRectF rect, qreal offset)
{
	return rect.adjusted(-offset, -offset, offset, offset);
}

static inline QRect blowRect(QRect rect, qreal coeff)
{
	int w = int(int(rect.width()) * coeff);
	int h = int(qreal(rect.height()) * coeff);
	return rect.adjusted(-w, -h, w, h);
}

static inline void accumulateBounds(QPointF pt, QRectF &bounds)
{
	if(bounds.isEmpty()) {
		qreal eps = 1e-10;
		bounds = QRectF(pt, QSizeF(eps, eps));
	}

	if(pt.x() < bounds.left()) {
		bounds.setLeft(pt.x());
	} else if(pt.x() > bounds.right()) {
		bounds.setRight(pt.x());
	}

	if(pt.y() < bounds.top()) {
		bounds.setTop(pt.y());
	} else if(pt.y() > bounds.bottom()) {
		bounds.setBottom(pt.y());
	}
}

static inline bool pointFuzzyCompare(QPointF a, QPointF b)
{
	return qFuzzyCompare(a.x(), b.x()) && qFuzzyCompare(a.y(), b.y());
}

static inline bool pointFuzzyEqual(QPointF a, QPointF b, qreal eps)
{
	return qAbs(a.x() - b.x()) < eps || qAbs(a.y() - b.y()) < eps;
}

bool pointVectorsFuzzyEqual(
	const QVector<QPointF> &a, const QVector<QPointF> &b, qreal eps);

bool polygonsFuzzyEqual(const QPolygonF &a, const QPolygonF &b, qreal eps);

bool isPolygonRect(const QPolygonF &poly, qreal tolerance);

bool isPolygonPixelAlignedRect(const QPolygonF &poly, qreal tolerance);

bool isPolygonTrulyConvex(const QPolygonF &polygon, bool ensureNoLoops = true);

QVector<QRect>::iterator mergeSparseRects(
	QVector<QRect>::iterator beginIt, QVector<QRect>::iterator endIt);

}

#endif

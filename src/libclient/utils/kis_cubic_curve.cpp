// Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
// Copyright (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
// Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
// SPDX-License-Identifier: GPL-2.0-or-later
extern "C" {
#include <dpcommon/curve.h>
}
#include "libclient/utils/kis_cubic_curve.h"
#include <QStringList>

KisCubicCurve::KisCubicCurve()
	: m_points({{0.0, 0.0}, {1.0, 1.0}})
{
}

KisCubicCurve::KisCubicCurve(const QList<QPointF> &points)
	: m_points(points)
{
	keepSorted();
}

KisCubicCurve::KisCubicCurve(const KisCubicCurve &other)
	: m_points(other.m_points)
	, m_curve(DP_curve_incref_nullable(other.m_curve))
{
}

KisCubicCurve::KisCubicCurve(KisCubicCurve &&other)
{
	std::swap(m_points, other.m_points);
	std::swap(m_curve, other.m_curve);
}

KisCubicCurve &KisCubicCurve::operator=(const KisCubicCurve &other)
{
	if(this != &other) {
		m_points = other.m_points;
		DP_curve_decref_nullable(m_curve);
		m_curve = DP_curve_incref_nullable(other.m_curve);
	}
	return *this;
}

KisCubicCurve &KisCubicCurve::operator=(KisCubicCurve &&other)
{
	std::swap(m_points, other.m_points);
	DP_curve_decref_nullable(m_curve);
	m_curve = other.m_curve;
	other.m_curve = nullptr;
	return *this;
}

KisCubicCurve::~KisCubicCurve()
{
	DP_curve_decref_nullable(m_curve);
}

bool KisCubicCurve::operator==(const KisCubicCurve &other) const
{
	return (m_curve && m_curve == other.m_curve) || m_points == other.m_points;
}

qreal KisCubicCurve::value(qreal x) const
{
	if(!m_curve) {
		m_curve = makeCurve(m_points);
	}
	return DP_curve_value_at(m_curve, x);
}

void KisCubicCurve::setPoints(const QList<QPointF> &points)
{
	m_points = points;
	invalidate();
}

void KisCubicCurve::setPoint(int idx, const QPointF &point)
{
	m_points[idx] = point;
	keepSorted();
	invalidate();
}

int KisCubicCurve::addPoint(const QPointF &point)
{
	m_points.append(point);
	keepSorted();
	invalidate();
	return m_points.indexOf(point);
}

void KisCubicCurve::removePoint(int idx)
{
	m_points.removeAt(idx);
	invalidate();
}

QString KisCubicCurve::toString() const
{
	QString sCurve;
	if(m_points.size() > 0) {
		for(const QPointF &pair : m_points) {
			sCurve += QString::number(pair.x());
			sCurve += ',';
			sCurve += QString::number(pair.y());
			sCurve += ';';
		}
	}
	return sCurve;
}

void KisCubicCurve::fromString(const QString &string)
{
	QStringList data = string.split(';');
	QList<QPointF> points;
	for(const QString &pair : data) {
		if(pair.indexOf(',') > -1) {
			QPointF p;
			p.rx() = qBound(0.0, pair.section(',', 0, 0).toDouble(), 1.0);
			p.ry() = qBound(0.0, pair.section(',', 1, 1).toDouble(), 1.0);
			points.append(p);
		}
	}

	if(points.size() < 2) {
		points = {{0.0, 0.0}, {1.0, 1.0}};
	}

	setPoints(points);
}

DP_Curve *KisCubicCurve::makeCurve(const QList<QPointF> &points)
{
	return DP_curve_new(
		DP_CURVE_TYPE_CUBIC, points.size(), &KisCubicCurve::getPointCallback,
		const_cast<QList<QPointF> *>(&points));
}

void KisCubicCurve::transferIntoFloat(float *out, int size) const
{
	qreal end = 1.0 / (size - 1);
	for(int i = 0; i < size; ++i) {
		out[i] = value(i * end);
	}
}

static bool pointLessThan(const QPointF &a, const QPointF &b)
{
	return a.x() < b.x();
}

void KisCubicCurve::keepSorted()
{
	std::stable_sort(m_points.begin(), m_points.end(), pointLessThan);
}

void KisCubicCurve::invalidate()
{
	if(m_curve) {
		DP_curve_decref(m_curve);
		m_curve = nullptr;
	}
}

void KisCubicCurve::getPointCallback(
	void *user, int i, double *out_x, double *out_y)
{
	const QList<QPointF> &points = *static_cast<const QList<QPointF> *>(user);
	QPointF point = points[i];
	*out_x = point.x();
	*out_y = point.y();
}

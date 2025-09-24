// Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LIBCLIENT_UTILS_KIS_CUBIC_CURVE_H
#define LIBCLIENT_UTILS_KIS_CUBIC_CURVE_H
#include <QList>
#include <QPointF>
#include <QVariant>

struct DP_Curve;

class KisCubicCurve {
public:
	KisCubicCurve();
	KisCubicCurve(const QList<QPointF> &points);
	KisCubicCurve(const KisCubicCurve &other);
	KisCubicCurve(KisCubicCurve &&other);
	KisCubicCurve &operator=(const KisCubicCurve &other);
	KisCubicCurve &operator=(KisCubicCurve &&other);
	~KisCubicCurve();

	bool operator==(const KisCubicCurve &curve) const;

	qreal value(qreal x) const;
	QList<QPointF> points() const { return m_points; }
	void setPoints(const QList<QPointF> &points);
	void setPoint(int idx, const QPointF &point);
	/**
	 * Add a point to the curve, the list of point is always sorted.
	 * @return the index of the inserted point
	 */
	int addPoint(const QPointF &point);
	void removePoint(int idx);

	void transferIntoFloat(float *out, int size) const;
	QString toString() const;
	void fromString(const QString &);

    static DP_Curve *makeCurve(const QList<QPointF> &points);

private:
	void keepSorted();
	void invalidate();

	static void
	getPointCallback(void *user, int i, double *out_x, double *out_y);

	QList<QPointF> m_points;
	mutable DP_Curve *m_curve = nullptr;
};

Q_DECLARE_METATYPE(KisCubicCurve)

#endif

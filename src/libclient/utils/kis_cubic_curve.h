// Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _KIS_CUBIC_CURVE_H_
#define _KIS_CUBIC_CURVE_H_

#include <QList>
#include <QVector>
#include <QVariant>

class QPointF;

const QString DEFAULT_CURVE_STRING = "0,0;1,1;";

/**
 * Hold the data for a cubic curve.
 */
class KisCubicCurve
{
public:
    KisCubicCurve();
    KisCubicCurve(const QList<QPointF>& points);
    KisCubicCurve(const KisCubicCurve& curve);
    ~KisCubicCurve();
    KisCubicCurve& operator=(const KisCubicCurve& curve);
    bool operator==(const KisCubicCurve& curve) const;
public:
    qreal value(qreal x) const;
    QList<QPointF> points() const;
    void setPoints(const QList<QPointF>& points);
    void setPoint(int idx, const QPointF& point);
    /**
     * Add a point to the curve, the list of point is always sorted.
     * @return the index of the inserted point
     */
    int addPoint(const QPointF& point);
    void removePoint(int idx);
public:
    const QVector<quint16> uint16Transfer(int size = 256) const;
    const QVector<qreal> floatTransfer(int size = 256) const;
public:
    QString toString() const;
    void fromString(const QString&);
private:
    struct Data;
    struct Private;
    Private* const d;
};

Q_DECLARE_METATYPE(KisCubicCurve)

#endif

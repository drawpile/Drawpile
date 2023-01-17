/*
 *  SPDX-FileCopyrightText: 2014, 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QColor>

namespace KisAlgebra2D {
template <typename Point>
Point lerp(const Point &pt1, const Point &pt2, qreal t)
{
    return pt1 + (pt2 - pt1) * t;
}
}

namespace KisPaintingTweaks {
inline QColor blendColors(const QColor &c1, const QColor &c2, qreal r1)
{
    const qreal r2 = 1.0 - r1;
    return QColor::fromRgbF(
        c1.redF() * r1 + c2.redF() * r2,
        c1.greenF() * r1 + c2.greenF() * r2,
        c1.blueF() * r1 + c2.blueF() * r2);
}
}

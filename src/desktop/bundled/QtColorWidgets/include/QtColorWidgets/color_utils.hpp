/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_UTILS_HPP
#define COLOR_UTILS_HPP

#include <QColor>
#include <QPoint>
#include <qmath.h>

#include "QtColorWidgets/colorwidgets_global.hpp"
#include "QtColorWidgets/qt_compatibility.hpp"

namespace color_widgets {

    namespace utils {


QCP_EXPORT inline qreal color_chromaF(const QColor& c)
{
    qreal max = qMax(c.redF(), qMax(c.greenF(), c.blueF()));
    qreal min = qMin(c.redF(), qMin(c.greenF(), c.blueF()));
    return max - min;
}

QCP_EXPORT inline qreal color_lumaF(const QColor& c)
{
    return 0.30 * c.redF() + 0.59 * c.greenF() + 0.11 * c.blueF();
}

QCP_EXPORT QColor color_from_lch(qt_color_type hue, qt_color_type chroma, qt_color_type luma, qt_color_type alpha = 1 );

QCP_EXPORT inline QColor rainbow_lch(qreal hue)
{
    return color_from_lch(hue,1,0.5);
}

QCP_EXPORT inline QColor rainbow_hsv(qreal hue)
{
    return QColor::fromHsvF(hue,1,1);
}

QCP_EXPORT inline qreal color_lightnessF(const QColor& c)
{
    return ( qMax(c.redF(),qMax(c.greenF(),c.blueF())) +
             qMin(c.redF(),qMin(c.greenF(),c.blueF())) ) / 2;
}

QCP_EXPORT inline qreal color_HSL_saturationF(const QColor& col)
{
    qreal c = color_chromaF(col);
    qreal l = color_lightnessF(col);
    if ( qFuzzyCompare(l+1,1) || qFuzzyCompare(l+1,2) )
        return 0;
    return c / (1-qAbs(2*l-1));
}


QCP_EXPORT QColor color_from_hsl(qt_color_type hue, qt_color_type sat, qt_color_type lig, qt_color_type alpha = 1 );

QCP_EXPORT QColor get_screen_color(const QPoint &global_pos);

} // namespace utils
} // namespace color_widgets

#endif // COLOR_UTILS_HPP

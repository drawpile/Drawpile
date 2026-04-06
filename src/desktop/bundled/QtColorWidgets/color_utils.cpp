/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "QtColorWidgets/color_utils.hpp"
#include "QtColorWidgets/oklab.hpp"

#include <QScreen>
#include <QApplication>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include<QDesktopWidget>
#endif

static constexpr double OKHSL_HUE_OFFSET = 0.0812052;

void color_widgets::utils::color_oklab_set(const QColor &c, qreal *out_hue, qreal *out_chroma, qreal *out_luma)
{

    color_widgets::Triplet result = color_widgets::oklab_to_okhsl(color_widgets::rgb_to_oklab({c.redF(), c.greenF(), c.blueF()}));
    if (out_hue) {
        *out_hue = std::fmod(result[0] + (1.0 - OKHSL_HUE_OFFSET), 1.0);
    }
    if (out_chroma) {
        *out_chroma = result[1];
    }
    if (out_luma) {
        *out_luma = result[2];
    }
}

QColor color_widgets::utils::color_from_lch(qt_color_type hue, qt_color_type chroma, qt_color_type luma, qt_color_type alpha )
{
    qreal h1 = hue*6;
    qreal x = chroma*(1-qAbs(std::fmod(h1,2)-1));
    QColor col;
    if ( h1 >= 0 && h1 < 1 )
        col = QColor::fromRgbF(chroma,x,0);
    else if ( h1 < 2 )
        col = QColor::fromRgbF(x,chroma,0);
    else if ( h1 < 3 )
        col = QColor::fromRgbF(0,chroma,x);
    else if ( h1 < 4 )
        col = QColor::fromRgbF(0,x,chroma);
    else if ( h1 < 5 )
        col = QColor::fromRgbF(x,0,chroma);
    else if ( h1 <= 6 )
        col = QColor::fromRgbF(chroma,0,x);

    qreal m = luma - color_lumaF(col);

    return QColor::fromRgbF(
        qBound(0.0,col.redF()+m,1.0),
        qBound(0.0,col.greenF()+m,1.0),
        qBound(0.0,col.blueF()+m,1.0),
        alpha);
}

QColor color_widgets::utils::color_from_oklch(qt_color_type hue, qt_color_type chroma, qt_color_type luma, qt_color_type alpha )
{
    color_widgets::Triplet result = color_widgets::oklab_to_rgb(color_widgets::okhsl_to_oklab({std::fmod(hue + OKHSL_HUE_OFFSET, 1.0), chroma, luma}));
    return QColor::fromRgbF(qBound(0.0, result[0], 1.0), qBound(0.0, result[1], 1.0), qBound(0.0, result[2], 1.0), alpha);
}

QColor color_widgets::utils::color_from_hsl(qt_color_type hue, qt_color_type sat, qt_color_type lig, qt_color_type alpha )
{
    qreal chroma = (1 - qAbs(2*lig-1))*sat;
    qreal h1 = hue*6;
    qreal x = chroma*(1-qAbs(std::fmod(h1,2)-1));
    QColor col;
    if ( h1 >= 0 && h1 < 1 )
        col = QColor::fromRgbF(chroma,x,0);
    else if ( h1 < 2 )
        col = QColor::fromRgbF(x,chroma,0);
    else if ( h1 < 3 )
        col = QColor::fromRgbF(0,chroma,x);
    else if ( h1 < 4 )
        col = QColor::fromRgbF(0,x,chroma);
    else if ( h1 < 5 )
        col = QColor::fromRgbF(x,0,chroma);
    else if ( h1 <= 6 )
        col = QColor::fromRgbF(chroma,0,x);

    qreal m = lig-chroma/2;

    return QColor::fromRgbF(
        qBound(0.0,col.redF()+m,1.0),
        qBound(0.0,col.greenF()+m,1.0),
        qBound(0.0,col.blueF()+m,1.0),
        alpha);
}


QColor color_widgets::utils::get_screen_color(const QPoint &global_pos)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    QScreen *screen = QApplication::screenAt(global_pos);
    // Drawpile patch: make sure we don't have a null screen.
    if ( !screen )
        return QColor();
#else
    int screenNum = QApplication::desktop()->screenNumber(global_pos);
    QScreen *screen = QApplication::screens().at(screenNum);
#endif


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    WId wid = QApplication::desktop()->winId();
    QPoint screen_pos = global_pos;
#else
    int wid = 0;
    QPoint screen_pos = global_pos - screen->geometry().topLeft();
#endif

    QImage img = screen->grabWindow(wid, screen_pos.x(), screen_pos.y(), 1, 1).toImage();

    return img.pixel(0,0);
}

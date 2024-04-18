/*
 * SPDX-FileCopyrightText: 2022-2022 Jeremy Borgman
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef QT_COMPATIBILITY_HPP
#define QT_COMPATIBILITY_HPP

#include <QWidget>
/**
 * \brief pos is deprecated in Qt6.
 */
template <typename T>
QPointF pos_wrap(T* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->pos();
#endif
}

// Qt6 change the type of QColor from qreal to float
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
typedef float qt_color_type;
#else
typedef qreal qt_color_type;
#endif


#endif // QT_COMPATIBILITY_HPP

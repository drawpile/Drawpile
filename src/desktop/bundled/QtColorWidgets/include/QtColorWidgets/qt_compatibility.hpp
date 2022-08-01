/**
 * \file qt_compatibility.hpp
 *
 * \author Jeremy Borgman
 *
 * \copyright Copyright (C) 2022-2022 Jeremy Borgman
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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

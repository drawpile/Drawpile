/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_WIDGETS_COLOR_NAMES_HPP
#define COLOR_WIDGETS_COLOR_NAMES_HPP

#include <QColor>
#include <QString>

#include <QtColorWidgets/colorwidgets_global.hpp>

namespace color_widgets {

/**
 * \brief Convert a string into a color
 *
 * Supported string formats:
 *  * Short hex strings #f00
 *  * Long hex strings  #ff0000
 *  * Color names       red
 *  * Function-like     rgb(255,0,0)
 *
 * Additional string formats supported only when \p alpha is true:
 *  * Long hex strings  #ff0000ff
 *  * Function like     rgba(255,0,0,255)
 */
QCP_EXPORT QColor colorFromString(const QString& string, bool alpha = true);

/**
 * \brief Convert a color into a string
 *
 * Format:
 *  * If the color has full alpha: #ff0000
 *  * If alpha is true and the color has non-full alpha: #ff000088
 */
QCP_EXPORT QString stringFromColor(const QColor& color, bool alpha = true);

} // namespace color_widgets
#endif // COLOR_WIDGETS_COLOR_NAMES_HPP

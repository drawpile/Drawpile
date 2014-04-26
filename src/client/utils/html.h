/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
#ifndef DP_UTILS_HTML_H
#define DP_UTILS_HTML_H

#include <QString>

namespace htmlutils {

/**
 * @brief Convert newlines to br:s
 * @param input
 * @return
 */
QString newlineToBr(const QString &input);

/**
 * @brief Take an input string and wrap all links in <a> tags
 *
 * @param input
 * @return
 */
QString linkify(const QString &input);

}

#endif

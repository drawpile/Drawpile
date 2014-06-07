/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ICON_H
#define ICON_H

#include <QIcon>

namespace icon {

/**
 * @brief Get an icon from the system theme, falling back to the bundled icon set
 * @param name
 * @return
 */
inline QIcon fromTheme(const QString &name)
{
	return QIcon::fromTheme(name, QIcon(QLatin1Literal("icons:") + name));
}

}

#endif


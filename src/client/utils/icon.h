/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

//! Check if a dark theme icon should be used on a background of this color
bool isDark(const QColor &color);

//! Select whether to use the dark or light theme based on current palette
void selectThemeVariant();

//! Did selectThemeVariant pick the dark theme?
bool isDarkThemeSelected();

//! Get an icon from the system theme, falling back to the bundled icon set
inline QIcon fromTheme(const QString &name) {
	// Note: after we no longer support anything older thant Qt 5.11,
	// we can just use QIcon::fromTheme(name) and set a fallback search path
	return QIcon::fromTheme(name, QIcon(QStringLiteral("theme:") + name + QStringLiteral(".svg")));
}

}

#endif


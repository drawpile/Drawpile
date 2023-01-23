/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "libclient/utils/icon.h"
#include "libshared/util/paths.h"

#include <QDir>
#include <QPalette>

namespace icon {

static bool is_dark_theme = false;

bool isDark(const QColor &c)
{
	const qreal luminance = c.redF() * 0.216 + c.greenF() * 0.7152 + c.blueF() * 0.0722;

	return luminance <= 0.5;
}

bool isDarkThemeSelected() { return is_dark_theme; }

void selectThemeVariant()
{
	is_dark_theme = isDark(QPalette().color(QPalette::Window));

	const QString themePath = is_dark_theme ? QStringLiteral("/theme/dark") : QStringLiteral("/theme/light");

	QStringList themePaths;
	for(const QString &path : utils::paths::dataPaths()) {
		themePaths.append(path + themePath);
	}

#if 0
	// We can use this after we no longer support anything older than Qt 5.11
	// The nice thing about fallback search path is that the icons are automagically
	// reloaded when it changes.
	// Note: On Windows and Mac, we need to explicitly select a theme too. (A dummy theme should do.)
	QIcon::setFallbackSearchPaths(themePaths);
#endif

	QDir::setSearchPaths("theme", themePaths);
}

}


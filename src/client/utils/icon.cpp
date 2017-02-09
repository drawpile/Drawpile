/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "icon.h"
#include "utils/settings.h"

#include <QDir>
#include <QPalette>
#include <QStandardPaths>

namespace icon {

bool isDark(const QColor &c)
{
	const qreal luminance = c.redF() * 0.216 + c.greenF() * 0.7152 + c.redF() * 0.0722;

	return luminance <= 0.5;
}

void selectThemeVariant()
{
	QStringList builtinPaths;
	QString themePath;

	if(isDark(QPalette().color(QPalette::Window))) {
		themePath = QStringLiteral("/theme/dark");
		builtinPaths << QStringLiteral(":/icons/dark");
	} else {
		themePath = QStringLiteral("/theme/light");
		builtinPaths << QStringLiteral(":/icons/light");
	}

	builtinPaths << QStringLiteral(":/icons");

	QStringList themePaths;
	for(const QString &path : utils::settings::dataPaths()) {
		themePaths << path + themePath;
	}

	QDir::setSearchPaths("theme", themePaths);
	QDir::setSearchPaths("builtin", builtinPaths);

}

}


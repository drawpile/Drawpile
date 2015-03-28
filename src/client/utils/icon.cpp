/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include <QPalette>

namespace icon {

static QString LIGHT_THEME_ROOT = QStringLiteral("icons:light/");
static QString DARK_THEME_ROOT = QStringLiteral("icons:dark/");
static Theme THEME_VARIANT = LIGHT;

void selectThemeVariant()
{
	QColor bg = QPalette().color(QPalette::Window);

	if(bg.valueF() > 0.5)
		THEME_VARIANT = LIGHT;
	else
		THEME_VARIANT = DARK;
}

QIcon fromTheme(const QString &name, Theme variant)
{

	if(variant==CURRENT || variant==THEME_VARIANT)
		return QIcon::fromTheme(name, QIcon((THEME_VARIANT == LIGHT ? LIGHT_THEME_ROOT : DARK_THEME_ROOT) + name + QStringLiteral(".svg")));

	// Because QIcon::fromTheme doesn't support theme variants,
	// we use bundled icons if current theme is not the selected variant
	return QIcon((variant == LIGHT ? LIGHT_THEME_ROOT : DARK_THEME_ROOT) + name + QStringLiteral(".svg"));
}

}

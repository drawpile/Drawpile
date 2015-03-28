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
#include "main.h"

#include <QDir>
#include <QPalette>
#include <QStandardPaths>

namespace icon {

static Theme THEME_VARIANT = LIGHT;

bool isLightColor(const QColor &c)
{
	return c.valueF() > 0.5;
}

void selectThemeVariant()
{
	const QString lightpath = QStringLiteral("/theme/light");
	const QString darkpath = QStringLiteral("/theme/dark");
	QString curpath;

	QStringList builtinPaths;

	if(isLightColor(QPalette().color(QPalette::Window))) {
		THEME_VARIANT = LIGHT;
		curpath = lightpath;
		builtinPaths << QStringLiteral(":/icons/light");

	} else {
		THEME_VARIANT = DARK;
		curpath = darkpath;
		builtinPaths << QStringLiteral(":/icons/dark");
	}

	builtinPaths << QStringLiteral(":/icons");


	QStringList themePaths, lightPaths, darkPaths;
	for(const QString &path : DrawpileApp::dataPaths()) {
		themePaths << path + curpath;
		lightPaths << path + lightpath;
		darkPaths << path + darkpath;
	}

	QDir::setSearchPaths("theme", themePaths);
	QDir::setSearchPaths("themelight", lightPaths);
	QDir::setSearchPaths("themedark", darkPaths);
	QDir::setSearchPaths("builtin", builtinPaths);

}

QIcon fromTheme(const QString &name, Theme variant)
{

	if(variant==CURRENT || variant==THEME_VARIANT)
		return QIcon::fromTheme(name, QIcon(QStringLiteral("theme:") + name + QStringLiteral(".svg")));

	// Because QIcon::fromTheme doesn't support theme variants,
	// we use bundled icons if current theme is not the selected variant
	return QIcon((variant == LIGHT ? QStringLiteral("themelight:") : QStringLiteral("themedark:")) + name + QStringLiteral(".svg"));
}

}

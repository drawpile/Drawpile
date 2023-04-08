// SPDX-License-Identifier: GPL-3.0-or-later

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


// SPDX-License-Identifier: GPL-3.0-or-later

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


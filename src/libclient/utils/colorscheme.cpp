/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "colorscheme.h"

#include <QPalette>
#include <QPair>
#include <QSettings>

#include <array>

namespace colorscheme {

static std::array<QPair<QPalette::ColorGroup, QString>, 3> GROUPS {{
	{QPalette::Active, "Active"},
	{QPalette::Disabled, "Disabled"},
	{QPalette::Inactive, "Inactive"}
}};

static std::array<QPair<QPalette::ColorRole, QString>, 14> ROLES {{
	{QPalette::Window, "Window"},
	{QPalette::WindowText, "WindowText"},
	{QPalette::Base, "Base"},
	{QPalette::AlternateBase, "AlternateBase"},
	{QPalette::ToolTipBase, "ToolTipBase"},
	{QPalette::ToolTipText, "ToolTipText"},
	{QPalette::Text, "Text"},
	{QPalette::Button, "Button"},
	{QPalette::ButtonText, "ButtonText"},
	{QPalette::BrightText, "BrightText"},
	{QPalette::Highlight, "Highlight"},
	{QPalette::HighlightedText, "HighlightedText"},
	{QPalette::Light, "Light"},
	{QPalette::Dark, "Dark"},
}};

QPalette loadFromFile(const QString &filename)
{
	QPalette palette;

	QSettings settings(filename, QSettings::IniFormat);

	for(const auto &group : GROUPS) {
		settings.beginGroup(group.second);
		for(const auto &role : ROLES) {
			palette.setColor(group.first, role.first, QColor(settings.value(role.second).toString()));
		}
		settings.endGroup();
	}
	return palette;
}

bool saveToFile(const QString &filename, const QPalette &palette)
{
	QSettings settings(filename, QSettings::IniFormat);

	for(const auto &group : GROUPS) {
		settings.beginGroup(group.second);
		for(const auto &role : ROLES) {
			settings.setValue(role.second, palette.color(group.first, role.first).name());
		}
		settings.endGroup();
	}
	return false;
}

}


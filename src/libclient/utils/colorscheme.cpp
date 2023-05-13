// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/colorscheme.h"

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

static std::array<QPair<QPalette::ColorRole, QString>, 20> ROLES {{
	{QPalette::AlternateBase, "AlternateBase"},
	{QPalette::Base, "Base"},
	{QPalette::BrightText, "BrightText"},
	{QPalette::Button, "Button"},
	{QPalette::ButtonText, "ButtonText"},
	{QPalette::Dark, "Dark"},
	{QPalette::Highlight, "Highlight"},
	{QPalette::HighlightedText, "HighlightedText"},
	{QPalette::Light, "Light"},
	{QPalette::Link, "Link"},
	{QPalette::LinkVisited, "LinkVisited"},
	{QPalette::Mid, "Mid"},
	{QPalette::Midlight, "Midlight"},
	{QPalette::PlaceholderText, "PlaceholderText"},
	{QPalette::Shadow, "Shadow"},
	{QPalette::Text, "Text"},
	{QPalette::ToolTipBase, "ToolTipBase"},
	{QPalette::ToolTipText, "ToolTipText"},
	{QPalette::Window, "Window"},
	{QPalette::WindowText, "WindowText"}
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


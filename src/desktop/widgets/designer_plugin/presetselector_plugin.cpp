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

#include <QtPlugin>

#include "desktop/widgets/presetselector.h"
#include "desktop/widgets/designer_plugin/presetselector_plugin.h"

PresetSelectorPlugin::PresetSelectorPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void PresetSelectorPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool PresetSelectorPlugin::isInitialized() const
{
	return initialized;
}

QWidget *PresetSelectorPlugin::createWidget(QWidget *parent)
{
	return new widgets::PresetSelector(parent);
}

QString PresetSelectorPlugin::name() const
{
	return "widgets::PresetSelector";
}

QString PresetSelectorPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon PresetSelectorPlugin::icon() const
{
	return QIcon();
}

QString PresetSelectorPlugin::toolTip() const
{
	return "A box for selecting and saving presets";
}

QString PresetSelectorPlugin::whatsThis() const
{
	return "";
}

bool PresetSelectorPlugin::isContainer() const
{
	return false;
}

QString PresetSelectorPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"PresetSelector\">\n"
		"<widget class=\"widgets::PresetSelector\" name=\"presetSelector\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>400</width>\n"
		"   <height>20</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString PresetSelectorPlugin::includeFile() const
{
	return "widgets/presetselector.h";
}


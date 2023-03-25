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

#include <QtPlugin>

#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/designer_plugin/groupedtoolbutton_plugin.h"

GroupedToolButtonPlugin::GroupedToolButtonPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void GroupedToolButtonPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool GroupedToolButtonPlugin::isInitialized() const
{
	return initialized;
}

QWidget *GroupedToolButtonPlugin::createWidget(QWidget *parent)
{
	return new widgets::GroupedToolButton(parent);
}

QString GroupedToolButtonPlugin::name() const
{
	return "widgets::GroupedToolButton";
}

QString GroupedToolButtonPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon GroupedToolButtonPlugin::icon() const
{
	return QIcon();
}

QString GroupedToolButtonPlugin::toolTip() const
{
	return "A thin tool button which can be grouped with another and look like a solid bar";
}

QString GroupedToolButtonPlugin::whatsThis() const
{
	return "";
}

bool GroupedToolButtonPlugin::isContainer() const
{
	return false;
}

QString GroupedToolButtonPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"GroupedToolButton\">\n"
		"<widget class=\"widgets::GroupedToolButton\" name=\"groupedToolButton\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>24</width>\n"
		"   <height>24</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString GroupedToolButtonPlugin::includeFile() const
{
	return "widgets/groupedtoolbutton.h";
}


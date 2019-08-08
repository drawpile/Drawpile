/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#include "../tablettest.h"
#include "tablettester_plugin.h"

TabletTesterPlugin::TabletTesterPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void TabletTesterPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool TabletTesterPlugin::isInitialized() const
{
	return initialized;
}

QWidget *TabletTesterPlugin::createWidget(QWidget *parent)
{
	return new widgets::TabletTester(parent);
}

QString TabletTesterPlugin::name() const
{
	return "widgets::TabletTester";
}

QString TabletTesterPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon TabletTesterPlugin::icon() const
{
	return QIcon();
}

QString TabletTesterPlugin::toolTip() const
{
	return "A widget for testing tablet stylus events";
}

QString TabletTesterPlugin::whatsThis() const
{
	return "";
}

bool TabletTesterPlugin::isContainer() const
{
	return false;
}

QString TabletTesterPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"TabletTester\">\n"
		"<widget class=\"widgets::TabletTester\" name=\"resizer\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>300</width>\n"
		"   <height>300</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString TabletTesterPlugin::includeFile() const
{
	return "widgets/tablettest.h";
}


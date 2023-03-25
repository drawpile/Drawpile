/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "desktop/widgets/modifierkeys.h"
#include "desktop/widgets/designer_plugin/modifierkeys_plugin.h"

ModifierKeysPlugin::ModifierKeysPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ModifierKeysPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ModifierKeysPlugin::isInitialized() const
{
    return initialized;
}

QWidget *ModifierKeysPlugin::createWidget(QWidget *parent)
{
    return new widgets::ModifierKeys(parent);
}

QString ModifierKeysPlugin::name() const
{
    return "widgets::ModifierKeys";
}

QString ModifierKeysPlugin::group() const
{
    return "Drawpile Widgets";
}

QIcon ModifierKeysPlugin::icon() const
{
    return QIcon();
}

QString ModifierKeysPlugin::toolTip() const
{
    return "A widget for previewing brushes";
}

QString ModifierKeysPlugin::whatsThis() const
{
    return "";
}

bool ModifierKeysPlugin::isContainer() const
{
    return false;
}

QString ModifierKeysPlugin::domXml() const
{
    return 
		"<ui language=\"c++\" displayname=\"ModifierKeys\">\n"
		"<widget class=\"widgets::ModifierKeys\" name=\"modifierKeys\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>256</width>\n"
		"   <height>32</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>\n";
}

QString ModifierKeysPlugin::includeFile() const
{
    return "widgets/modifierkeys.h";
}


/*
   DrawPile - a collaborative drawing program.

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

#include "../colorbutton.h"
#include "colorbutton_plugin.h"

ColorButtonPlugin::ColorButtonPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ColorButtonPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ColorButtonPlugin::isInitialized() const
{
	return initialized;
}

QWidget *ColorButtonPlugin::createWidget(QWidget *parent)
{
	return new ColorButton(parent, Qt::black);
}

QString ColorButtonPlugin::name() const
{
	return "ColorButton";
}

QString ColorButtonPlugin::group() const
{
	return "DrawPile Widgets";
}

QIcon ColorButtonPlugin::icon() const
{
	return QIcon();
}

QString ColorButtonPlugin::toolTip() const
{
	return "A button for selecting colors";
}

QString ColorButtonPlugin::whatsThis() const
{
	return "";
}

bool ColorButtonPlugin::isContainer() const
{
	return false;
}

QString ColorButtonPlugin::domXml() const
{
	return "<ui language=\"c++\">\n"
		"<widget class=\"ColorButton\" name=\"colorButton\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>64</width>\n"
		"   <height>16</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString ColorButtonPlugin::includeFile() const
{
	return "widgets/colorbutton.h";
}


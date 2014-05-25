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

#include "widgets/dualcolorbutton.h"
#include "dualcolorbutton_plugin.h"

#include <QtPlugin>

DualColorButtonPlugin::DualColorButtonPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void DualColorButtonPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool DualColorButtonPlugin::isInitialized() const
{
	return initialized;
}

QWidget *DualColorButtonPlugin::createWidget(QWidget *parent)
{
	return new DualColorButton(parent);
}

QString DualColorButtonPlugin::name() const
{
	return "DualColorButton";
}

QString DualColorButtonPlugin::group() const
{
	return "DrawPile Widgets";
}

QIcon DualColorButtonPlugin::icon() const
{
	return QIcon();
}

QString DualColorButtonPlugin::toolTip() const
{
	return "A button to select foreground and background colors";
}

QString DualColorButtonPlugin::whatsThis() const
{
	return "";
}

bool DualColorButtonPlugin::isContainer() const
{
	return false;
}

QString DualColorButtonPlugin::domXml() const
{
	return "<ui language=\"c++\">\n"
		   "<widget class=\"DualColorButton\" name=\"dualColorButton\">\n"
		   " <property name=\"geometry\">\n"
		   "  <rect>\n"
		   "   <x>0</x>\n"
		   "   <y>0</y>\n"
		   "   <width>128</width>\n"
		   "   <height>16</height>\n"
		   "  </rect>\n"
		   " </property>\n"
		   "</widget>\n"
		   "</ui>\n";
}

QString DualColorButtonPlugin::includeFile() const
{
	return "widgets/dualcolorbutton.h";
}


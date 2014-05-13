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

#include "../gradientslider.h"
#include "plugin.h"

#include <QtPlugin>

GradientSliderPlugin::GradientSliderPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void GradientSliderPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool GradientSliderPlugin::isInitialized() const
{
	return initialized;
}

QWidget *GradientSliderPlugin::createWidget(QWidget *parent)
{
	return new GradientSlider(parent);
}

QString GradientSliderPlugin::name() const
{
	return "GradientSlider";
}

QString GradientSliderPlugin::group() const
{
	return "DrawPile Widgets";
}

QIcon GradientSliderPlugin::icon() const
{
	return QIcon(":gradientslider.png");
}

QString GradientSliderPlugin::toolTip() const
{
	return "A slider widget that displays a color gradient";
}

QString GradientSliderPlugin::whatsThis() const
{
	return "";
}

bool GradientSliderPlugin::isContainer() const
{
	return false;
}

QString GradientSliderPlugin::domXml() const
{
	return
		"<ui language=\"c++\">\n"
		"<widget class=\"GradientSlider\" name=\"gradientSlider\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>128</width>\n"
		"   <height>16</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>\n"
		;
}

QString GradientSliderPlugin::includeFile() const
{
	return "widgets/gradientslider.h";
}


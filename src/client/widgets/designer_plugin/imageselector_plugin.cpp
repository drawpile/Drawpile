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

#include "../imageselector.h"
#include "imageselector_plugin.h"

ImageSelectorPlugin::ImageSelectorPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ImageSelectorPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ImageSelectorPlugin::isInitialized() const
{
    return initialized;
}

QWidget *ImageSelectorPlugin::createWidget(QWidget *parent)
{
    return new ImageSelector(parent);
}

QString ImageSelectorPlugin::name() const
{
    return "ImageSelector";
}

QString ImageSelectorPlugin::group() const
{
    return "Drawpile Widgets";
}

QIcon ImageSelectorPlugin::icon() const
{
    return QIcon();
}

QString ImageSelectorPlugin::toolTip() const
{
    return "Image selecting preview widget";
}

QString ImageSelectorPlugin::whatsThis() const
{
    return "";
}

bool ImageSelectorPlugin::isContainer() const
{
    return false;
}

QString ImageSelectorPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
			"<widget class=\"ImageSelector\" name=\"imageSelector\">\n"
			" <property name=\"geometry\">\n"
			"  <rect>\n"
			"   <x>0</x>\n"
			"   <y>0</y>\n"
			"   <width>128</width>\n"
			"   <height>128</height>\n"
			"  </rect>\n"
			" </property>\n"
			"</widget>\n"
			"</ui>\n";
}

QString ImageSelectorPlugin::includeFile() const
{
    return "widgets/imageselector.h";
}


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

#include "desktop/widgets/brushpreview.h"
#include "desktop/widgets/designer_plugin/brushpreview_plugin.h"

BrushPreviewPlugin::BrushPreviewPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void BrushPreviewPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool BrushPreviewPlugin::isInitialized() const
{
    return initialized;
}

QWidget *BrushPreviewPlugin::createWidget(QWidget *parent)
{
    return new widgets::BrushPreview(parent);
}

QString BrushPreviewPlugin::name() const
{
    return "widgets::BrushPreview";
}

QString BrushPreviewPlugin::group() const
{
    return "Drawpile Widgets";
}

QIcon BrushPreviewPlugin::icon() const
{
    return QIcon();
}

QString BrushPreviewPlugin::toolTip() const
{
    return "A widget for previewing brushes";
}

QString BrushPreviewPlugin::whatsThis() const
{
    return "";
}

bool BrushPreviewPlugin::isContainer() const
{
    return false;
}

QString BrushPreviewPlugin::domXml() const
{
    return 
		"<ui language=\"c++\" displayname=\"BrushPreview\">\n"
		"<widget class=\"widgets::BrushPreview\" name=\"brushPreview\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>128</width>\n"
		"   <height>64</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>\n";
}

QString BrushPreviewPlugin::includeFile() const
{
    return "widgets/brushpreview.h";
}


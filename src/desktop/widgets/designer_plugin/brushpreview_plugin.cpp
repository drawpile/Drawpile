// SPDX-License-Identifier: GPL-3.0-or-later

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


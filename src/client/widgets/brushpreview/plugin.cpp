#include <QtPlugin>

#include "../../brushpreview.h"
#include "plugin.h"

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
    return new BrushPreview(parent);
}

QString BrushPreviewPlugin::name() const
{
    return "BrushPreview";
}

QString BrushPreviewPlugin::group() const
{
    return "DrawPile Widgets";
}

QIcon BrushPreviewPlugin::icon() const
{
    return QIcon(":brushpreview.png");
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
    return "<widget class=\"BrushPreview\" name=\"brushPreview\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>128</width>\n"
           "   <height>64</height>\n"
           "  </rect>\n"
           " </property>\n"
           "</widget>\n";
}

QString BrushPreviewPlugin::includeFile() const
{
    return "brushpreview.h";
}

Q_EXPORT_PLUGIN2(brushpreviewlugin, BrushPreviewPlugin)


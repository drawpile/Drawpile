#include <QtPlugin>

#include "../../colortriangle.h"
#include "plugin.h"

ColorTrianglePlugin::ColorTrianglePlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ColorTrianglePlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ColorTrianglePlugin::isInitialized() const
{
    return initialized;
}

QWidget *ColorTrianglePlugin::createWidget(QWidget *parent)
{
    return new ColorTriangle(parent);
}

QString ColorTrianglePlugin::name() const
{
    return "ColorTriangle";
}

QString ColorTrianglePlugin::group() const
{
    return "DrawPile Widgets";
}

QIcon ColorTrianglePlugin::icon() const
{
    return QIcon(":colortriangle.png");
}

QString ColorTrianglePlugin::toolTip() const
{
    return "A HSV color picker widget";
}

QString ColorTrianglePlugin::whatsThis() const
{
    return "";
}

bool ColorTrianglePlugin::isContainer() const
{
    return false;
}

QString ColorTrianglePlugin::domXml() const
{
    return "<widget class=\"ColorTriangle\" name=\"colorTriangle\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>128</width>\n"
           "   <height>128</height>\n"
           "  </rect>\n"
           " </property>\n"
           "</widget>\n";
}

QString ColorTrianglePlugin::includeFile() const
{
    return "colortriangle.h";
}

Q_EXPORT_PLUGIN2(colortriangleplugin, ColorTrianglePlugin)


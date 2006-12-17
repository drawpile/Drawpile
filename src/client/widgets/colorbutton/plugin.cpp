#include "../../colorbutton.h"
#include "plugin.h"

#include <QtPlugin>

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
	return QIcon(":colorbutton.png");
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
	return "<widget class=\"ColorButton\" name=\"colorButton\">\n"
		   " <property name=\"geometry\">\n"
		   "  <rect>\n"
		   "   <x>0</x>\n"
		   "   <y>0</y>\n"
		   "   <width>64</width>\n"
		   "   <height>16</height>\n"
		   "  </rect>\n"
		   " </property>\n"
		   "</widget>\n";
}

QString ColorButtonPlugin::includeFile() const
{
	return "colorbutton.h";
}

Q_EXPORT_PLUGIN2(colorbuttonplugin, ColorButtonPlugin)

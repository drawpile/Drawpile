#include "../../dualcolorbutton.h"
#include "plugin.h"

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
	return QIcon(":dualcolorbutton.png");
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
	return "<widget class=\"DualColorButton\" name=\"dualColorButton\">\n"
		   " <property name=\"geometry\">\n"
		   "  <rect>\n"
		   "   <x>0</x>\n"
		   "   <y>0</y>\n"
		   "   <width>128</width>\n"
		   "   <height>16</height>\n"
		   "  </rect>\n"
		   " </property>\n"
		   "</widget>\n";
}

QString DualColorButtonPlugin::includeFile() const
{
	return "dualcolorbutton.h";
}

Q_EXPORT_PLUGIN2(dualcolorbuttonplugin, DualColorButtonPlugin)

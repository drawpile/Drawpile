#include "../../brushslider.h"
#include "plugin.h"

#include <QtPlugin>

BrushSliderPlugin::BrushSliderPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void BrushSliderPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool BrushSliderPlugin::isInitialized() const
{
	return initialized;
}

QWidget *BrushSliderPlugin::createWidget(QWidget *parent)
{
	return new BrushSlider(parent);
}

QString BrushSliderPlugin::name() const
{
	return "BrushSlider";
}

QString BrushSliderPlugin::group() const
{
	return "DrawPile Widgets";
}

QIcon BrushSliderPlugin::icon() const
{
	return QIcon(":brushslider.png");
}

QString BrushSliderPlugin::toolTip() const
{
	return "A slider widget for changing brush parameters";
}

QString BrushSliderPlugin::whatsThis() const
{
	return "";
}

bool BrushSliderPlugin::isContainer() const
{
	return false;
}

QString BrushSliderPlugin::domXml() const
{
	return "<widget class=\"BrushSlider\" name=\"brushSlider\">\n"
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

QString BrushSliderPlugin::includeFile() const
{
	return "brushslider.h";
}

Q_EXPORT_PLUGIN2(brushsliderplugin, BrushSliderPlugin)

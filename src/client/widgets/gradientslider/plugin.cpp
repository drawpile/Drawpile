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


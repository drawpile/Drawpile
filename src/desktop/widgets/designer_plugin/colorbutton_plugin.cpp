// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/colorbutton.h"
#include "desktop/widgets/designer_plugin/colorbutton_plugin.h"

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
	return new widgets::ColorButton(parent, Qt::black);
}

QString ColorButtonPlugin::name() const
{
	return "widgets::ColorButton";
}

QString ColorButtonPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon ColorButtonPlugin::icon() const
{
	return QIcon();
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
	return "<ui language=\"c++\" displayname=\"ColorButton\">\n"
		"<widget class=\"widgets::ColorButton\" name=\"colorButton\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>64</width>\n"
		"   <height>16</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString ColorButtonPlugin::includeFile() const
{
	return "widgets/colorbutton.h";
}


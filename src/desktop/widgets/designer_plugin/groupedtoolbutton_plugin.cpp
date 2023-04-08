// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/designer_plugin/groupedtoolbutton_plugin.h"

GroupedToolButtonPlugin::GroupedToolButtonPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void GroupedToolButtonPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool GroupedToolButtonPlugin::isInitialized() const
{
	return initialized;
}

QWidget *GroupedToolButtonPlugin::createWidget(QWidget *parent)
{
	return new widgets::GroupedToolButton(parent);
}

QString GroupedToolButtonPlugin::name() const
{
	return "widgets::GroupedToolButton";
}

QString GroupedToolButtonPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon GroupedToolButtonPlugin::icon() const
{
	return QIcon();
}

QString GroupedToolButtonPlugin::toolTip() const
{
	return "A thin tool button which can be grouped with another and look like a solid bar";
}

QString GroupedToolButtonPlugin::whatsThis() const
{
	return "";
}

bool GroupedToolButtonPlugin::isContainer() const
{
	return false;
}

QString GroupedToolButtonPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"GroupedToolButton\">\n"
		"<widget class=\"widgets::GroupedToolButton\" name=\"groupedToolButton\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>24</width>\n"
		"   <height>24</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString GroupedToolButtonPlugin::includeFile() const
{
	return "widgets/groupedtoolbutton.h";
}


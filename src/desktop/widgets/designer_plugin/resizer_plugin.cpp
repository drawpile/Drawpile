// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/resizerwidget.h"
#include "desktop/widgets/designer_plugin/resizer_plugin.h"

ResizerPlugin::ResizerPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ResizerPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ResizerPlugin::isInitialized() const
{
	return initialized;
}

QWidget *ResizerPlugin::createWidget(QWidget *parent)
{
	return new widgets::ResizerWidget(parent);
}

QString ResizerPlugin::name() const
{
	return "widgets::ResizerWidget";
}

QString ResizerPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon ResizerPlugin::icon() const
{
	return QIcon();
}

QString ResizerPlugin::toolTip() const
{
	return "A widget for positioning the canvas after a resize";
}

QString ResizerPlugin::whatsThis() const
{
	return "";
}

bool ResizerPlugin::isContainer() const
{
	return false;
}

QString ResizerPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"ResizerWidget\">\n"
		"<widget class=\"widgets::ResizerWidget\" name=\"resizer\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>300</width>\n"
		"   <height>200</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString ResizerPlugin::includeFile() const
{
	return "widgets/resizerwidget.h";
}


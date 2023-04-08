// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/modifierkeys.h"
#include "desktop/widgets/designer_plugin/modifierkeys_plugin.h"

ModifierKeysPlugin::ModifierKeysPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ModifierKeysPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ModifierKeysPlugin::isInitialized() const
{
    return initialized;
}

QWidget *ModifierKeysPlugin::createWidget(QWidget *parent)
{
    return new widgets::ModifierKeys(parent);
}

QString ModifierKeysPlugin::name() const
{
    return "widgets::ModifierKeys";
}

QString ModifierKeysPlugin::group() const
{
    return "Drawpile Widgets";
}

QIcon ModifierKeysPlugin::icon() const
{
    return QIcon();
}

QString ModifierKeysPlugin::toolTip() const
{
    return "A widget for previewing brushes";
}

QString ModifierKeysPlugin::whatsThis() const
{
    return "";
}

bool ModifierKeysPlugin::isContainer() const
{
    return false;
}

QString ModifierKeysPlugin::domXml() const
{
    return
		"<ui language=\"c++\" displayname=\"ModifierKeys\">\n"
		"<widget class=\"widgets::ModifierKeys\" name=\"modifierKeys\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>256</width>\n"
		"   <height>32</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>\n";
}

QString ModifierKeysPlugin::includeFile() const
{
    return "widgets/modifierkeys.h";
}


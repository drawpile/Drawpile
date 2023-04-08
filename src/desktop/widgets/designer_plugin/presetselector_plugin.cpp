// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/presetselector.h"
#include "desktop/widgets/designer_plugin/presetselector_plugin.h"

PresetSelectorPlugin::PresetSelectorPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void PresetSelectorPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool PresetSelectorPlugin::isInitialized() const
{
	return initialized;
}

QWidget *PresetSelectorPlugin::createWidget(QWidget *parent)
{
	return new widgets::PresetSelector(parent);
}

QString PresetSelectorPlugin::name() const
{
	return "widgets::PresetSelector";
}

QString PresetSelectorPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon PresetSelectorPlugin::icon() const
{
	return QIcon();
}

QString PresetSelectorPlugin::toolTip() const
{
	return "A box for selecting and saving presets";
}

QString PresetSelectorPlugin::whatsThis() const
{
	return "";
}

bool PresetSelectorPlugin::isContainer() const
{
	return false;
}

QString PresetSelectorPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"PresetSelector\">\n"
		"<widget class=\"widgets::PresetSelector\" name=\"presetSelector\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>400</width>\n"
		"   <height>20</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString PresetSelectorPlugin::includeFile() const
{
	return "widgets/presetselector.h";
}


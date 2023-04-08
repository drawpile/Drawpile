// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtPlugin>

#include "desktop/widgets/filmstrip.h"
#include "desktop/widgets/designer_plugin/filmstrip_plugin.h"

FilmstripPlugin::FilmstripPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void FilmstripPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool FilmstripPlugin::isInitialized() const
{
	return initialized;
}

QWidget *FilmstripPlugin::createWidget(QWidget *parent)
{
	return new widgets::Filmstrip(parent);
}

QString FilmstripPlugin::name() const
{
	return "widgets::Filmstrip";
}

QString FilmstripPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon FilmstripPlugin::icon() const
{
	return QIcon();
}

QString FilmstripPlugin::toolTip() const
{
	return "A widget for indicating position in a recording";
}

QString FilmstripPlugin::whatsThis() const
{
	return "";
}

bool FilmstripPlugin::isContainer() const
{
	return false;
}

QString FilmstripPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"Filmstrip\">\n"
		"<widget class=\"widgets::Filmstrip\" name=\"filmStrip\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>400</width>\n"
		"   <height>100</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString FilmstripPlugin::includeFile() const
{
	return "widgets/filmstrip.h";
}


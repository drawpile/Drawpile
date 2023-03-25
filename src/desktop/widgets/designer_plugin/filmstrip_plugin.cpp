/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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


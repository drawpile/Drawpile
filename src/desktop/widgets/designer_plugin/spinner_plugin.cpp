/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "desktop/widgets/spinner.h"
#include "desktop/widgets/designer_plugin/spinner_plugin.h"

SpinnerPlugin::SpinnerPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void SpinnerPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool SpinnerPlugin::isInitialized() const
{
	return initialized;
}

QWidget *SpinnerPlugin::createWidget(QWidget *parent)
{
	return new widgets::Spinner(parent);
}

QString SpinnerPlugin::name() const
{
	return "widgets::Spinner";
}

QString SpinnerPlugin::group() const
{
	return "Drawpile Widgets";
}

QIcon SpinnerPlugin::icon() const
{
	return QIcon();
}

QString SpinnerPlugin::toolTip() const
{
	return "A loading spinner";
}

QString SpinnerPlugin::whatsThis() const
{
	return "";
}

bool SpinnerPlugin::isContainer() const
{
	return false;
}

QString SpinnerPlugin::domXml() const
{
	return "<ui language=\"c++\" displayname=\"Spinner\">\n"
		"<widget class=\"widgets::Spinner\" name=\"loadingSpinner\">\n"
		" <property name=\"geometry\">\n"
		"  <rect>\n"
		"   <x>0</x>\n"
		"   <y>0</y>\n"
		"   <width>64</width>\n"
		"   <height>64</height>\n"
		"  </rect>\n"
		" </property>\n"
		"</widget>\n"
		"</ui>";
}

QString SpinnerPlugin::includeFile() const
{
	return "widgets/spinner.h";
}


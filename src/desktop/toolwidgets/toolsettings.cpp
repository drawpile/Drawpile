/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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

#include "desktop/toolwidgets/toolsettings.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

QWidget *ToolSettings::createUi(QWidget *parent)
{
	Q_ASSERT(m_widget==0);
	m_widget = createUiWidget(parent);
	return m_widget;
}

void ToolSettings::pushSettings()
{
	// Default implementation has no settings
}

ToolProperties ToolSettings::saveToolSettings()
{
	return ToolProperties();
}

void ToolSettings::restoreToolSettings(const ToolProperties &)
{
}

}


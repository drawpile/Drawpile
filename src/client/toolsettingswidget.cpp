/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "toolsettingswidget.h"
#include "toolsettings.h"

namespace widgets {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent)
{
	// Create settings widget for brush
	brushsettings_ = new tools::BrushSettings;
	brush_ = brushsettings_->createUi(this);
	currenttool_ = brushsettings_;
	currentwidget_ = brush_;

	// Create settings widget for eraser
	erasersettings_ = new tools::BrushSettings;
	eraser_ = erasersettings_->createUi(this);
	eraser_->hide();

	setTool(tools::BRUSH);
}

ToolSettings::~ToolSettings()
{
	delete brushsettings_;
	delete erasersettings_;
}

void ToolSettings::setTool(tools::Type tool) {
	currentwidget_->hide();
	switch(tool) {
		case tools::BRUSH:
			setWindowTitle(tr("Brush"));
			currentwidget_ = brush_;
			currenttool_ = brushsettings_;
			break;
		case tools::ERASER:
			setWindowTitle(tr("Eraser"));
			currentwidget_ = eraser_;
			currenttool_ = erasersettings_;
			break;
	}
	currentwidget_->show();
	setWidget(currentwidget_);
}

}


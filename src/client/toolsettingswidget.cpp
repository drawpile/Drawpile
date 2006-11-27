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
	brushsettings_ = new tools::BrushSettings("brush", tr("Brush"));
	brushsettings_->createUi(this);
	currenttool_ = brushsettings_;

	// Create settings widget for eraser
	erasersettings_ = new tools::BrushSettings("brush", tr("Eraser"),true);
	erasersettings_->createUi(this);

	setTool(tools::BRUSH);
}

ToolSettings::~ToolSettings()
{
	delete brushsettings_;
	delete erasersettings_;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	currenttool_->getUi()->hide();
	switch(tool) {
		case tools::BRUSH: currenttool_ = brushsettings_; break;
		case tools::ERASER: currenttool_ = erasersettings_; break;
	}
	setWindowTitle(currenttool_->getTitle());
	currenttool_->getUi()->show();
	setWidget(currenttool_->getUi());
}

/**
 * Get a brush with settings from the currently visible widget
 * @param foreground foreground color
 * @param background background color
 * @return brush
 */
drawingboard::Brush ToolSettings::getBrush(const QColor& foreground,
				const QColor& background) const
{
	return currenttool_->getBrush(foreground, background);
}

}


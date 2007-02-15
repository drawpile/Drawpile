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

#include <QStackedWidget>

#include "toolsettingswidget.h"
#include "toolsettings.h"

namespace widgets {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent)
{
	// Create a widget stack
	widgets_ = new QStackedWidget(this);
	setWidget(widgets_);

	// Create settings widget for brush
	brushsettings_ = new tools::BrushSettings("brush", tr("Brush"));
	widgets_->addWidget(brushsettings_->createUi(this));
	currenttool_ = brushsettings_;

	// Create settings widget for eraser
	erasersettings_ = new tools::BrushSettings("eraser", tr("Eraser"),true);
	widgets_->addWidget(erasersettings_->createUi(this));

	// Create a settings widget for color picker
	pickersettings_ = new tools::NoSettings("picker", tr("Color picker"));
	widgets_->addWidget(pickersettings_->createUi(this));

	// Create settings widget for line
	linesettings_ = new tools::SimpleSettings("line", tr("Line"), tools::SimpleSettings::Line);
	widgets_->addWidget(linesettings_->createUi(this));

	// Create settings widget for line
	rectsettings_ = new tools::SimpleSettings("rectangle", tr("Rectangle"), tools::SimpleSettings::Rectangle);
	widgets_->addWidget(rectsettings_->createUi(this));


}

ToolSettings::~ToolSettings()
{
	delete brushsettings_;
	delete erasersettings_;
	delete pickersettings_;
	delete linesettings_;
	delete rectsettings_;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	switch(tool) {
		case tools::BRUSH: currenttool_ = brushsettings_; break;
		case tools::ERASER: currenttool_ = erasersettings_; break;
		case tools::PICKER: currenttool_ = pickersettings_; break;
		case tools::LINE: currenttool_ = linesettings_; break;
		case tools::RECTANGLE: currenttool_ = rectsettings_; break;
	}
	setWindowTitle(currenttool_->getTitle());
	widgets_->setCurrentWidget(currenttool_->getUi());
	currenttool_->setForeground(fgcolor_);
	currenttool_->setBackground(bgcolor_);
	emit sizeChanged(currenttool_->getSize());

	if(currenttool_ == erasersettings_) // eraser is a special case
		emit colorsChanged(bgcolor_, fgcolor_);
	else
		emit colorsChanged(fgcolor_, bgcolor_);
}

void ToolSettings::setForeground(const QColor& color)
{
	fgcolor_ = color;
	currenttool_->setForeground(color);
	if(currenttool_ == erasersettings_) // eraser is a special case
		emit colorsChanged(bgcolor_, fgcolor_);
	else
		emit colorsChanged(fgcolor_, bgcolor_);
}

void ToolSettings::setBackground(const QColor& color)
{
	bgcolor_ = color;
	currenttool_->setBackground(color);
	if(currenttool_ == erasersettings_) // eraser is a special case
		emit colorsChanged(bgcolor_, fgcolor_);
	else
		emit colorsChanged(fgcolor_, bgcolor_);
}

/**
 * Get a brush with settings from the currently visible widget
 * @return brush
 */
const drawingboard::Brush& ToolSettings::getBrush() const
{
	return currenttool_->getBrush();
}

}


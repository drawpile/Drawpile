/*
	DrawPile - a collaborative drawing program.

	Copyright (C) 2006-2014 Calle Laakkonen

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

#include "toolsettingsdock.h"
#include "toolsettings.h"

namespace docks {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent)
{
	// Create a widget stack
	widgets_ = new QStackedWidget(this);
	setWidget(widgets_);

	pensettings_ = new tools::PenSettings("pen", tr("Pen"));
	widgets_->addWidget(pensettings_->createUi(this));

	brushsettings_ = new tools::BrushSettings("brush", tr("Brush"));
	widgets_->addWidget(brushsettings_->createUi(this));
	currenttool_ = brushsettings_;

	erasersettings_ = new tools::EraserSettings("eraser", tr("Eraser"));
	widgets_->addWidget(erasersettings_->createUi(this));

	_pickersettings = new tools::ColorPickerSettings("picker", tr("Color picker"));
	widgets_->addWidget(_pickersettings->createUi(this));

	linesettings_ = new tools::SimpleSettings("line", tr("Line"), tools::SimpleSettings::Line, true);
	widgets_->addWidget(linesettings_->createUi(this));

	rectsettings_ = new tools::SimpleSettings("rectangle", tr("Rectangle"), tools::SimpleSettings::Rectangle, false);
	widgets_->addWidget(rectsettings_->createUi(this));

	_ellipsesettings = new tools::SimpleSettings("ellipse", tr("Ellipse"), tools::SimpleSettings::Ellipse, true);
	widgets_->addWidget(_ellipsesettings->createUi(this));

	_textsettings = new tools::AnnotationSettings("annotation", tr("Annotation"));
	widgets_->addWidget(_textsettings->createUi(this));

	selectionsettings_ = new tools::SelectionSettings("selection", tr("Selection"));
	widgets_->addWidget(selectionsettings_->createUi(this));

	_lasersettings = new tools::LaserPointerSettings("laser", tr("Laser pointer"));
	widgets_->addWidget(_lasersettings->createUi(this));
}

ToolSettings::~ToolSettings()
{
	delete pensettings_,
	delete brushsettings_,
	delete erasersettings_,
	delete _pickersettings,
	delete linesettings_,
	delete rectsettings_;
	delete _ellipsesettings;
	delete _textsettings;
	delete selectionsettings_;
	delete _lasersettings;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	switch(tool) {
		case tools::PEN: currenttool_ = pensettings_; break;
		case tools::BRUSH: currenttool_ = brushsettings_; break;
		case tools::ERASER: currenttool_ = erasersettings_; break;
		case tools::PICKER: currenttool_ = _pickersettings; break;
		case tools::LINE: currenttool_ = linesettings_; break;
		case tools::RECTANGLE: currenttool_ = rectsettings_; break;
		case tools::ELLIPSE: currenttool_ = _ellipsesettings; break;
		case tools::ANNOTATION: currenttool_ = _textsettings; break;
		case tools::SELECTION: currenttool_ = selectionsettings_; break;
		case tools::LASERPOINTER: currenttool_ = _lasersettings; break;
	}

	// Deselect annotation on tool change
	if(tool != tools::ANNOTATION) {
		int a = getAnnotationSettings()->selected();
		if(a)
			getAnnotationSettings()->setSelection(0);
	}

	setWindowTitle(currenttool_->getTitle());
	widgets_->setCurrentWidget(currenttool_->getUi());
	currenttool_->setForeground(fgcolor_);
	currenttool_->setBackground(bgcolor_);
	emit sizeChanged(currenttool_->getSize());
}

void ToolSettings::setForeground(const QColor& color)
{
	fgcolor_ = color;
	currenttool_->setForeground(color);
}

void ToolSettings::setBackground(const QColor& color)
{
	bgcolor_ = color;
	currenttool_->setBackground(color);
}

/**
 * Get a brush with settings from the currently visible widget
 * @return brush
 */
const paintcore::Brush& ToolSettings::getBrush(bool swapcolors) const
{
	return currenttool_->getBrush(swapcolors);
}

}


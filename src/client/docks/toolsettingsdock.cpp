/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include "docks/toolsettingsdock.h"
#include "tools/toolsettings.h"
#include "widgets/dualcolorbutton.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <Color_Dialog>

namespace docks {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent)
{
	QWidget *w = new QWidget(this);
	setWidget(w);

	auto *layout = new QVBoxLayout(w);

	// Create a widget stack
	_widgets = new QStackedWidget(this);
	layout->addWidget(_widgets, 1);

	_pensettings = new tools::PenSettings("pen", tr("Pen"));
	_widgets->addWidget(_pensettings->createUi(this));

	_brushsettings = new tools::BrushSettings("brush", tr("Brush"));
	_widgets->addWidget(_brushsettings->createUi(this));
	_currenttool = _brushsettings;

	_erasersettings = new tools::EraserSettings("eraser", tr("Eraser"));
	_widgets->addWidget(_erasersettings->createUi(this));

	_pickersettings = new tools::ColorPickerSettings("picker", tr("Color picker"));
	_widgets->addWidget(_pickersettings->createUi(this));

	_linesettings = new tools::SimpleSettings("line", tr("Line"), tools::SimpleSettings::Line, true);
	_widgets->addWidget(_linesettings->createUi(this));

	_rectsettings = new tools::SimpleSettings("rectangle", tr("Rectangle"), tools::SimpleSettings::Rectangle, false);
	_widgets->addWidget(_rectsettings->createUi(this));

	_ellipsesettings = new tools::SimpleSettings("ellipse", tr("Ellipse"), tools::SimpleSettings::Ellipse, true);
	_widgets->addWidget(_ellipsesettings->createUi(this));

	_textsettings = new tools::AnnotationSettings("annotation", tr("Annotation"));
	_widgets->addWidget(_textsettings->createUi(this));

	_selectionsettings = new tools::SelectionSettings("selection", tr("Selection"));
	_widgets->addWidget(_selectionsettings->createUi(this));

	_lasersettings = new tools::LaserPointerSettings("laser", tr("Laser pointer"));
	_widgets->addWidget(_lasersettings->createUi(this));

	// Create foreground/background color changing widget
	QFrame *separator = new QFrame(w);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	layout->addWidget(separator);

	auto *hlayout = new QHBoxLayout;
	hlayout->setMargin(3);
	layout->addLayout(hlayout);

	_fgbgcolor = new widgets::DualColorButton(w);
	_fgbgcolor->setMinimumSize(48,48);
	hlayout->addWidget(_fgbgcolor);
	hlayout->addSpacerItem(new QSpacerItem(10, 5, QSizePolicy::Expanding));

	connect(_fgbgcolor, &widgets::DualColorButton::foregroundChanged, [this](const QColor &c){
		_currenttool->setForeground(c);
	});
	connect(_fgbgcolor, &widgets::DualColorButton::backgroundChanged, [this](const QColor &c){
		_currenttool->setBackground(c);
	});
	connect(_fgbgcolor, SIGNAL(foregroundChanged(QColor)), this, SIGNAL(foregroundColorChanged(QColor)));
	connect(_fgbgcolor, SIGNAL(backgroundChanged(QColor)), this, SIGNAL(backgroundColorChanged(QColor)));

	connect(_pickersettings, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	// Create color changer dialogs
	auto dlg_fgcolor = new Color_Dialog(this);
	dlg_fgcolor->setAlphaEnabled(false);
	dlg_fgcolor->setWindowTitle(tr("Foreground color"));
	connect(dlg_fgcolor, SIGNAL(colorSelected(QColor)), this, SLOT(setForegroundColor(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundClicked(QColor)), dlg_fgcolor, SLOT(showColor(QColor)));

	auto dlg_bgcolor = new Color_Dialog(this);
	dlg_bgcolor->setWindowTitle(tr("Background color"));
	dlg_bgcolor->setAlphaEnabled(false);
	connect(dlg_bgcolor, SIGNAL(colorSelected(QColor)), this, SLOT(setBackgroundColor(QColor)));
	connect(_fgbgcolor, SIGNAL(backgroundClicked(QColor)), dlg_bgcolor, SLOT(showColor(QColor)));
}

ToolSettings::~ToolSettings()
{
	delete _pensettings,
	delete _brushsettings,
	delete _erasersettings,
	delete _pickersettings,
	delete _linesettings,
	delete _rectsettings;
	delete _ellipsesettings;
	delete _textsettings;
	delete _selectionsettings;
	delete _lasersettings;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	switch(tool) {
		case tools::PEN: _currenttool = _pensettings; break;
		case tools::BRUSH: _currenttool = _brushsettings; break;
		case tools::ERASER: _currenttool = _erasersettings; break;
		case tools::PICKER: _currenttool = _pickersettings; break;
		case tools::LINE: _currenttool = _linesettings; break;
		case tools::RECTANGLE: _currenttool = _rectsettings; break;
		case tools::ELLIPSE: _currenttool = _ellipsesettings; break;
		case tools::ANNOTATION: _currenttool = _textsettings; break;
		case tools::SELECTION: _currenttool = _selectionsettings; break;
		case tools::LASERPOINTER: _currenttool = _lasersettings; break;
	}

	// Deselect annotation on tool change
	if(tool != tools::ANNOTATION) {
		int a = getAnnotationSettings()->selected();
		if(a)
			getAnnotationSettings()->setSelection(0);
	}

	setWindowTitle(_currenttool->getTitle());
	_widgets->setCurrentWidget(_currenttool->getUi());
	_currenttool->setForeground(foregroundColor());
	_currenttool->setBackground(backgroundColor());
	emit sizeChanged(_currenttool->getSize());
}

QColor ToolSettings::foregroundColor() const
{
	return _fgbgcolor->foreground();
}

void ToolSettings::setForegroundColor(const QColor& color)
{
	_fgbgcolor->setForeground(color);
}

QColor ToolSettings::backgroundColor() const
{
	return _fgbgcolor->background();
}

void ToolSettings::setBackgroundColor(const QColor& color)
{
	_fgbgcolor->setBackground(color);
}

void ToolSettings::swapForegroundBackground()
{
	_fgbgcolor->swapColors();
}

void ToolSettings::quickAdjustCurrent1(float adjustment)
{
	_currenttool->quickAdjust1(adjustment);
}

/**
 * Get a brush with settings from the currently visible widget
 * @return brush
 */
const paintcore::Brush& ToolSettings::getBrush(bool swapcolors) const
{
	return _currenttool->getBrush(swapcolors);
}

}


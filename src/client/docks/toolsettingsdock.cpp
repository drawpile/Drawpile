/*
   Drawpile - a collaborative drawing program.

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
#include "docks/utils.h"
#include "tools/toolsettings.h"
#include "widgets/toolslotbutton.h"
#include "utils/icon.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QSettings>

#include <Color_Dialog>

namespace docks {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent), _currentQuickslot(0), _eraserOverride(0)
{
	// Initialize tool slots
	_toolprops.reserve(QUICK_SLOTS);
	for(int i=0;i<QUICK_SLOTS;++i)
		_toolprops.append(tools::ToolsetProperties());

	setStyleSheet(defaultDockStylesheet());

	// Initialize UI
	QWidget *w = new QWidget(this);
	setWidget(w);

	auto *layout = new QVBoxLayout(w);
	layout->setMargin(0);
	layout->setSpacing(0);

	// Create quick toolchange slot buttons
	auto *hlayout = new QHBoxLayout;
	hlayout->setContentsMargins(3, 3, 3, 0);
	hlayout->setSpacing(0);
	layout->addLayout(hlayout);

	QButtonGroup *quickbuttons = new QButtonGroup(this);
	quickbuttons->setExclusive(true);

	for(int i=0;i<QUICK_SLOTS;++i) {
		auto *b = new widgets::ToolSlotButton(w);

		connect(b, SIGNAL(doubleClicked()), this, SLOT(changeForegroundColor()));

		b->setCheckable(true);
		b->setText(QString::number(i+1));
		b->setMinimumSize(40, 40);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		b->setAutoRaise(true);

		hlayout->addWidget(b);
		quickbuttons->addButton(b, i);
		_quickslot[i] = b;
	}

	connect(quickbuttons, SIGNAL(buttonClicked(int)), this, SLOT(setToolSlot(int)));

	// Create a widget stack
	_widgets = new QStackedWidget(this);
	layout->addWidget(_widgets, 1);

	_pensettings = new tools::PenSettings("pen", tr("Pen"));
	_widgets->addWidget(_pensettings->createUi(this));

	_brushsettings = new tools::BrushSettings("brush", tr("Brush"));
	_widgets->addWidget(_brushsettings->createUi(this));
	_currenttool = _brushsettings;

	_smudgesettings = new tools::SmudgeSettings("smudge", tr("Watercolor"));
	_widgets->addWidget(_smudgesettings->createUi(this));

	_erasersettings = new tools::EraserSettings("eraser", tr("Eraser"));
	_widgets->addWidget(_erasersettings->createUi(this));

	_pickersettings = new tools::ColorPickerSettings("picker", tr("Color Picker"));
	_widgets->addWidget(_pickersettings->createUi(this));

	_linesettings = new tools::SimpleSettings("line", tr("Line"), icon::fromTheme("draw-line"), tools::SimpleSettings::Line, true);
	_widgets->addWidget(_linesettings->createUi(this));

	_rectsettings = new tools::SimpleSettings("rectangle", tr("Rectangle"), icon::fromTheme("draw-rectangle"), tools::SimpleSettings::Rectangle, false);
	_widgets->addWidget(_rectsettings->createUi(this));

	_ellipsesettings = new tools::SimpleSettings("ellipse", tr("Ellipse"), icon::fromTheme("draw-ellipse"), tools::SimpleSettings::Ellipse, true);
	_widgets->addWidget(_ellipsesettings->createUi(this));

	_fillsettings = new tools::FillSettings("fill", tr("Flood Fill"));
	_widgets->addWidget(_fillsettings->createUi(this));

	_textsettings = new tools::AnnotationSettings("annotation", tr("Annotation"));
	_widgets->addWidget(_textsettings->createUi(this));

	_selectionsettings = new tools::SelectionSettings("selection", tr("Selection (Rectangular)"), false);
	_widgets->addWidget(_selectionsettings->createUi(this));

	_polyselectionsettings = new tools::SelectionSettings("polygonselection", tr("Selection (Free-Form)"), true);
	_widgets->addWidget(_polyselectionsettings->createUi(this));

	_lasersettings = new tools::LaserPointerSettings("laser", tr("Laser pointer"));
	_widgets->addWidget(_lasersettings->createUi(this));

	connect(_pickersettings, SIGNAL(colorSelected(QColor)), this, SLOT(setForegroundColor(QColor)));

	// Create color changer dialogs
	_fgdialog = new Color_Dialog(this);
	_fgdialog->setAlphaEnabled(false);
	_fgdialog->setWindowTitle(tr("Foreground Color"));
	connect(_fgdialog, SIGNAL(colorSelected(QColor)), this, SLOT(setForegroundColor(QColor)));

	_bgdialog = new Color_Dialog(this);
	_bgdialog->setAlphaEnabled(false);
	_bgdialog->setWindowTitle(tr("Background Color"));
	connect(_bgdialog, SIGNAL(colorSelected(QColor)), this, SLOT(setBackgroundColor(QColor)));
}

ToolSettings::~ToolSettings()
{
	delete _pensettings;
	delete _brushsettings;
	delete _smudgesettings;
	delete _erasersettings;
	delete _pickersettings;
	delete _linesettings;
	delete _rectsettings;
	delete _ellipsesettings;
	delete _fillsettings;
	delete _textsettings;
	delete _selectionsettings;
	delete _polyselectionsettings;
	delete _lasersettings;
}

void ToolSettings::readSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");

	int quickslot = qBound(0, cfg.value("slot", 0).toInt(), QUICK_SLOTS-1);

	_toolprops.clear();

	for(int i=0;i<QUICK_SLOTS;++i) {
		cfg.beginGroup(QString("slot-%1").arg(i));
		_toolprops << tools::ToolsetProperties::load(cfg);
		cfg.endGroup();
		updateToolSlot(i, true);
	}

	selectToolSlot(quickslot);
}

void ToolSettings::saveSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");

	cfg.setValue("slot", _currentQuickslot);

	saveCurrentTool();

	for(int i=0;i<_toolprops.size();++i) {
		cfg.beginGroup(QString("slot-%1").arg(i));
		_toolprops[i].save(cfg);
		cfg.endGroup();
	}
}

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Type tool)
{
	switch(tool) {
	case tools::PEN: return _pensettings;
	case tools::BRUSH: return _brushsettings;
	case tools::SMUDGE: return _smudgesettings;
	case tools::ERASER: return _erasersettings;
	case tools::LINE: return _linesettings;
	case tools::RECTANGLE: return _rectsettings;
	case tools::ELLIPSE: return _ellipsesettings;
	case tools::FLOODFILL: return _fillsettings;
	case tools::ANNOTATION: return _textsettings;
	case tools::PICKER: return _pickersettings;
	case tools::LASERPOINTER: return _lasersettings;
	case tools::SELECTION: return _selectionsettings;
	case tools::POLYGONSELECTION: return _polyselectionsettings;
	}

	qFatal("Unhandled tools::Type %d", tool);
	return nullptr;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	// Save old tool settings, then switch to the new tool
	_previousTool = currentTool();
	saveCurrentTool();
	selectTool(tool);
}

void ToolSettings::setPreviousTool()
{
	saveCurrentTool();
	selectTool(_previousTool);
}

void ToolSettings::selectTool(tools::Type tool)
{
	tools::ToolSettings *ts = getToolSettingsPage(tool);
	if(!ts) {
		qWarning("selectTool: invalid tool %d", tool);
		return;
	}

	_currenttool = ts;

	// Deselect annotation on tool change
	if(tool != tools::ANNOTATION) {
		int a = _textsettings->selected();
		if(a)
			_textsettings->setSelection(0);
	}

	setWindowTitle(QStringLiteral("%1. %2").arg(currentToolSlot()+1).arg(_currenttool->getTitle()));
	_widgets->setCurrentWidget(_currenttool->getUi());
	_currenttool->setForeground(foregroundColor());
	_currenttool->setBackground(backgroundColor());
	_currenttool->restoreToolSettings(_toolprops[currentToolSlot()].tool(_currenttool->getName()));
	_toolprops[_currentQuickslot].setCurrentTool(tool);

	updateToolSlot(currentToolSlot(), true);
	emit toolChanged(tool);
	emit sizeChanged(_currenttool->getSize());
	updateSubpixelMode();
}

void ToolSettings::updateSubpixelMode()
{
	emit subpixelModeChanged(_currenttool->getSubpixelMode());
}

tools::Type ToolSettings::currentTool() const
{
	return tools::Type(_toolprops[_currentQuickslot].currentTool());
}

void ToolSettings::setToolSlot(int i)
{
	Q_ASSERT(i>=0 && i<QUICK_SLOTS);
	// Save old tool state, then switch to new slot (and tool)
	_previousToolSlot = _currentQuickslot;
	saveCurrentTool();
	selectToolSlot(i);
}

void ToolSettings::setPreviousToolSlot()
{
	saveCurrentTool();
	selectToolSlot(_previousToolSlot);
}

void ToolSettings::selectToolSlot(int i)
{
	_quickslot[i]->setChecked(true);
	_currentQuickslot = i;

	setForegroundColor(_toolprops[i].foregroundColor());
	setBackgroundColor(_toolprops[i].backgroundColor());

	selectTool(tools::Type(_toolprops[i].currentTool()));
}

int ToolSettings::currentToolSlot() const
{
	return _currentQuickslot;
}

QColor ToolSettings::foregroundColor() const
{
	return _foreground;
}

void ToolSettings::setForegroundColor(const QColor& color)
{
	if(color != _foreground) {
		_foreground = color;

		_currenttool->setForeground(color);
		_toolprops[_currentQuickslot].setForegroundColor(color);
		updateToolSlot(_currentQuickslot, false);

		if(_fgdialog->isVisible())
			_fgdialog->setColor(color);

		emit foregroundColorChanged(color);
	}
}

QColor ToolSettings::backgroundColor() const
{
	return _background;
}

void ToolSettings::setBackgroundColor(const QColor& color)
{
	if(color != _background) {
		_background = color;

		_currenttool->setBackground(color);
		_toolprops[_currentQuickslot].setBackgroundColor(color);
		updateToolSlot(_currentQuickslot, false);

		if(_bgdialog->isVisible())
			_bgdialog->setColor(color);

		emit backgroundColorChanged(color);
	}
}

void ToolSettings::changeForegroundColor()
{
	_fgdialog->showColor(_foreground);
}


void ToolSettings::changeBackgroundColor()
{
	_bgdialog->showColor(_background);
}

void ToolSettings::swapForegroundBackground()
{
	QColor oldForeground = _foreground;
	_foreground = _background;
	_background = oldForeground;

	_currenttool->setForeground(_foreground);
	_currenttool->setBackground(_background);
	_toolprops[_currentQuickslot].setForegroundColor(_foreground);
	_toolprops[_currentQuickslot].setBackgroundColor(_background);
	updateToolSlot(_currentQuickslot, false);

	emit foregroundColorChanged(_foreground);
	emit backgroundColorChanged(_background);
}

void ToolSettings::quickAdjustCurrent1(float adjustment)
{
	_currenttool->quickAdjust1(adjustment);
}

/**
 * Get a brush with settings from the currently visible widget
 * @return brush
 */
paintcore::Brush ToolSettings::getBrush(bool swapcolors) const
{
	return _currenttool->getBrush(swapcolors);
}

/**
 * @brief Update the tool slot button to match the stored tool settings
 * @param i
 */
void ToolSettings::updateToolSlot(int i, bool typeChanged)
{
	int tool = _toolprops[i].currentTool();
	tools::ToolSettings *ts = getToolSettingsPage(tools::Type(tool));
	if(!ts)
		ts = getToolSettingsPage(tools::PEN);


	_quickslot[i]->setColors(_toolprops[i].foregroundColor(), _toolprops[i].backgroundColor());

	if(typeChanged) {
		_quickslot[i]->setIcon(ts->getIcon());
		_quickslot[i]->setToolTip(QStringLiteral("#%1: %2").arg(i+1).arg(ts->getTitle()));
	}
}

void ToolSettings::saveCurrentTool()
{
	Q_ASSERT(_toolprops.size() > currentToolSlot());
	tools::ToolProperties tp = _currenttool->saveToolSettings();
	_toolprops[currentToolSlot()].setTool(_currenttool->getName(), tp);
}

void ToolSettings::eraserNear(bool near)
{
	if(near) {
		_eraserOverride = currentTool();
		setTool(tools::ERASER);
	} else {
		setTool(tools::Type(_eraserOverride));
	}
}

void ToolSettings::disableEraserOverride(tools::Type tool)
{
	if(_eraserOverride == tool)
		_eraserOverride = tools::BRUSH; // select some tool that can't be disabled
}

}

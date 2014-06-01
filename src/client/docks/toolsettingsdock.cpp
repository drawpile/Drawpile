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
#include "utils/icon.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QToolButton>
#include <QButtonGroup>
#include <QSettings>

#include <Color_Dialog>

namespace docks {

ToolSettings::ToolSettings(QWidget *parent)
	: QDockWidget(parent), _currentQuickslot(0)
{
	// Initialize tool slots
	_toolprops.reserve(QUICK_SLOTS);
	for(int i=0;i<QUICK_SLOTS;++i)
		_toolprops.append(tools::ToolsetProperties());

	// Initialize UI
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

	_linesettings = new tools::SimpleSettings("line", tr("Line"), icon::fromTheme("draw-line"), tools::SimpleSettings::Line, true);
	_widgets->addWidget(_linesettings->createUi(this));

	_rectsettings = new tools::SimpleSettings("rectangle", tr("Rectangle"), icon::fromTheme("draw-rectangle"), tools::SimpleSettings::Rectangle, false);
	_widgets->addWidget(_rectsettings->createUi(this));

	_ellipsesettings = new tools::SimpleSettings("ellipse", tr("Ellipse"), icon::fromTheme("draw-ellipse"), tools::SimpleSettings::Ellipse, true);
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
		_toolprops[_currentQuickslot].setForegroundColor(c);
		emit foregroundColorChanged(c);
	});
	connect(_fgbgcolor, &widgets::DualColorButton::backgroundChanged, [this](const QColor &c){
		_currenttool->setBackground(c);
		_toolprops[_currentQuickslot].setBackgroundColor(c);
		emit backgroundColorChanged(c);
	});

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

	// Create quick toolchange slot buttons
	QButtonGroup *quickbuttons = new QButtonGroup(this);
	quickbuttons->setExclusive(true);
	for(int i=0;i<QUICK_SLOTS;++i) {
		QToolButton *b = new QToolButton(w);

		b->setCheckable(true);
		b->setText(QString::number(i+1));
		//b->setMinimumSize(32, 32);
		b->setIconSize(QSize(22, 22));
		b->setAutoRaise(true);

		hlayout->addWidget(b);
		quickbuttons->addButton(b, i);
		_quickslot[i] = b;
	}

	connect(quickbuttons, SIGNAL(buttonClicked(int)), this, SLOT(setToolSlot(int)));
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
		updateToolSlot(i);
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
		case tools::PEN: return _pensettings; break;
		case tools::BRUSH: return _brushsettings; break;
		case tools::ERASER: return _erasersettings; break;
		case tools::PICKER: return _pickersettings; break;
		case tools::LINE: return _linesettings; break;
		case tools::RECTANGLE: return _rectsettings; break;
		case tools::ELLIPSE: return _ellipsesettings; break;
		case tools::ANNOTATION: return _textsettings; break;
		case tools::SELECTION: return _selectionsettings; break;
		case tools::LASERPOINTER: return _lasersettings; break;
	}

	return nullptr;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Type tool) {
	// Save old tool settings, then switch to the new tool
	saveCurrentTool();
	selectTool(tool);
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

	setWindowTitle(_currenttool->getTitle());
	_widgets->setCurrentWidget(_currenttool->getUi());
	_currenttool->setForeground(foregroundColor());
	_currenttool->setBackground(backgroundColor());
	_currenttool->restoreToolSettings(_toolprops[currentToolSlot()].tool(_currenttool->getName()));
	_toolprops[_currentQuickslot].setCurrentTool(tool);

	updateToolSlot(currentToolSlot());
	emit toolChanged(tool);
	emit sizeChanged(_currenttool->getSize());
}

tools::Type ToolSettings::currentTool() const
{
	return tools::Type(_toolprops[_currentQuickslot].currentTool());
}

void ToolSettings::setToolSlot(int i)
{
	Q_ASSERT(i>=0 && i<QUICK_SLOTS);
	// Save old tool state, then switch to new slot (and tool)
	saveCurrentTool();
	selectToolSlot(i);
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

/**
 * @brief Update the tool slot button to match the stored tool settings
 * @param i
 */
void ToolSettings::updateToolSlot(int i)
{
	int tool = _toolprops[i].currentTool();
	tools::ToolSettings *ts = getToolSettingsPage(tools::Type(tool));
	if(!ts)
		ts = getToolSettingsPage(tools::PEN);

	_quickslot[i]->setIcon(ts->getIcon());
	_quickslot[i]->setToolTip(QString("%1: %2").arg(i+1).arg(ts->getTitle()));
}

void ToolSettings::saveCurrentTool()
{
	Q_ASSERT(_toolprops.size() > currentToolSlot());
	tools::ToolProperties tp = _currenttool->saveToolSettings();
	_toolprops[currentToolSlot()].setTool(_currenttool->getName(), tp);
}

}


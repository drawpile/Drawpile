/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#include "tools/toolcontroller.h"
#include "widgets/toolslotbutton.h"
#include "utils/icon.h"

#include "toolwidgets/brushsettings.h"
#include "toolwidgets/shapetoolsettings.h"
#include "toolwidgets/colorpickersettings.h"
#include "toolwidgets/selectionsettings.h"
#include "toolwidgets/annotationsettings.h"
#include "toolwidgets/fillsettings.h"
#include "toolwidgets/lasersettings.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QButtonGroup>
#include <QSettings>

#include <ColorDialog>

namespace docks {

ToolSettings::ToolSettings(tools::ToolController *ctrl, QWidget *parent)
	: QDockWidget(parent), m_settingspage{},
	m_ctrl(ctrl), _currentQuickslot(0), _eraserOverride(0), _eraserActive(false)
{
	Q_ASSERT(ctrl);

	// Initialize tool slots
	_toolprops.reserve(QUICK_SLOTS);
	for(int i=0;i<QUICK_SLOTS;++i)
		_toolprops.append(tools::ToolsetProperties());

	setStyleSheet(defaultDockStylesheet());

	// Create quick toolchange slot buttons
	// We use the buttons as the title bar for this dock.
	// The widget has special event handling so it can be used
	// to drag the dock like a normal titlebar
	QWidget *tb = new QWidget(this);
	auto *hlayout = new QHBoxLayout(tb);
	hlayout->setContentsMargins(3, 3, 3, 0);
	hlayout->setSpacing(0);
	setTitleBarWidget(tb);

	QButtonGroup *quickbuttons = new QButtonGroup(this);
	quickbuttons->setExclusive(true);

	for(int i=0;i<QUICK_SLOTS;++i) {
		auto *b = new widgets::ToolSlotButton(tb);

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
	m_widgets = new QStackedWidget(this);
	setWidget(m_widgets);

	addPage(tools::Tool::PEN, new tools::PenSettings("pen", tr("Pen"), m_ctrl));
	addPage(tools::Tool::BRUSH, new tools::BrushSettings("brush", tr("Brush"), m_ctrl));
	addPage(tools::Tool::SMUDGE, new tools::SmudgeSettings("smudge", tr("Watercolor"), m_ctrl));
	addPage(tools::Tool::ERASER, new tools::EraserSettings("eraser", tr("Eraser"), m_ctrl));
	addPage(tools::Tool::PICKER, new tools::ColorPickerSettings("picker", tr("Color Picker"), m_ctrl));
	addPage(tools::Tool::LINE, new tools::SimpleSettings("line", tr("Line"), "draw-line", tools::SimpleSettings::Line, true, m_ctrl));
	addPage(tools::Tool::RECTANGLE, new tools::SimpleSettings("rectangle", tr("Rectangle"), "draw-rectangle", tools::SimpleSettings::Rectangle, false, m_ctrl));
	addPage(tools::Tool::ELLIPSE, new tools::SimpleSettings("ellipse", tr("Ellipse"), "draw-ellipse", tools::SimpleSettings::Ellipse, true, m_ctrl));
	addPage(tools::Tool::FLOODFILL, new tools::FillSettings("fill", tr("Flood Fill"), m_ctrl));
	addPage(tools::Tool::ANNOTATION, new tools::AnnotationSettings("annotation", tr("Annotation"), m_ctrl));
	addPage(tools::Tool::SELECTION, new tools::SelectionSettings("selection", tr("Selection (Rectangular)"), false, m_ctrl));
	addPage(tools::Tool::POLYGONSELECTION, new tools::SelectionSettings("polygonselection", tr("Selection (Free-Form)"), true, m_ctrl));
	addPage(tools::Tool::LASERPOINTER, new tools::LaserPointerSettings("laser", tr("Laser pointer"), m_ctrl));

	_currenttool = getToolSettingsPage(tools::Tool::BRUSH);

	connect(static_cast<tools::ColorPickerSettings*>(getToolSettingsPage(tools::Tool::PICKER)), &tools::ColorPickerSettings::colorSelected,
			this, &ToolSettings::setForegroundColor);

	// Create color changer dialogs
	_fgdialog = new color_widgets::ColorDialog(this);
	_fgdialog->setAlphaEnabled(false);
	_fgdialog->setWindowTitle(tr("Foreground Color"));
	connect(_fgdialog, SIGNAL(colorSelected(QColor)), this, SLOT(setForegroundColor(QColor)));
}

ToolSettings::~ToolSettings()
{
	for(unsigned int i=0;i<(sizeof(m_settingspage) / sizeof(*m_settingspage));++i)
		delete m_settingspage[i];
}

void ToolSettings::addPage(tools::Tool::Type type, tools::ToolSettings *page)
{
	Q_ASSERT(type >= 0 && type < tools::Tool::_LASTTOOL);
	Q_ASSERT(m_settingspage[type] == nullptr);

	m_settingspage[type] = page;
	m_widgets->addWidget(page->createUi(this));
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

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Tool::Type tool)
{
	Q_ASSERT(tool>=0 && tool < tools::Tool::_LASTTOOL);
	Q_ASSERT(m_settingspage[tool] != nullptr);
	return m_settingspage[tool];
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Tool::Type tool) {
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

void ToolSettings::selectTool(tools::Tool::Type tool)
{
	tools::ToolSettings *ts = getToolSettingsPage(tool);
	if(!ts) {
		qWarning("selectTool: invalid tool %d", tool);
		return;
	}

	_currenttool = ts;

	setWindowTitle(QStringLiteral("%1. %2").arg(currentToolSlot()+1).arg(_currenttool->getTitle()));
	m_widgets->setCurrentWidget(_currenttool->getUi());
	_currenttool->setForeground(foregroundColor());
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

tools::Tool::Type ToolSettings::currentTool() const
{
	return tools::Tool::Type(_toolprops[_currentQuickslot].currentTool());
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
	selectTool(tools::Tool::Type(_toolprops[i].currentTool()));
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

void ToolSettings::changeForegroundColor()
{
	_fgdialog->showColor(_foreground);
}

void ToolSettings::quickAdjustCurrent1(qreal adjustment)
{
	_currenttool->quickAdjust1(adjustment);
}

/**
 * @brief Update the tool slot button to match the stored tool settings
 * @param i
 */
void ToolSettings::updateToolSlot(int i, bool typeChanged)
{
	int tool = _toolprops[i].currentTool();
	tools::ToolSettings *ts = getToolSettingsPage(tools::Tool::Type(tool));
	if(!ts)
		ts = getToolSettingsPage(tools::Tool::PEN);


	_quickslot[i]->setColors(_toolprops[i].foregroundColor(), _toolprops[i].backgroundColor());

	if(typeChanged) {
		_quickslot[i]->setIcons(ts->getIcon(icon::LIGHT), ts->getIcon(icon::DARK));
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
	if(near && !_eraserActive) {
		_eraserOverride = currentTool();
		setTool(tools::Tool::ERASER);
		_eraserActive = true;
	} else if(!near && _eraserActive) {
		setTool(tools::Tool::Type(_eraserOverride));
		_eraserActive = false;
	}
}

void ToolSettings::disableEraserOverride(tools::Tool::Type tool)
{
	if(_eraserOverride == tool)
		_eraserOverride = tools::Tool::BRUSH; // select some tool that can't be disabled
}

}

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

#include "docks/toolsettingsdock.h"
#include "docks/utils.h"

#include "toolwidgets/brushsettings.h"
#include "toolwidgets/shapetoolsettings.h"
#include "toolwidgets/colorpickersettings.h"
#include "toolwidgets/selectionsettings.h"
#include "toolwidgets/annotationsettings.h"
#include "toolwidgets/fillsettings.h"
#include "toolwidgets/lasersettings.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QSettings>

#include <ColorDialog>

namespace docks {

ToolSettings::ToolSettings(tools::ToolController *ctrl, QWidget *parent)
	: QDockWidget(parent), m_settingspage{},
	m_ctrl(ctrl), m_eraserOverride(0), m_eraserActive(false)
{
	Q_ASSERT(ctrl);

	setStyleSheet(defaultDockStylesheet());

	// Create a widget stack
	m_widgets = new QStackedWidget(this);
	setWidget(m_widgets);

	addPage(new tools::PenSettings("pen", tr("Pen"), m_ctrl));
	addPage(new tools::BrushSettings("brush", tr("Brush"), m_ctrl));
	addPage(new tools::SmudgeSettings("smudge", tr("Watercolor"), m_ctrl));
	addPage(new tools::EraserSettings("eraser", tr("Eraser"), m_ctrl));
	addPage(new tools::ColorPickerSettings("picker", tr("Color Picker"), m_ctrl));
	addPage(new tools::SimpleSettings("line", tr("Line"), "draw-line", tools::SimpleSettings::Line, true, m_ctrl));
	addPage(new tools::SimpleSettings("rectangle", tr("Rectangle"), "draw-rectangle", tools::SimpleSettings::Rectangle, false, m_ctrl));
	addPage(new tools::SimpleSettings("ellipse", tr("Ellipse"), "draw-ellipse", tools::SimpleSettings::Ellipse, true, m_ctrl));
	addPage(new tools::FillSettings("fill", tr("Flood Fill"), m_ctrl));
	addPage(new tools::AnnotationSettings("annotation", tr("Annotation"), m_ctrl));
	addPage(new tools::SelectionSettings("selection", tr("Selection (Rectangular)"), false, m_ctrl));
	addPage(new tools::SelectionSettings("polygonselection", tr("Selection (Free-Form)"), true, m_ctrl));
	addPage(new tools::LaserPointerSettings("laser", tr("Laser pointer"), m_ctrl));

	m_currenttool = getToolSettingsPage(tools::Tool::BRUSH);
	setWindowTitle(m_currenttool->getTitle());

	connect(static_cast<tools::ColorPickerSettings*>(getToolSettingsPage(tools::Tool::PICKER)), &tools::ColorPickerSettings::colorSelected,
			this, &ToolSettings::setForegroundColor);

	// Create color changer dialogs
	m_colorDialog = new color_widgets::ColorDialog(this);
	m_colorDialog->setAlphaEnabled(false);
	connect(m_colorDialog, &color_widgets::ColorDialog::colorSelected, this, &ToolSettings::setForegroundColor);
}

ToolSettings::~ToolSettings()
{
	for(unsigned int i=0;i<(sizeof(m_settingspage) / sizeof(*m_settingspage));++i)
		delete m_settingspage[i];
}

void ToolSettings::addPage(tools::ToolSettings *page)
{
	Q_ASSERT(page->toolType() >= 0 && page->toolType() < tools::Tool::_LASTTOOL);
	Q_ASSERT(m_settingspage[page->toolType()] == nullptr);

	m_settingspage[page->toolType()] = page;
	m_widgets->addWidget(page->createUi(this));
}

void ToolSettings::readSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.beginGroup("toolset");
	for(int i=0;i<tools::Tool::_LASTTOOL;++i) {
		cfg.beginGroup(m_settingspage[i]->getName());
		m_settingspage[i]->restoreToolSettings(tools::ToolProperties::load(cfg));
		cfg.endGroup();
	}
	cfg.endGroup();
	setForegroundColor(cfg.value("color").value<QColor>());
	setTool(tools::Tool::Type(cfg.value("tool").toInt()));
}

void ToolSettings::saveSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.setValue("tool", m_currenttool->toolType());
	cfg.setValue("color", m_color);

	cfg.beginGroup("toolset");
	for(int i=0;i<tools::Tool::_LASTTOOL;++i) {
		cfg.beginGroup(m_settingspage[i]->getName());
		m_settingspage[i]->saveToolSettings().save(cfg);
		cfg.endGroup();
	}
}

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Tool::Type tool)
{
	Q_ASSERT(tool>=0 && tool < tools::Tool::_LASTTOOL);
	Q_ASSERT(m_settingspage[tool] != nullptr);
	if(tool>=0 && tool<tools::Tool::_LASTTOOL)
		return m_settingspage[tool];
	else
		return nullptr;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Tool::Type tool) {
	m_previousTool = currentTool();
	selectTool(tool);
}

void ToolSettings::setToolAndProps(const tools::ToolProperties &tool)
{
	if(tool.toolType()<0)
		return;
	m_previousTool = currentTool();
	selectTool(tools::Tool::Type(tool.toolType()), tool);
}

void ToolSettings::setPreviousTool()
{
	selectTool(m_previousTool);
}

void ToolSettings::selectTool(tools::Tool::Type tool, const tools::ToolProperties &props)
{
	tools::ToolSettings *ts = getToolSettingsPage(tool);
	if(!ts) {
		qWarning("selectTool(%d): no such tool!", tool);
		return;
	}

	m_currenttool = ts;

	if(props.toolType() == tool)
		m_currenttool->restoreToolSettings(props);

	m_currenttool->setForeground(m_color);
	m_currenttool->pushSettings();

	setWindowTitle(m_currenttool->getTitle());
	m_widgets->setCurrentWidget(m_currenttool->getUi());

	emit toolChanged(tool);
	emit sizeChanged(m_currenttool->getSize());
	updateSubpixelMode();
}

tools::ToolProperties ToolSettings::getCurrentToolProperties() const
{
	return m_currenttool->saveToolSettings();
}

void ToolSettings::updateSubpixelMode()
{
	emit subpixelModeChanged(m_currenttool->getSubpixelMode());
}

tools::Tool::Type ToolSettings::currentTool() const
{
	return tools::Tool::Type(m_currenttool->toolType());
}

QColor ToolSettings::foregroundColor() const
{
	return m_color;
}

void ToolSettings::setForegroundColor(const QColor& color)
{
	if(color != m_color) {
		m_color = color;

		m_currenttool->setForeground(color);

		if(m_colorDialog->isVisible())
			m_colorDialog->setColor(color);

		emit foregroundColorChanged(color);
	}
}

void ToolSettings::changeForegroundColor()
{
	m_colorDialog->showColor(m_color);
}

void ToolSettings::quickAdjustCurrent1(qreal adjustment)
{
	m_currenttool->quickAdjust1(adjustment);
}

void ToolSettings::eraserNear(bool near)
{
	if(near && !m_eraserActive) {
		m_eraserOverride = currentTool();
		setTool(tools::Tool::ERASER);
		m_eraserActive = true;
	} else if(!near && m_eraserActive) {
		setTool(tools::Tool::Type(m_eraserOverride));
		m_eraserActive = false;
	}
}

void ToolSettings::disableEraserOverride(tools::Tool::Type tool)
{
	if(m_eraserOverride == tool)
		m_eraserOverride = tools::Tool::BRUSH; // select some tool that can't be disabled
}

}

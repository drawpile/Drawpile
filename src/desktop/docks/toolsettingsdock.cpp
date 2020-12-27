/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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

#include "inputpresetmodel.h"

#include "docks/toolsettingsdock.h"
#include "docks/utils.h"

#include "toolwidgets/brushsettings.h"
#include "toolwidgets/colorpickersettings.h"
#include "toolwidgets/selectionsettings.h"
#include "toolwidgets/annotationsettings.h"
#include "toolwidgets/fillsettings.h"
#include "toolwidgets/lasersettings.h"
#include "toolwidgets/zoomsettings.h"
#include "toolwidgets/inspectorsettings.h"

#include "tools/toolproperties.h"

#include <ColorDialog>

#include <QStackedWidget>
#include <QApplication>
#include <QSettings>

namespace docks {

struct ToolPage {
	// Note: multiple different tools (e.g. Freehand and Line) can share the same settings
	QSharedPointer<tools::ToolSettings> settings;
	QString name;
	QString title;
};

struct ToolSettings::Private {
	ToolPage pages[tools::Tool::_LASTTOOL];
	QVector<QSharedPointer<tools::ToolSettings>> toolSettings;
	tools::ToolController *ctrl;

	QStackedWidget *widgetStack;
	color_widgets::ColorDialog *colorDialog;

	tools::Tool::Type currentTool;
	tools::Tool::Type previousTool;
	int previousToolSlot;
	QColor color;

	bool switchedWithStylusEraser;

	tools::ToolSettings *currentSettings() {
		Q_ASSERT(currentTool>=0 && currentTool <= tools::Tool::_LASTTOOL);
		return pages[currentTool].settings.data();
	}

	Private(tools::ToolController *ctrl, input::PresetModel *presetModel)
		: ctrl(ctrl),
		  widgetStack(nullptr),
		  colorDialog(nullptr),
		  currentTool(tools::Tool::FREEHAND),
		  previousTool(tools::Tool::FREEHAND),
		  previousToolSlot(0),
		  color(Qt::black),
		  switchedWithStylusEraser(false)
	{
		Q_ASSERT(ctrl);

		// Create tool pages
		auto brush = QSharedPointer<tools::ToolSettings>(new tools::BrushSettings(ctrl, presetModel));
		auto sel = QSharedPointer<tools::ToolSettings>(new tools::SelectionSettings(ctrl));

		pages[tools::Tool::FREEHAND] = {
				brush,
				"freehand",
				QApplication::tr("Freehand")
			};
		pages[tools::Tool::ERASER] = {
				brush,
				"eraser",
				QApplication::tr("Eraser")
			};
		pages[tools::Tool::LINE] = {
				brush,
				"line",
				QApplication::tr("Line")
			};
		pages[tools::Tool::RECTANGLE] = {
				brush,
				"rectangle",
				QApplication::tr("Rectangle")
			};
		pages[tools::Tool::ELLIPSE] = {
				brush,
				"ellipse",
				QApplication::tr("Ellipse")
			};
		pages[tools::Tool::BEZIER] = {
				brush,
				"bezier",
				QApplication::tr("Bezier Curve")
			};
		pages[tools::Tool::FLOODFILL] = {
				QSharedPointer<tools::ToolSettings>(new tools::FillSettings(ctrl)),
				"fill",
				QApplication::tr("Flood Fill")
			};
		pages[tools::Tool::ANNOTATION] = {
				QSharedPointer<tools::ToolSettings>(new tools::AnnotationSettings(ctrl)),
				"annotation",
				QApplication::tr("Annotation")
			};
		pages[tools::Tool::PICKER] = {
				QSharedPointer<tools::ToolSettings>(new tools::ColorPickerSettings(ctrl)),
				"picker",
				QApplication::tr("Color Picker")
			};
		pages[tools::Tool::LASERPOINTER] = {
				QSharedPointer<tools::ToolSettings>(new tools::LaserPointerSettings(ctrl)),
				"laser",
				QApplication::tr("Laser Pointer")
			};
		pages[tools::Tool::SELECTION] = {
				sel,
				"selection",
				QApplication::tr("Selection (Rectangular)")
			};
		pages[tools::Tool::POLYGONSELECTION] = {
				sel,
				"selection",
				QApplication::tr("Selection (Free-Form)")
			};
		pages[tools::Tool::ZOOM] = {
			QSharedPointer<tools::ToolSettings>(new tools::ZoomSettings(ctrl)),
			"zoom",
			QApplication::tr("Zoom")
			};
		pages[tools::Tool::INSPECTOR] = {
			QSharedPointer<tools::ToolSettings>(new tools::InspectorSettings(ctrl)),
			"inspector",
			QApplication::tr("Inspector")
			};

		for(int i=0;i<tools::Tool::_LASTTOOL;++i) {
			if(!toolSettings.contains(pages[i].settings))
				toolSettings << pages[i].settings;
		}
	}
};

ToolSettings::ToolSettings(tools::ToolController *ctrl, input::PresetModel *presetModel,
		QWidget *parent) : QDockWidget(parent), d(new Private(ctrl, presetModel))
{
	setStyleSheet(defaultDockStylesheet());

	// Create a widget stack
	d->widgetStack = new QStackedWidget(this);
	setWidget(d->widgetStack);

	for(int i=0;i<tools::Tool::_LASTTOOL;++i) {
		if(!d->pages[i].settings->getUi())
			d->widgetStack->addWidget(d->pages[i].settings->createUi(this));
	}

	setWindowTitle(d->pages[d->currentTool].title);

	connect(static_cast<tools::BrushSettings*>(getToolSettingsPage(tools::Tool::FREEHAND)), &tools::BrushSettings::colorChanged,
			this, &ToolSettings::setForegroundColor);
	connect(static_cast<tools::BrushSettings*>(getToolSettingsPage(tools::Tool::FREEHAND)), &tools::BrushSettings::subpixelModeChanged,
			this, &ToolSettings::subpixelModeChanged);
	connect(static_cast<tools::ColorPickerSettings*>(getToolSettingsPage(tools::Tool::PICKER)), &tools::ColorPickerSettings::colorSelected,
			this, &ToolSettings::setForegroundColor);

	d->colorDialog = new color_widgets::ColorDialog(this);
	d->colorDialog->setAlphaEnabled(false);
	connect(d->colorDialog, &color_widgets::ColorDialog::colorSelected, this, &ToolSettings::setForegroundColor);
}

ToolSettings::~ToolSettings()
{
	delete d;
}

void ToolSettings::readSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.beginGroup("toolset");
	for(auto ts : d->toolSettings) {
		ts->restoreToolSettings(tools::ToolProperties::load(cfg, ts->toolType()));
	}
	cfg.endGroup();
	setForegroundColor(cfg.value("color").value<QColor>());
	selectTool(tools::Tool::Type(cfg.value("tool").toInt()));
}

void ToolSettings::saveSettings()
{
	QSettings cfg;
	cfg.beginGroup("tools");
	cfg.setValue("tool", d->currentTool);
	cfg.setValue("color", d->color);

	cfg.beginGroup("toolset");
	for(auto ts : d->toolSettings) {
		ts->saveToolSettings().save(cfg);
	}
}

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Tool::Type tool)
{
	Q_ASSERT(tool < tools::Tool::_LASTTOOL);
	if(tool<tools::Tool::_LASTTOOL)
		return d->pages[tool].settings.data();
	else
		return nullptr;
}

/**
 * Set which tool setting widget is visible
 * @param tool tool identifier
 */
void ToolSettings::setTool(tools::Tool::Type tool) {
	if(tool != d->currentTool) {
		d->previousTool = d->currentTool;
		tools::BrushSettings *bs = qobject_cast<tools::BrushSettings*>(d->currentSettings());
		d->previousToolSlot = bs ? bs->currentBrushSlot() : 0;
		selectTool(tool);
	}
}

void ToolSettings::setToolSlot(int idx)
{
	// Currently, brush tool is the only tool with tool slots
	tools::BrushSettings *bs = qobject_cast<tools::BrushSettings*>(d->currentSettings());
	if(bs) {
		// Eraser tool is a specialization of the freehand tool locked to the eraser slot
		if(d->currentTool == tools::Tool::ERASER)
			return;

		d->previousTool = d->currentTool;
		d->previousToolSlot = bs->currentBrushSlot();
		bs->selectBrushSlot(idx);
	} else {
		setTool(tools::Tool::FREEHAND);
		static_cast<tools::BrushSettings*>(getToolSettingsPage(tools::Tool::FREEHAND))->selectBrushSlot(idx);
	}
}

void ToolSettings::toggleEraserMode()
{
	d->currentSettings()->toggleEraserMode();
}

void ToolSettings::eraserNear(bool near)
{
	// Auto-switch to eraser mode only when using a brush tool, since
	// other tools don't currently have eraser modes.
	tools::BrushSettings *bs = qobject_cast<tools::BrushSettings*>(d->currentSettings());
	if(!bs)
		return;

	if(near) {
		// Eraser was just brought near: switch to erase mode if not already
		d->switchedWithStylusEraser = !bs->isCurrentEraserSlot();
		if(!bs->isCurrentEraserSlot())
			bs->selectEraserSlot(true);
	} else {
		// Eraser taken away: switch back
		if(d->switchedWithStylusEraser) {
			d->switchedWithStylusEraser = false;
			bs->selectEraserSlot(false);
		}
	}
}

void ToolSettings::setPreviousTool()
{
	selectTool(d->previousTool);
	tools::BrushSettings *bs = qobject_cast<tools::BrushSettings*>(d->currentSettings());
	if(bs)
		bs->selectBrushSlot(d->previousToolSlot);
}

void ToolSettings::selectTool(tools::Tool::Type tool)
{
	if(tool<0 || tool >= tools::Tool::_LASTTOOL) {
		qWarning("selectTool(%d): no such tool!", tool);
		tool = tools::Tool::FREEHAND;
	}

	tools::ToolSettings *ts = d->pages[tool].settings.data();
	if(!ts) {
		qWarning("selectTool(%d): tool settings not created!", tool);
		return;
	}

	d->currentTool = tool;
	ts->setActiveTool(tool);

	ts->setForeground(d->color);
	ts->pushSettings();

	setWindowTitle(d->pages[tool].title);
	d->widgetStack->setCurrentWidget(ts->getUi());

	emit toolChanged(tool);
	emit sizeChanged(ts->getSize());
	emit subpixelModeChanged(d->currentSettings()->getSubpixelMode(), d->currentSettings()->isSquare());
}

tools::Tool::Type ToolSettings::currentTool() const
{
	return d->currentTool;
}

QColor ToolSettings::foregroundColor() const
{
	return d->color;
}

void ToolSettings::setForegroundColor(const QColor& color)
{
	if(color.isValid() && color != d->color) {
		d->color = color;

		d->currentSettings()->setForeground(color);

		if(d->colorDialog->isVisible())
			d->colorDialog->setColor(color);

		emit foregroundColorChanged(color);
	}
}

void ToolSettings::changeForegroundColor()
{
	d->colorDialog->showColor(d->color);
}

void ToolSettings::quickAdjustCurrent1(qreal adjustment)
{
	d->currentSettings()->quickAdjust1(adjustment);
}

}

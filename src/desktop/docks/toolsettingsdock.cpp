// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/toolsettingsdock.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/main.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/toolwidgets/inspectorsettings.h"
#include "desktop/dialogs/colordialog.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"

#include <QtColorWidgets/color_palette.hpp>

#include <QIcon>
#include <QStackedWidget>
#include <QApplication>
#include <QLabel>

namespace docks {

static const int LASTUSED_COLOR_COUNT = 8;

struct ToolPage {
	// Note: multiple different tools (e.g. Freehand and Line) can share the same settings
	QSharedPointer<tools::ToolSettings> settings;
	QString name;
	QIcon icon;
	QString title;
};

struct ToolSettings::Private {
	ToolPage pages[tools::Tool::_LASTTOOL];
	QVector<QSharedPointer<tools::ToolSettings>> toolSettings;
	tools::ToolController *ctrl;

	QStackedWidget *widgetStack = nullptr;
	QStackedWidget *headerStack = nullptr;
	QLabel *headerLabel = nullptr;
	color_widgets::ColorDialog *colorDialog = nullptr;

	tools::Tool::Type currentTool = tools::Tool::FREEHAND;
	tools::Tool::Type previousTool = tools::Tool::FREEHAND;
	int previousToolSlot = 0;
	QColor color = Qt::black;
	QColor colorAlt = Qt::black;

	color_widgets::ColorPalette lastUsedColors;
	color_widgets::ColorPalette lastUsedColorsAlt;

	bool switchedWithStylusEraser = false;

	tools::ToolSettings *currentSettings() {
		Q_ASSERT(currentTool>=0 && currentTool <= tools::Tool::_LASTTOOL);
		return pages[currentTool].settings.data();
	}

	Private(tools::ToolController *ctrl_)
		: ctrl(ctrl_)
	{
		Q_ASSERT(ctrl);

		// Create tool pages
		auto brush = QSharedPointer<tools::ToolSettings>(new tools::BrushSettings(ctrl));
		auto sel = QSharedPointer<tools::ToolSettings>(new tools::SelectionSettings(ctrl));

		pages[tools::Tool::FREEHAND] = {
				brush,
				"freehand",
				QIcon::fromTheme("draw-brush"),
				QApplication::tr("Freehand")
			};
		pages[tools::Tool::ERASER] = {
				brush,
				"eraser",
			QIcon::fromTheme("draw-eraser"),
				QApplication::tr("Eraser")
			};
		pages[tools::Tool::LINE] = {
				brush,
				"line",
				QIcon::fromTheme("draw-line"),
				QApplication::tr("Line")
			};
		pages[tools::Tool::RECTANGLE] = {
				brush,
				"rectangle",
				QIcon::fromTheme("draw-rectangle"),
				QApplication::tr("Rectangle")
			};
		pages[tools::Tool::ELLIPSE] = {
				brush,
				"ellipse",
				QIcon::fromTheme("draw-ellipse"),
				QApplication::tr("Ellipse")
			};
		pages[tools::Tool::BEZIER] = {
				brush,
				"bezier",
				QIcon::fromTheme("draw-bezier-curves"),
				QApplication::tr("Bezier Curve")
			};
		pages[tools::Tool::FLOODFILL] = {
				QSharedPointer<tools::ToolSettings>(new tools::FillSettings(ctrl)),
				"fill",
				QIcon::fromTheme("fill-color"),
				QApplication::tr("Flood Fill")
			};
		pages[tools::Tool::ANNOTATION] = {
				QSharedPointer<tools::ToolSettings>(new tools::AnnotationSettings(ctrl)),
				"annotation",
				QIcon::fromTheme("draw-text"),
				QApplication::tr("Annotation")
			};
		pages[tools::Tool::PICKER] = {
				QSharedPointer<tools::ToolSettings>(new tools::ColorPickerSettings(ctrl)),
				"picker",
				QIcon::fromTheme("color-picker"),
				QApplication::tr("Color Picker")
			};
		pages[tools::Tool::LASERPOINTER] = {
				QSharedPointer<tools::ToolSettings>(new tools::LaserPointerSettings(ctrl)),
				"laser",
				QIcon::fromTheme("cursor-arrow"),
				QApplication::tr("Laser Pointer")
			};
		pages[tools::Tool::SELECTION] = {
				sel,
				"selection",
				QIcon::fromTheme("select-rectangular"),
				QApplication::tr("Selection (Rectangular)")
			};
		pages[tools::Tool::POLYGONSELECTION] = {
				sel,
				"selection",
				QIcon::fromTheme("edit-select-lasso"),
				QApplication::tr("Selection (Free-Form)")
			};
		pages[tools::Tool::ZOOM] = {
				QSharedPointer<tools::ToolSettings>(new tools::ZoomSettings(ctrl)),
				"zoom",
				QIcon::fromTheme("zoom-select"),
				QApplication::tr("Zoom")
			};
		pages[tools::Tool::INSPECTOR] = {
				QSharedPointer<tools::ToolSettings>(new tools::InspectorSettings(ctrl)),
				"inspector",
				QIcon::fromTheme("help-whatsthis"),
				QApplication::tr("Inspector")
			};

		for(int i=0;i<tools::Tool::_LASTTOOL;++i) {
			if(!toolSettings.contains(pages[i].settings))
				toolSettings << pages[i].settings;
		}
	}
};

ToolSettings::ToolSettings(tools::ToolController *ctrl, QWidget *parent)
	: DockBase(parent), d(new Private(ctrl))
{
	setWindowTitle(tr("Tool"));

	auto titleWidget = new TitleWidget(this);
	setTitleBarWidget(titleWidget);

	// Create a widget stack
	d->widgetStack = new QStackedWidget(this);
	d->headerStack = new QStackedWidget(this);

	// Label widget for pages without a custom header
	d->headerLabel = new QLabel;
	d->headerLabel->setAlignment(Qt::AlignCenter);
	d->headerStack->addWidget(d->headerLabel);

	setWidget(d->widgetStack);
	titleWidget->addCustomWidget(d->headerStack, true);

	tools::BrushSettings *bs = brushSettings();
	connect(
		bs, &tools::BrushSettings::colorChanged, this,
		&ToolSettings::setForegroundColor);
	connect(
		bs, &tools::BrushSettings::pixelSizeChanged, this,
		[this](int size) {
			if(d->currentTool == tools::Tool::FREEHAND || d->currentTool == tools::Tool::ERASER) {
				emit sizeChanged(size);
			}
		});
	connect(
		bs, &tools::BrushSettings::subpixelModeChanged, this,
		[this](bool subpixel, bool square) {
			if(d->currentTool == tools::Tool::FREEHAND || d->currentTool == tools::Tool::ERASER) {
				emit subpixelModeChanged(subpixel, square);
			}
		});
	connect(
		ctrl, &tools::ToolController::globalSmoothingChanged, bs,
		&tools::BrushSettings::setGlobalSmoothing);

	connect(colorPickerSettings(), &tools::ColorPickerSettings::colorSelected,
			this, &ToolSettings::setForegroundColor);

	tools::FillSettings *fs = fillSettings();
	connect(
		fs, &tools::FillSettings::pixelSizeChanged, this,
		[this](int size) {
			if(d->currentTool == tools::Tool::FLOODFILL) {
				emit sizeChanged(size);
			}
		});
	connect(
		d->ctrl, &tools::ToolController::activeLayerChanged, fs,
		&tools::FillSettings::setActiveLayer);

	connect(d->ctrl, &tools::ToolController::activeBrushChanged, this,
			&ToolSettings::activeBrushChanged);

	d->colorDialog = dialogs::newColorDialog(this);
	d->colorDialog->setAlphaEnabled(false);
	connect(d->colorDialog, &color_widgets::ColorDialog::colorSelected, this, &ToolSettings::setForegroundColor);

	// Tool settings are only saved periodically currently, see TODO below.
	// Mixing periodic and instantaneous saving causes some desynchronization
	// with regards to tool slot colors, causing them to flicker when switching.
	auto &settings = dpApp().settings();
	setForegroundColor(settings.lastToolColor());
	selectTool(settings.lastTool());
}

ToolSettings::~ToolSettings()
{
	delete d;
}

void ToolSettings::saveSettings()
{
	desktop::settings::Settings::ToolsetType toolset;
	for(auto ts : qAsConst(d->toolSettings)) {
		// If no UI was loaded then the settings could not have changed and
		// there is no need to re-save them
		if(ts->getUi()) {
			// TODO: Tool settings should always be going to settings whenever
			// they are changed, and should not create a separate object just to
			// save
			const auto saveTs = ts->saveToolSettings();
			const auto [name, props] = saveTs.save();
			if (!name.isEmpty()) {
				toolset[name] = props;
			}
		}
	}
	auto &settings = dpApp().settings();
	settings.setToolset(toolset);
	settings.setLastToolColor(foregroundColor());
	settings.setLastTool(currentTool());
}

bool ToolSettings::isCurrentToolLocked() const
{
	return d->pages[d->currentTool].settings->isLocked();
}

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Tool::Type tool)
{
	Q_ASSERT(tool < tools::Tool::_LASTTOOL);
	if(tool<tools::Tool::_LASTTOOL) {
		auto ts = d->pages[tool].settings.data();
		if(!ts->getUi()) {
			d->widgetStack->addWidget(ts->createUi(this));
			auto *headerWidget = ts->getHeaderWidget();
			if(headerWidget)
				d->headerStack->addWidget(headerWidget);

			const auto toolset = dpApp().settings().toolset();
			ts->restoreToolSettings(tools::ToolProperties::load(ts->toolType(), toolset[ts->toolType()]));
		}
		return ts;
	}
	else
		return nullptr;
}

tools::AnnotationSettings *ToolSettings::annotationSettings()
{
	return static_cast<tools::AnnotationSettings *>(
		getToolSettingsPage(tools::Tool::ANNOTATION));
}

tools::BrushSettings *ToolSettings::brushSettings()
{
	return static_cast<tools::BrushSettings *>(
		getToolSettingsPage(tools::Tool::FREEHAND));
}

tools::ColorPickerSettings *ToolSettings::colorPickerSettings()
{
	return static_cast<tools::ColorPickerSettings *>(
		getToolSettingsPage(tools::Tool::PICKER));
}

tools::FillSettings *ToolSettings::fillSettings()
{
	return static_cast<tools::FillSettings *>(
		getToolSettingsPage(tools::Tool::FLOODFILL));
}

tools::InspectorSettings *ToolSettings::inspectorSettings()
{
	return static_cast<tools::InspectorSettings *>(
		getToolSettingsPage(tools::Tool::INSPECTOR));
}

tools::LaserPointerSettings *ToolSettings::laserPointerSettings()
{
	return static_cast<tools::LaserPointerSettings *>(
		getToolSettingsPage(tools::Tool::LASERPOINTER));
}

tools::SelectionSettings *ToolSettings::selectionSettings()
{
	return static_cast<tools::SelectionSettings *>(
		getToolSettingsPage(tools::Tool::SELECTION));
}

tools::ZoomSettings *ToolSettings::zoomSettings()
{
	return static_cast<tools::ZoomSettings *>(
		getToolSettingsPage(tools::Tool::ZOOM));
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

void ToolSettings::toggleRecolorMode()
{
	d->currentSettings()->toggleRecolorMode();
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

void ToolSettings::swapLastUsedColors()
{
	std::swap(d->lastUsedColors, d->lastUsedColorsAlt);
	const QColor c = d->colorAlt;
	d->colorAlt = d->color;
	setForegroundColor(c);

	emit lastUsedColorsChanged(d->lastUsedColors);
}

void ToolSettings::addLastUsedColor(const QColor &color)
{
	{
		const auto c = d->lastUsedColors.colorAt(0);
		if(c.isValid() && c.rgb() == color.rgb())
			return;
	}

	// Move color to the front of the palette
	d->lastUsedColors.insertColor(0, color);
	for(int i=1;i<d->lastUsedColors.count();++i) {
		if(d->lastUsedColors.colorAt(i).rgb() == color.rgb()) {
			d->lastUsedColors.eraseColor(i);
			break;
		}
	}

	// Limit number of remembered colors
	if(d->lastUsedColors.count() > LASTUSED_COLOR_COUNT)
		d->lastUsedColors.eraseColor(LASTUSED_COLOR_COUNT);

	emit lastUsedColorsChanged(d->lastUsedColors);
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
	if(tool >= tools::Tool::_LASTTOOL) {
		qWarning("selectTool(%d): no such tool!", tool);
		tool = tools::Tool::FREEHAND;
	}

	tools::ToolSettings *ts = getToolSettingsPage(tool);
	if(!ts) {
		qWarning("selectTool(%d): tool settings not created!", tool);
		return;
	}

	d->currentTool = tool;
	ts->setActiveTool(tool);

	ts->setForeground(d->color);
	ts->pushSettings();

	d->widgetStack->setCurrentWidget(ts->getUi());
	if(ts->getHeaderWidget()) {
		d->headerStack->setCurrentWidget(ts->getHeaderWidget());
	} else {
		d->headerLabel->setText(d->pages[tool].title);
		d->headerStack->setCurrentWidget(d->headerLabel);
	}

	TitleWidget *titleWidget = qobject_cast<TitleWidget *>(titleBarWidget());
	if(titleWidget) {
		titleWidget->setKeepButtonSpace(ts->keepTitleBarButtonSpace());
	}

	triggerUpdate();
}

void ToolSettings::triggerUpdate()
{
	emit toolChanged(d->currentTool);
	tools::ToolSettings *ts = d->currentSettings();
	if(ts) {
		emit sizeChanged(ts->getSize());
		emit subpixelModeChanged(ts->getSubpixelMode(), ts->isSquare());
	}
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

void ToolSettings::stepAdjustCurrent1(bool increase)
{
	d->currentSettings()->stepAdjust1(increase);
}

}

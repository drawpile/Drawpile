// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/main.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/toolwidgets/inspectorsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/pansettings.h"
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/toolwidgets/transformsettings.h"
#include "desktop/toolwidgets/zoomsettings.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/transform.h"
#include <QIcon>
#include <QLabel>
#include <QStackedWidget>
#include <QtColorWidgets/color_palette.hpp>
#include <cmath>
#include <utility>

namespace docks {

struct ToolPage {
	// Note: multiple different tools (e.g. Freehand and Line) can share the
	// same settings
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
	color_widgets::ColorDialog *foregroundColorDialog = nullptr;
	color_widgets::ColorDialog *backgroundColorDialog = nullptr;

	tools::Tool::Type currentTool = tools::Tool::FREEHAND;
	tools::Tool::Type previousTool = tools::Tool::FREEHAND;
	int previousToolSlot = 0;
	QColor foregroundColor = Qt::black;
	QColor backgroundColor = Qt::white;

	color_widgets::ColorPalette lastUsedColors{
		QVector<QColor>{}, QString{}, LASTUSED_COLOR_COUNT};

	bool switchedWithStylusEraser = false;

	tools::ToolSettings *currentSettings()
	{
		Q_ASSERT(currentTool >= 0 && currentTool <= tools::Tool::_LASTTOOL);
		return pages[currentTool].settings.data();
	}

	Private(tools::ToolController *ctrl_)
		: ctrl(ctrl_)
	{
		Q_ASSERT(ctrl);

		// Create tool pages
		auto brush =
			QSharedPointer<tools::ToolSettings>(new tools::BrushSettings(ctrl));
		auto sel = QSharedPointer<tools::ToolSettings>(
			new tools::SelectionSettings(ctrl));

		pages[tools::Tool::FREEHAND] = {
			brush, "freehand", QIcon::fromTheme("draw-brush"),
			QApplication::tr("Freehand")};
		pages[tools::Tool::ERASER] = {
			brush, "eraser", QIcon::fromTheme("draw-eraser"),
			QApplication::tr("Eraser")};
		pages[tools::Tool::LINE] = {
			brush, "line", QIcon::fromTheme("draw-line"),
			QApplication::tr("Line")};
		pages[tools::Tool::RECTANGLE] = {
			brush, "rectangle", QIcon::fromTheme("draw-rectangle"),
			QApplication::tr("Rectangle")};
		pages[tools::Tool::ELLIPSE] = {
			brush, "ellipse", QIcon::fromTheme("draw-ellipse"),
			QApplication::tr("Ellipse")};
		pages[tools::Tool::BEZIER] = {
			brush, "bezier", QIcon::fromTheme("draw-bezier-curves"),
			QApplication::tr("Bezier Curve")};
		pages[tools::Tool::FLOODFILL] = {
			QSharedPointer<tools::ToolSettings>(new tools::FillSettings(ctrl)),
			"fill", QIcon::fromTheme("fill-color"),
			QApplication::tr("Flood Fill")};
		pages[tools::Tool::ANNOTATION] = {
			QSharedPointer<tools::ToolSettings>(
				new tools::AnnotationSettings(ctrl)),
			"annotation", QIcon::fromTheme("draw-text"),
			QApplication::tr("Annotation")};
		pages[tools::Tool::PICKER] = {
			QSharedPointer<tools::ToolSettings>(
				new tools::ColorPickerSettings(ctrl)),
			"picker", QIcon::fromTheme("color-picker"),
			QApplication::tr("Color Picker")};
		pages[tools::Tool::LASERPOINTER] = {
			QSharedPointer<tools::ToolSettings>(
				new tools::LaserPointerSettings(ctrl)),
			"laser", QIcon::fromTheme("cursor-arrow"),
			QApplication::tr("Laser Pointer")};
		pages[tools::Tool::SELECTION] = {
			sel, "selection", QIcon::fromTheme("select-rectangular"),
			QApplication::tr("Selection (Rectangular)")};
		pages[tools::Tool::POLYGONSELECTION] = {
			sel, "selection", QIcon::fromTheme("edit-select-lasso"),
			QApplication::tr("Selection (Free-Form)")};
		pages[tools::Tool::MAGICWAND] = {
			sel, "selection", QIcon::fromTheme("drawpile_magicwand"),
			QApplication::tr("Selection (Magic Wand)")};
		pages[tools::Tool::TRANSFORM] = {
			QSharedPointer<tools::ToolSettings>(
				new tools::TransformSettings(ctrl)),
			"transform", QIcon::fromTheme("drawpile_transform"),
			QApplication::tr("Transform")};
		pages[tools::Tool::PAN] = {
			QSharedPointer<tools::ToolSettings>(new tools::PanSettings(ctrl)),
			"pan", QIcon::fromTheme("hand"), QApplication::tr("Pan")};
		pages[tools::Tool::ZOOM] = {
			QSharedPointer<tools::ToolSettings>(new tools::ZoomSettings(ctrl)),
			"zoom", QIcon::fromTheme("edit-find"), QApplication::tr("Zoom")};
		pages[tools::Tool::INSPECTOR] = {
			QSharedPointer<tools::ToolSettings>(
				new tools::InspectorSettings(ctrl)),
			"inspector", QIcon::fromTheme("help-whatsthis"),
			QApplication::tr("Inspector")};

		for(int i = 0; i < tools::Tool::_LASTTOOL; ++i) {
			if(!toolSettings.contains(pages[i].settings)) {
				toolSettings << pages[i].settings;
			}
		}
	}
};

ToolSettings::ToolSettings(tools::ToolController *ctrl, QWidget *parent)
	: DockBase(
		  tr("Tool Settings"), tr("Tool"), QIcon::fromTheme("configure"),
		  parent)
	, d(new Private(ctrl))
{
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
	titleWidget->addCustomWidget(d->headerStack, 1);

	tools::BrushSettings *bs = brushSettings();
	connect(
		bs, &tools::BrushSettings::colorChanged, this,
		&ToolSettings::setForegroundColor);
	connect(
		bs, &tools::BrushSettings::pixelSizeChanged, this, [this](int size) {
			if(hasBrushCursor(d->currentTool)) {
				emit sizeChanged(size);
			}
		});
	connect(
		bs, &tools::BrushSettings::subpixelModeChanged, this,
		[this](bool subpixel, bool square) {
			if(hasBrushCursor(d->currentTool)) {
				emit subpixelModeChanged(subpixel, square, false);
			}
		});
	connect(
		bs, &tools::BrushSettings::eraseModeChanged, this, [this](bool erase) {
			if(!erase && d->currentTool == tools::Tool::ERASER) {
				setTool(tools::Tool::FREEHAND);
			}
		});
	connect(
		ctrl, &tools::ToolController::globalSmoothingChanged, bs,
		&tools::BrushSettings::setGlobalSmoothing);

	connect(
		colorPickerSettings(), &tools::ColorPickerSettings::colorSelected, this,
		&ToolSettings::setForegroundColor);

	tools::ColorPickerSettings *cps = colorPickerSettings();
	connect(
		this, &ToolSettings::toolChanged, cps,
		&tools::ColorPickerSettings::cancelPickFromScreen);

	tools::FillSettings *fs = fillSettings();
	connect(fs, &tools::FillSettings::pixelSizeChanged, this, [this](int size) {
		if(d->currentTool == tools::Tool::FLOODFILL) {
			emit sizeChanged(size);
		}
	});

	connect(
		selectionSettings(), &tools::SelectionSettings::pixelSizeChanged, this,
		[this](int size) {
			if(d->currentTool == tools::Tool::MAGICWAND) {
				emit sizeChanged(size);
			}
		});

	connect(
		d->ctrl, &tools::ToolController::activeBrushChanged, this,
		&ToolSettings::activeBrushChanged);

	d->foregroundColorDialog = dialogs::newColorDialog(this);
	d->foregroundColorDialog->setAlphaEnabled(false);
	connect(
		d->foregroundColorDialog, &color_widgets::ColorDialog::colorSelected,
		this, &ToolSettings::setForegroundColor);

	d->backgroundColorDialog = dialogs::newColorDialog(this);
	d->backgroundColorDialog->setAlphaEnabled(false);
	connect(
		d->backgroundColorDialog, &color_widgets::ColorDialog::colorSelected,
		this, &ToolSettings::setBackgroundColor);

	connect(
		d->ctrl, &tools::ToolController::transformRequested, this,
		&ToolSettings::startTransformMove, Qt::QueuedConnection);
	connect(
		d->ctrl, &tools::ToolController::toolSwitchRequested, this,
		&ToolSettings::setToolTemporary);
	connect(
		d->ctrl, &tools::ToolController::showMessageRequested, this,
		&ToolSettings::showMessageRequested);

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
	for(auto ts : std::as_const(d->toolSettings)) {
		// If no UI was loaded then the settings could not have changed and
		// there is no need to re-save them
		if(ts->getUi()) {
			// TODO: Tool settings should always be going to settings whenever
			// they are changed, and should not create a separate object just to
			// save
			const auto saveTs = ts->saveToolSettings();
			const auto [name, props] = saveTs.save();
			if(!name.isEmpty()) {
				toolset[name] = props;
			}
		}
	}
	auto &settings = dpApp().settings();
	settings.setToolset(toolset);
	settings.setLastToolColor(foregroundColor());
	settings.setLastTool(currentTool());
}

bool ToolSettings::currentToolAffectsCanvas() const
{
	return d->pages[d->currentTool].settings->affectsCanvas();
}

bool ToolSettings::currentToolAffectsLayer() const
{
	return d->pages[d->currentTool].settings->affectsLayer();
}

bool ToolSettings::isCurrentToolLocked() const
{
	return d->pages[d->currentTool].settings->isLocked();
}

tools::ToolSettings *ToolSettings::getToolSettingsPage(tools::Tool::Type tool)
{
	Q_ASSERT(tool < tools::Tool::_LASTTOOL);
	if(tool < tools::Tool::_LASTTOOL) {
		auto ts = d->pages[tool].settings.data();
		if(!ts->getUi()) {
			d->widgetStack->addWidget(ts->createUi(this));
			auto *headerWidget = ts->getHeaderWidget();
			if(headerWidget) {
				d->headerStack->addWidget(headerWidget);
			}

			const auto toolset = dpApp().settings().toolset();
			ts->restoreToolSettings(tools::ToolProperties::load(
				ts->toolType(), toolset[ts->toolType()]));
		}
		return ts;
	} else
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

tools::PanSettings *ToolSettings::panSettings()
{
	return static_cast<tools::PanSettings *>(
		getToolSettingsPage(tools::Tool::PAN));
}

tools::SelectionSettings *ToolSettings::selectionSettings()
{
	return static_cast<tools::SelectionSettings *>(
		getToolSettingsPage(tools::Tool::SELECTION));
}

tools::TransformSettings *ToolSettings::transformSettings()
{
	return static_cast<tools::TransformSettings *>(
		getToolSettingsPage(tools::Tool::TRANSFORM));
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
void ToolSettings::setTool(tools::Tool::Type tool)
{
	if(tool != d->currentTool) {
		clearTemporaryTransform();
		d->previousTool = d->currentTool;
		tools::BrushSettings *bs =
			qobject_cast<tools::BrushSettings *>(d->currentSettings());
		d->previousToolSlot = bs ? bs->currentBrushSlot() : 0;
		selectTool(tool);
	}
}

void ToolSettings::setToolTemporary(tools::Tool::Type tool)
{
	if(tool != d->currentTool) {
		selectTool(tool);
	}
}

void ToolSettings::setToolSlot(int idx)
{
	// Currently, brush tool is the only tool with tool slots
	tools::BrushSettings *bs =
		qobject_cast<tools::BrushSettings *>(d->currentSettings());
	if(bs) {
		d->previousTool = d->currentTool;
		d->previousToolSlot = bs->currentBrushSlot();
		bs->selectBrushSlot(idx);
	} else {
		setTool(tools::Tool::FREEHAND);
		static_cast<tools::BrushSettings *>(
			getToolSettingsPage(tools::Tool::FREEHAND))
			->selectBrushSlot(idx);
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

void ToolSettings::switchToEraserSlot(bool near)
{
	// Only switch if currently using a brush, other tools don't have an eraser.
	tools::BrushSettings *bs =
		qobject_cast<tools::BrushSettings *>(d->currentSettings());
	if(bs) {
		if(near) {
			// Eraser was just brought near: switch to erase mode if not already
			if(!bs->isCurrentEraserSlot()) {
				d->switchedWithStylusEraser = true;
				bs->selectEraserSlot(true);
			}
		} else {
			// Eraser taken away: switch back
			if(d->switchedWithStylusEraser) {
				d->switchedWithStylusEraser = false;
				bs->selectEraserSlot(false);
			}
		}
	}
}

void ToolSettings::switchToEraserMode(bool near)
{
	// Only switch if currently using a brush and not using the eraser slot,
	// since those don't have an eraser mode that could be toggled.
	tools::BrushSettings *bs =
		qobject_cast<tools::BrushSettings *>(d->currentSettings());
	if(bs && !bs->isCurrentEraserSlot()) {
		if(near) {
			// Eraser was just brought near: switch to erase mode if not already
			if(!bs->currentBrush().isEraser()) {
				d->switchedWithStylusEraser = true;
				bs->setEraserMode(true);
			}
		} else {
			// Eraser taken away: switch back
			if(d->switchedWithStylusEraser) {
				d->switchedWithStylusEraser = false;
				bs->setEraserMode(false);
			}
		}
	}
}

void ToolSettings::swapColors()
{
	const QColor c = d->backgroundColor;
	setBackgroundColor(d->foregroundColor);
	setForegroundColor(c);
}

void ToolSettings::resetColors()
{
	setBackgroundColor(Qt::white);
	setForegroundColor(Qt::black);
}

void ToolSettings::addLastUsedColor(const QColor &color)
{
	{
		const auto c = d->lastUsedColors.colorAt(0);
		if(c.isValid() && c.rgb() == color.rgb()) {
			return;
		}
	}

	// Move color to the front of the palette
	d->lastUsedColors.insertColor(0, color);
	for(int i = 1; i < d->lastUsedColors.count(); ++i) {
		if(d->lastUsedColors.colorAt(i).rgb() == color.rgb()) {
			d->lastUsedColors.eraseColor(i);
			break;
		}
	}

	// Limit number of remembered colors
	if(d->lastUsedColors.count() > LASTUSED_COLOR_COUNT) {
		d->lastUsedColors.eraseColor(LASTUSED_COLOR_COUNT);
	}

	emit lastUsedColorsChanged(d->lastUsedColors);
}

void ToolSettings::setLastUsedColor(int i)
{
	QColor color = d->lastUsedColors.colorAt(i);
	if(color.isValid()) {
		setForegroundColor(color);
	}
}

void ToolSettings::startTransformMoveActiveLayer()
{
	startTransformMove(false);
}

void ToolSettings::startTransformMoveMask()
{
	startTransformMove(true);
}

void ToolSettings::startTransformMove(bool onlyMask)
{
	tools::Tool::Type toolToReturnTo = d->currentTool;
	setToolTemporary(tools::Tool::TRANSFORM);
	if(d->ctrl->activeTool() == tools::Tool::TRANSFORM) {
		d->ctrl->transformTool()->beginTemporaryMove(toolToReturnTo, onlyMask);
	} else {
		qWarning(
			"ToolSettings::startTransformMove: active tool is not transform");
	}
}

void ToolSettings::startTransformPaste(
	const QRect &srcBounds, const QImage &image)
{
	tools::Tool::Type toolToReturnTo = d->currentTool;
	setToolTemporary(tools::Tool::TRANSFORM);
	if(d->ctrl->activeTool() == tools::Tool::TRANSFORM) {
		d->ctrl->transformTool()->beginTemporaryPaste(
			toolToReturnTo, srcBounds, image);
	} else {
		qWarning(
			"ToolSettings::startTransformPaste: active tool is not transform");
	}
}

void ToolSettings::clearTemporaryTransform()
{
	d->ctrl->transformTool()->clearTemporary();
}

void ToolSettings::setPreviousTool()
{
	selectTool(d->previousTool);
	tools::BrushSettings *bs =
		qobject_cast<tools::BrushSettings *>(d->currentSettings());
	if(bs) {
		bs->selectBrushSlot(d->previousToolSlot);
	}
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

	if(tool != d->currentTool) {
		d->ctrl->finishActiveTool();
	}
	d->currentTool = tool;
	ts->setActiveTool(tool);
	emit toolChanged(d->currentTool);

	ts->setForeground(d->foregroundColor);
	emit foregroundColorChanged(d->foregroundColor);
	ts->pushSettings();

	d->widgetStack->setCurrentWidget(ts->getUi());
	if(ts->getHeaderWidget()) {
		d->headerStack->setCurrentWidget(ts->getHeaderWidget());
	} else {
		d->headerLabel->setText(d->pages[tool].title);
		d->headerStack->setCurrentWidget(d->headerLabel);
	}

	emit sizeChanged(ts->getSize());
	emit subpixelModeChanged(
		ts->getSubpixelMode(), ts->isSquare(), ts->requiresOutline());
	brushSettings()->triggerUpdate();
}

void ToolSettings::triggerUpdate()
{
	emit foregroundColorChanged(d->foregroundColor);
	emit backgroundColorChanged(d->backgroundColor);
	emit toolChanged(d->currentTool);
	tools::ToolSettings *ts = d->currentSettings();
	if(ts) {
		emit sizeChanged(ts->getSize());
		emit subpixelModeChanged(
			ts->getSubpixelMode(), ts->isSquare(), ts->requiresOutline());
	}
	brushSettings()->triggerUpdate();
}

tools::Tool::Type ToolSettings::currentTool() const
{
	return d->currentTool;
}

QColor ToolSettings::foregroundColor() const
{
	return d->foregroundColor;
}

QColor ToolSettings::backgroundColor() const
{
	return d->backgroundColor;
}

void ToolSettings::setForegroundColor(const QColor &color)
{
	if(color.isValid() && color != d->foregroundColor) {
		d->foregroundColor = color;

		d->currentSettings()->setForeground(color);

		if(d->foregroundColorDialog->isVisible()) {
			d->foregroundColorDialog->setColor(color);
		}

		d->ctrl->setForegroundColor(color);
		emit foregroundColorChanged(color);
	}
}

void ToolSettings::setBackgroundColor(const QColor &color)
{
	if(color.isValid() && color != d->backgroundColor) {
		d->backgroundColor = color;

		if(d->backgroundColorDialog->isVisible()) {
			d->backgroundColorDialog->setColor(color);
		}

		emit backgroundColorChanged(color);
	}
}

void ToolSettings::changeForegroundColor()
{
	d->foregroundColorDialog->showColor(d->foregroundColor);
}

void ToolSettings::changeBackgroundColor()
{
	d->backgroundColorDialog->showColor(d->backgroundColor);
}

void ToolSettings::quickAdjust(int type, qreal adjustment)
{
	switch(type) {
	case int(tools::QuickAdjustType::Tool):
		quickAdjustCurrent1(adjustment);
		break;
	case int(tools::QuickAdjustType::ColorH):
		requestColorAdjustment(0, adjustment, 360.0);
		break;
	case int(tools::QuickAdjustType::ColorS):
		requestColorAdjustment(1, adjustment, 256.0);
		break;
	case int(tools::QuickAdjustType::ColorV):
		requestColorAdjustment(2, adjustment, 256.0);
		break;
	default:
		qWarning("Unknown quick adjust type %d", type);
		break;
	}
}

void ToolSettings::requestColorAdjustment(
	int channel, qreal adjustment, qreal max)
{
	int amount = qRound(adjustment / 60.0 * max);
	if(adjustment < 0.0) {
		emit colorAdjustRequested(channel, qMin(-1, amount));
	} else if(adjustment > 0.0) {
		emit colorAdjustRequested(channel, qMax(1, amount));
	}
}

void ToolSettings::quickAdjustCurrent1(qreal adjustment)
{
	d->currentSettings()->quickAdjust1(adjustment);
}

void ToolSettings::stepAdjustCurrent1(bool increase)
{
	d->currentSettings()->stepAdjust1(increase);
}

bool ToolSettings::hasBrushCursor(tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::FREEHAND:
	case tools::Tool::ERASER:
	case tools::Tool::LINE:
	case tools::Tool::RECTANGLE:
	case tools::Tool::ELLIPSE:
	case tools::Tool::BEZIER:
		return true;
	default:
		return false;
	}
}

}

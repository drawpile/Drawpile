// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/brush.h>
#include <dpmsg/blend_mode.h>
}
#include "desktop/main.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/brushes/brush.h"
#include "libclient/settings.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_brushdock.h"
#include <QActionGroup>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QStandardItemModel>
#include <mypaint-brush-settings.h>

namespace tools {

namespace {

struct Preset : brushes::Preset {
	bool valid = false;
	bool attached = false;
	bool reattach = false;
	bool overwrite = false;

	static Preset makeDetached(
		const brushes::ActiveBrush &originalBrush, bool reattach = false,
		int id = 0)
	{
		Preset preset = {{}, true, false, reattach, false};
		preset.id = id;
		preset.originalBrush = originalBrush;
		return preset;
	}

	static Preset makeReattachable(const brushes::Preset &p)
	{
		Preset preset = {p, true, false, true, false};
		return preset;
	}

	static Preset makeAttached(const brushes::Preset &p)
	{
		Preset preset = {p, true, true, false, false};
		preset.changedBrush = {};
		return preset;
	}

	bool isAttached() const { return valid && attached; }

	bool changeName(const QString &name)
	{
		if(name == originalName) {
			if(changedName.has_value()) {
				changedName = {};
				return true;
			}
		} else {
			if(!changedName.has_value() || name != changedName.value()) {
				changedName = name;
				return true;
			}
		}
		return false;
	}

	bool changeDescription(const QString &description)
	{
		if(description == originalDescription) {
			if(changedDescription.has_value()) {
				changedDescription = {};
				return true;
			}
		} else {
			if(!changedDescription.has_value() ||
			   description != changedDescription.value()) {
				changedDescription = description;
				return true;
			}
		}
		return false;
	}

	bool changeThumbnail(const QPixmap &thumbnail)
	{
		qint64 cacheKey = thumbnail.cacheKey();
		if(cacheKey == originalThumbnail.cacheKey()) {
			if(changedThumbnail.has_value()) {
				changedThumbnail = {};
				return true;
			}
		} else {
			if(!changedThumbnail.has_value() ||
			   cacheKey != changedThumbnail->cacheKey()) {
				changedThumbnail = thumbnail;
				return true;
			}
		}
		return false;
	}

	void changeBrush(const brushes::ActiveBrush &brush, bool inEraserSlot)
	{
		if(originalBrush.equalPreset(brush, inEraserSlot)) {
			changedBrush = {};
		} else {
			changedBrush = brush;
		}
	}
};

struct Slot {
	brushes::ActiveBrush brush;
	QColor backgroundColor;
	Preset preset;
	widgets::GroupedToolButton *button;
};

}

struct BrushSettings::Private {
	Ui_BrushDock ui;
	brushes::BrushPresetModel *brushPresets = nullptr;

	Slot brushSlots[BRUSH_SLOT_COUNT];
	Slot eraserSlot;
	QWidget *brushSlotWidget = nullptr;

	QAction *editBrushAction;
	QAction *resetBrushAction;
	QAction *resetSlotPresetsAction;
	QAction *resetAllPresetsAction;
	QAction *newBrushAction;
	QAction *overwriteBrushAction;
	QAction *deleteBrushAction;
	QAction *detachBrushAction;

	BrushType brushType = BrushType::PixelRound;
	QActionGroup *brushTypeGroup;
	QAction *brushTypePixelRoundAction;
	QAction *brushTypePixelSquareAction;
	QAction *brushTypeSoftRoundAction;
	QAction *brushTypeMyPaintAction;

	DP_PaintMode paintMode = DP_PAINT_MODE_DIRECT;
	QActionGroup *paintModeGroup;
	QAction *paintModeDirectAction;
	QAction *paintModeIndirectWashAction;
	QAction *paintModeIndirectSoftAction;
	QAction *paintModeIndirectNormalAction;

	QActionGroup *stabilizationModeGroup;
	QAction *stabilizerAction;
	QAction *smoothingAction;
	QAction *finishStrokesAction;
	QAction *useBrushSampleCountAction;

	QMenu *menu;

	qreal quickAdjust1 = 0.0;
	int brushSlotCount = BRUSH_SLOT_COUNT;
	int current = 0;
	int previousNonEraser = 0;
	BrushMode previousBrushMode = UnknownMode;
	int previousBlendMode = -1;
	int globalSmoothing;
	int brushSizeLimit = -1;
	bool finishStrokes = true;
	bool useBrushSampleCount = true;
	bool shareBrushSlotColor = false;
	bool presetsAttach = true;
	bool updateInProgress = false;
	bool myPaintAllowed = true;
	bool automaticAlphaPreserve = true;

	Slot &slotAt(int i)
	{
		Q_ASSERT(i >= 0);
		Q_ASSERT(i < TOTAL_SLOT_COUNT);
		return i == ERASER_SLOT_INDEX ? eraserSlot : brushSlots[i];
	}

	brushes::ActiveBrush &brushAt(int i) { return slotAt(i).brush; }

	QColor &backgroundColorAt(int i) { return slotAt(i).backgroundColor; }

	Preset &presetAt(int i) { return slotAt(i).preset; }

	widgets::GroupedToolButton *buttonAt(int i) { return slotAt(i).button; }

	Slot &currentSlot() { return slotAt(current); }

	brushes::ActiveBrush &currentBrush() { return currentSlot().brush; }

	QColor &currentBackgroundColor() { return currentSlot().backgroundColor; }

	Preset &currentPreset() { return currentSlot().preset; }

	bool currentIsMyPaint()
	{
		return currentBrush().activeType() == brushes::ActiveBrush::MYPAINT;
	}

	brushes::StabilizationMode stabilizationMode()
	{
		if(smoothingAction->isChecked()) {
			return brushes::Smoothing;
		} else {
			return brushes::Stabilizer;
		}
	}

	Private(BrushSettings *b)
		: globalSmoothing{b->controller()->globalSmoothing()}
	{
	}
};

BrushSettings::BrushSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, d(new Private(this))
{
}

BrushSettings::~BrushSettings()
{
	delete d;
}

void BrushSettings::setActions(
	QAction *reloadPreset, QAction *reloadPresetSlots,
	QAction *reloadAllPresets, QAction *nextSlot, QAction *previousSlot,
	QAction *automaticAlphaPreserve)
{
	d->ui.reloadButton->setDefaultAction(reloadPreset);
	d->menu->addSeparator();
	d->menu->addAction(automaticAlphaPreserve);
	connect(
		d->resetBrushAction, &QAction::triggered, reloadPreset,
		&QAction::trigger);
	connect(
		d->resetSlotPresetsAction, &QAction::triggered, reloadPresetSlots,
		&QAction::trigger);
	connect(
		d->resetAllPresetsAction, &QAction::triggered, reloadAllPresets,
		&QAction::trigger);
	connect(
		nextSlot, &QAction::triggered, this, &BrushSettings::selectNextSlot);
	connect(
		previousSlot, &QAction::triggered, this,
		&BrushSettings::selectPreviousSlot);
}

void BrushSettings::connectBrushPresets(brushes::BrushPresetModel *brushPresets)
{
	d->brushPresets = brushPresets;

	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		Preset &preset = d->presetAt(i);
		if(preset.valid && preset.id != 0) {
			std::optional<brushes::Preset> opt =
				brushPresets->searchPresetBrushData(preset.id);
			preset.attached = opt.has_value();
			if(preset.attached) {
				preset.originalName = opt->originalName;
				preset.originalDescription = opt->originalDescription;
				preset.originalThumbnail = opt->originalThumbnail;
				preset.originalBrush = opt->originalBrush;
				preset.changedName = opt->changedName;
				preset.changedDescription = opt->changedDescription;
				preset.changedThumbnail = opt->changedThumbnail;
			}
			if(preset.overwrite) {
				changeBrushInSlot(preset.originalBrush, i);
				preset.overwrite = false;
			}
			if(preset.attached) {
				preset.changeBrush(d->brushAt(i), i == ERASER_SLOT_INDEX);
			}
		}
	}
	updateMenuActions();

	connect(
		brushPresets, &brushes::BrushPresetModel::presetChanged, this,
		&BrushSettings::handlePresetChanged);
	connect(
		brushPresets, &brushes::BrushPresetModel::presetRemoved, this,
		&BrushSettings::handlePresetRemoved);

	const Preset &preset = d->currentPreset();
	emit presetIdChanged(preset.id, preset.attached);
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	d->brushSlotWidget = new QWidget(parent);
	auto brushSlotWidgetLayout = new QHBoxLayout;
	brushSlotWidgetLayout->setSpacing(0);
	brushSlotWidgetLayout->setContentsMargins(0, 0, 0, 0);

	d->brushSlotWidget->setLayout(brushSlotWidgetLayout);

	widgets::GroupedToolButton *menuButton = new widgets::GroupedToolButton(
		widgets::GroupedToolButton::NotGrouped, d->brushSlotWidget);
	brushSlotWidgetLayout->addWidget(menuButton);
	menuButton->setIcon(QIcon::fromTheme("application-menu"));
	menuButton->setPopupMode(QToolButton::InstantPopup);

	d->menu = new QMenu(menuButton);
	menuButton->setMenu(d->menu);

	d->editBrushAction =
		d->menu->addAction(QIcon::fromTheme("configure"), tr("&Edit Brush…"));
	connect(
		d->editBrushAction, &QAction::triggered, this,
		&BrushSettings::editBrushRequested);

	d->resetBrushAction = d->menu->addAction(
		QIcon::fromTheme("view-refresh"), tr("&Reset Brush"));
	d->resetSlotPresetsAction =
		d->menu->addAction(tr("Reset All Brush &Slots"));
	d->resetAllPresetsAction = d->menu->addAction(tr("Reset All &Brushes"));

	d->menu->addSeparator();

	d->newBrushAction =
		d->menu->addAction(QIcon::fromTheme("list-add"), tr("&New Brush…"));
	connect(
		d->newBrushAction, &QAction::triggered, this,
		&BrushSettings::newBrushRequested);

	d->overwriteBrushAction = d->menu->addAction(
		QIcon::fromTheme("document-save"), tr("&Overwrite Brush"));
	connect(
		d->overwriteBrushAction, &QAction::triggered, this,
		&BrushSettings::overwriteBrushRequested);

	d->deleteBrushAction = d->menu->addAction(
		QIcon::fromTheme("trash-empty"), tr("&Delete Brush"));
	connect(
		d->deleteBrushAction, &QAction::triggered, this,
		&BrushSettings::deleteBrushRequested);

	d->menu->addSeparator();

	d->detachBrushAction = d->menu->addAction(
		QIcon::fromTheme("network-disconnect"), tr("De&tach Brush"));
	connect(
		d->detachBrushAction, &QAction::triggered, this,
		&BrushSettings::detachCurrentSlot);

	brushSlotWidgetLayout->addStretch(1);

	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		widgets::GroupedToolButton *button =
			new widgets::GroupedToolButton(d->brushSlotWidget);
		button->setCheckable(true);
		button->setAutoExclusive(true);
		button->setText(QString::number(i + 1));
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		brushSlotWidgetLayout->addWidget(button, 1);

		switch(i) {
		case 0:
			button->setGroupPosition(widgets::GroupedToolButton::GroupLeft);
			break;
		case ERASER_SLOT_INDEX:
			button->setGroupPosition(widgets::GroupedToolButton::GroupRight);
			button->setIcon(QIcon::fromTheme("draw-eraser"));
			break;
		default:
			button->setGroupPosition(widgets::GroupedToolButton::GroupCenter);
			// Hide for now so that this doesn't blow the widget width out of
			// proportion. If appropriate, it'll be reenabled by the first call
			// to setBrushSlotCount when that setting is loaded.
			button->setVisible(false);
			break;
		}

		connect(button, &QToolButton::clicked, this, [this, i]() {
			selectBrushSlot(i);
		});
		d->slotAt(i).button = button;
	}

	brushSlotWidgetLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	d->ui.setupUi(widget);

	// It's really hard to see that the paint mode button is disabled when
	// smudging is turned on, so we just hide it altogether and keep the space.
	utils::setWidgetRetainSizeWhenHidden(d->ui.paintMode, true);

	// Exponential sliders for easier picking of small values.
	d->ui.brushsizeBox->setExponentRatio(3.0);
	d->ui.brushspacingBox->setExponentRatio(3.0);
	d->ui.stabilizerBox->setExponentRatio(3.0);

	QMenu *brushTypeMenu = new QMenu(d->ui.brushTypeButton);
	d->brushTypeGroup = new QActionGroup(brushTypeMenu);
	d->brushTypePixelRoundAction = brushTypeMenu->addAction(
		QIcon::fromTheme("drawpile_pixelround"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Round Pixel Brush"));
	d->brushTypePixelSquareAction = brushTypeMenu->addAction(
		QIcon::fromTheme("drawpile_square"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Square Pixel Brush"));
	d->brushTypeSoftRoundAction = brushTypeMenu->addAction(
		QIcon::fromTheme("drawpile_round"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Soft Round Brush"));
	d->brushTypeMyPaintAction = brushTypeMenu->addAction(
		QIcon::fromTheme("drawpile_mypaint"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "MyPaint Brush"));
	d->brushTypeGroup->addAction(d->brushTypePixelRoundAction);
	d->brushTypeGroup->addAction(d->brushTypePixelSquareAction);
	d->brushTypeGroup->addAction(d->brushTypeSoftRoundAction);
	d->brushTypeGroup->addAction(d->brushTypeMyPaintAction);
	d->ui.brushTypeButton->setMenu(brushTypeMenu);

	QMenu *paintModeMenu = new QMenu(d->ui.paintMode);
	d->paintModeGroup = new QActionGroup(paintModeMenu);
	d->paintModeDirectAction = paintModeMenu->addAction(
		QIcon::fromTheme("drawpile_incremental_mode"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Direct Build-Up"));
	d->paintModeIndirectWashAction = paintModeMenu->addAction(
		QIcon::fromTheme("drawpile_wash_mode"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Indirect Wash"));
	d->paintModeIndirectSoftAction = paintModeMenu->addAction(
		QIcon::fromTheme("drawpile_soft_mode"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog", "Indirect Soft (Drawpile 2.2)"));
	d->paintModeIndirectNormalAction = paintModeMenu->addAction(
		QIcon::fromTheme("drawpile_indirect_mode"),
		QCoreApplication::translate(
			"dialogs::BrushSettingsDialog",
			"Indirect Build-Up (Drawpile 2.1)"));
	d->paintModeGroup->addAction(d->paintModeDirectAction);
	d->paintModeGroup->addAction(d->paintModeIndirectWashAction);
	d->paintModeGroup->addAction(d->paintModeIndirectSoftAction);
	d->paintModeGroup->addAction(d->paintModeIndirectNormalAction);
	d->ui.paintMode->setMenu(paintModeMenu);
	setPaintModeInUi(int(DP_PAINT_MODE_DIRECT));

	QMenu *stabilizerMenu = new QMenu{d->ui.stabilizerButton};
	d->stabilizationModeGroup = new QActionGroup{stabilizerMenu};
	d->stabilizerAction =
		stabilizerMenu->addAction(tr("Time-Based Stabilizer"));
	d->smoothingAction = stabilizerMenu->addAction(tr("Average Smoothing"));
	d->stabilizerAction->setStatusTip(tr(
		"Slows down the stroke and stabilizes it over time. Can produce very "
		"smooth results, but may feel sluggish."));
	d->smoothingAction->setStatusTip(
		tr("Simply averages inputs to get a smoother result. Faster than the "
		   "time-based stabilizer, but not as smooth."));
	d->stabilizerAction->setCheckable(true);
	d->smoothingAction->setCheckable(true);
	d->stabilizationModeGroup->addAction(d->stabilizerAction);
	d->stabilizationModeGroup->addAction(d->smoothingAction);
	stabilizerMenu->addSeparator();
	d->finishStrokesAction = stabilizerMenu->addAction(tr("Finish Strokes"));
	d->finishStrokesAction->setStatusTip(tr(
		"Draws stabilized strokes to the end, rather than cutting them off."));
	d->finishStrokesAction->setCheckable(true);
	d->useBrushSampleCountAction =
		stabilizerMenu->addAction(tr("Synchronize With Brush"));
	d->useBrushSampleCountAction->setStatusTip(tr(
		"Makes the stabilizer a brush setting, like in MyPaint, rather than an "
		"independent setting, like in Krita."));
	d->useBrushSampleCountAction->setCheckable(true);
	d->ui.stabilizerButton->setMenu(stabilizerMenu);

	connect(
		d->ui.preview, &widgets::BrushPreview::requestEditor, this,
		&BrushSettings::editBrushRequested);

	connect(
		d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::changeSizeSetting);
	connect(
		d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::changeRadiusLogarithmicSetting);

	connect(
		d->ui.blendmode, QOverload<int>::of(&QComboBox::activated), this,
		&BrushSettings::updateBlendMode);
	connect(
		d->ui.modeEraser, &QToolButton::clicked, this,
		&BrushSettings::setEraserMode);

	connect(
		brushTypeMenu, &QMenu::triggered, this,
		&BrushSettings::changeBrushType);
	connect(
		paintModeMenu, &QMenu::triggered, this,
		&BrushSettings::changePaintMode);

	connect(
		d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureSize, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.opacityBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureOpacity, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.hardnessBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureHardness, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.smudgingBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureSmudging, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::updateFromUi);
	connect(
		d->ui.colorpickupBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.brushspacingBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::updateFromUi);
	connect(
		d->ui.gainBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.stabilizerBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.smoothingBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.modeColorpick, &QToolButton::clicked, this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.alphaPreserve, &QToolButton::clicked, this,
		&BrushSettings::updateAlphaPreserve);

	connect(
		d->stabilizerAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->smoothingAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->stabilizerAction, &QAction::triggered, this,
		&BrushSettings::updateStabilizationSettingVisibility);
	connect(
		d->smoothingAction, &QAction::triggered, this,
		&BrushSettings::updateStabilizationSettingVisibility);

	connect(
		d->finishStrokesAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->useBrushSampleCountAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);

	// By default, the docker shows all settings at once, making it bigger on
	// startup and messing up the application layout. This will hide the excess
	// UI elements and bring the docker to a reasonable size to begin with.
	adjustSettingVisibilities(false, false);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindAutomaticAlphaPreserve(
		this, &BrushSettings::setAutomaticAlphaPreserve);

	return widget;
}

QWidget *BrushSettings::getHeaderWidget()
{
	return d->brushSlotWidget;
}

bool BrushSettings::isLocked()
{
	return getLock() != Lock::None;
}

BrushSettings::Lock BrushSettings::getLock()
{
	if(!d->myPaintAllowed && d->currentIsMyPaint()) {
		return Lock::MyPaintPermission;
	}
	return Lock::None;
}

QString BrushSettings::getLockDescription(Lock lock)
{
	switch(lock) {
	case Lock::MyPaintPermission:
		return tr("You don't have permission to use MyPaint brushes.");
	default:
		return QString{};
	}
}

void BrushSettings::setMyPaintAllowed(bool myPaintAllowed)
{
	d->myPaintAllowed = myPaintAllowed;
	updateUi();
}

void BrushSettings::setBrushSizeLimit(int brushSizeLimit)
{
	int previousLimit = d->brushSizeLimit;
	bool unlimited = brushSizeLimit < 0 || brushSizeLimit >= DP_BRUSH_SIZE_MAX;
	KisSliderSpinBox *brushSizeBox = d->ui.brushsizeBox;
	if(unlimited) {
		d->brushSizeLimit = -1;
		brushSizeBox->setSoftRange(0, 0);
		brushSizeBox->setSuffix(tr("px"));
	} else {
		d->brushSizeLimit = brushSizeLimit;
		brushSizeBox->setSoftRange(brushSizeBox->minimum(), brushSizeLimit);
		//: Limit suffix for the brush size slider, %1 is the size limit. So it
		//: will look something like "100/255px". Unless your language uses a
		//: different slash symbol or something, leave this unchanged.
		brushSizeBox->setSuffix(tr("/%1px").arg(brushSizeLimit));
	}
	updateRadiusLogarithmicLimit();

	if(d->brushSizeLimit != previousLimit) {
		changeSizeSetting(brushSizeBox->value());
		changeRadiusLogarithmicSetting(d->ui.radiusLogarithmicBox->value());
		d->ui.preview->setBrushSizeLimit(d->brushSizeLimit);
	}
}

int BrushSettings::brushSlotCount() const
{
	return d->brushSlotCount;
}

void BrushSettings::setBrushSlotCount(int count)
{
	bool wasInEraserSlot = isCurrentEraserSlot();
	d->brushSlotCount = qBound(1, count, BRUSH_SLOT_COUNT);
	d->buttonAt(0)->setIcon(
		d->brushSlotCount == 1 ? QIcon::fromTheme("draw-brush") : QIcon());

	for(int i = 0; i < BRUSH_SLOT_COUNT; ++i) {
		widgets::GroupedToolButton *button = d->buttonAt(i);
		bool active = i < d->brushSlotCount;
		button->setEnabled(active);
		button->setVisible(active);
	}

	if(!wasInEraserSlot && d->current >= d->brushSlotCount) {
		selectBrushSlot(d->brushSlotCount - 1);
	}
}

void BrushSettings::setShareBrushSlotColor(bool sameColor)
{
	d->shareBrushSlotColor = sameColor;
	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		Slot &slot = d->slotAt(i);
		slot.button->setColorSwatch(sameColor ? QColor() : slot.brush.qColor());
		slot.button->setBackgroundSwatch(
			sameColor ? QColor() : slot.backgroundColor);
	}
}

bool BrushSettings::brushPresetsAttach() const
{
	return d->presetsAttach;
}

void BrushSettings::setBrushPresetsAttach(bool brushPresetsAttach)
{
	if(brushPresetsAttach && !d->presetsAttach) {
		d->presetsAttach = true;
		for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
			Preset &preset = d->presetAt(i);
			if(preset.valid && preset.reattach && preset.id != 0) {
				std::optional<brushes::Preset> opt =
					d->brushPresets->searchPresetBrushData(preset.id);
				preset.attached = opt.has_value();
				if(preset.attached) {
					preset.originalName = opt->originalName;
					preset.originalDescription = opt->originalDescription;
					preset.originalThumbnail = opt->originalThumbnail;
					preset.originalBrush = opt->originalBrush;
				}
				preset.changeBrush(d->brushAt(i), i == ERASER_SLOT_INDEX);
				if(d->current == i) {
					emit presetIdChanged(preset.id, preset.attached);
					if(preset.attached) {
						d->ui.preview->setPreset(
							preset.effectiveThumbnail(), preset.hasChanges());
						updateChangesInCurrentBrushPreset();
					}
				}
			} else if(d->current == i) {
				emit presetIdChanged(0, false);
			}
			preset.reattach = false;
		}
		updateMenuActions();
	} else if(!brushPresetsAttach && d->presetsAttach) {
		d->presetsAttach = false;
		for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
			Preset &preset = d->presetAt(i);
			if(preset.isAttached()) {
				preset.attached = false;
				preset.reattach = true;
				if(d->current == i) {
					d->ui.preview->clearPreset();
				}
			} else {
				preset.reattach = false;
			}
		}
		updateMenuActions();
	}
}

void BrushSettings::setCurrentBrushDetached(const brushes::ActiveBrush &brush)
{
	setBrushDetachedInSlot(brush, d->current);
}

void BrushSettings::setCurrentBrushPreset(const brushes::Preset &p)
{
	setBrushPresetInSlot(p, d->current);
}

void BrushSettings::setBrushDetachedInSlot(
	const brushes::ActiveBrush &brush, int i)
{
	brushes::Preset p;
	p.originalBrush = brush;
	setBrushPresetInSlot(p, i);
}

void BrushSettings::changeCurrentBrush(const brushes::ActiveBrush &brush)
{
	changePresetBrush(changeBrushInSlot(brush, d->current));
	updateUi();
}

void BrushSettings::setBrushPresetInSlot(const brushes::Preset &p, int i)
{
	brushes::ActiveBrush brush = changeBrushInSlot(p.effectiveBrush(), i);
	Preset &preset = d->presetAt(i);
	bool isCurrent = i == d->current;
	if(p.id <= 0) {
		preset = Preset::makeDetached(brush);
		if(isCurrent) {
			emit presetIdChanged(0, false);
		}
	} else if(d->presetsAttach) {
		preset = Preset::makeAttached(p);
		preset.changeBrush(brush, i == ERASER_SLOT_INDEX);
		if(isCurrent) {
			emit presetIdChanged(p.id, true);
		}
	} else {
		preset = Preset::makeReattachable(p);
		emit presetIdChanged(p.id, false);
	}
	if(isCurrent) {
		updateMenuActions();
		updateUi();
	}
	updateChangesInBrushPresetInSlot(i);
}

brushes::ActiveBrush
BrushSettings::changeBrushInSlot(brushes::ActiveBrush brush, int i)
{
	brushes::ActiveBrush &slot = d->brushAt(i);
	brush.setQColor(slot.qColor());

	brushes::ActiveBrush::ActiveType activeType = brush.activeType();
	switch(activeType) {
	case brushes::ActiveBrush::CLASSIC:
		if(i == ERASER_SLOT_INDEX) {
			brush.classic().erase = true;
		}
		slot.setClassic(brush.classic());
		break;
	case brushes::ActiveBrush::MYPAINT:
		if(i == ERASER_SLOT_INDEX) {
			brush.myPaint().brush().erase = true;
		}
		slot.setMyPaint(brush.myPaint());
		break;
	default:
		qWarning("Invalid brush type %d", int(brush.activeType()));
		return brush;
	}

	slot.setActiveType(activeType);
	return brush;
}

brushes::ActiveBrush BrushSettings::currentBrush() const
{
	return d->currentBrush();
}

int BrushSettings::currentPresetId() const
{
	return d->currentPreset().id;
}

const QString &BrushSettings::currentPresetName() const
{
	return d->currentPreset().effectiveName();
}

const QString &BrushSettings::currentPresetDescription() const
{
	return d->currentPreset().effectiveDescription();
}

const QPixmap &BrushSettings::currentPresetThumbnail() const
{
	return d->currentPreset().effectiveThumbnail();
}

bool BrushSettings::isCurrentPresetAttached() const
{
	return d->currentPreset().attached;
}

void BrushSettings::clearCurrentDetachedPresetChanges() const
{
	Preset &preset = d->currentPreset();
	if(preset.valid && !preset.attached) {
		preset.changedName = {};
		preset.changedDescription = {};
		preset.changedThumbnail = {};
	}
}

int BrushSettings::currentBrushSlot() const
{
	return d->current;
}

void BrushSettings::selectBrushSlot(int i)
{
	if(i < 0 || i >= TOTAL_SLOT_COUNT) {
		qWarning("Slot index %d out of bounds", i);
		return;
	}

	if(i == d->brushSlotCount) {
		i = ERASER_SLOT_INDEX;
	} else if(i > d->brushSlotCount && i != ERASER_SLOT_INDEX) {
		qWarning("Slot index %d is not active (max %d)", i, d->brushSlotCount);
		return;
	}

	int previousSlot = d->current;
	d->buttonAt(i)->setChecked(true);
	d->current = i;

	if(!d->shareBrushSlotColor) {
		emit colorChanged(d->currentBrush().qColor());
		emit backgroundColorChanged(d->currentBackgroundColor());
	}

	bool inEraserSlot = i == ERASER_SLOT_INDEX;
	if(inEraserSlot) {
		setEraserMode(true);
	} else {
		updateUi();
	}

	if((previousSlot == ERASER_SLOT_INDEX) != inEraserSlot) {
		emit eraseModeChanged(inEraserSlot);
	}

	const Preset &preset = d->currentPreset();
	updateMenuActions();
	emit presetIdChanged(preset.id, preset.attached);
	updateChangesInCurrentBrushPreset();
}

void BrushSettings::toggleEraserMode()
{
	if(!isCurrentEraserSlot()) {
		// Eraser mode is fixed in dedicated eraser slot
		setEraserMode(!d->currentBrush().isEraser());
	}
}

void BrushSettings::toggleRecolorMode()
{
	if(!isCurrentEraserSlot()) {
		brushes::ActiveBrush &brush = d->currentBrush();
		switch(brush.activeType()) {
		case brushes::ActiveBrush::CLASSIC: {
			brushes::ClassicBrush &cb = brush.classic();
			if(cb.brush_mode == DP_BLEND_MODE_RECOLOR) {
				cb.brush_mode = DP_BLEND_MODE_NORMAL;
			} else {
				cb.brush_mode = DP_BLEND_MODE_RECOLOR;
			}
			break;
		}
		case brushes::ActiveBrush::MYPAINT: {
			DP_MyPaintBrush &mpb = brush.myPaint().brush();
			if(mpb.brush_mode == DP_BLEND_MODE_RECOLOR) {
				mpb.brush_mode = DP_BLEND_MODE_NORMAL;
			} else {
				mpb.brush_mode = DP_BLEND_MODE_RECOLOR;
			}
			break;
		}
		default:
			break;
		}
		updateUi();
	}
}

void BrushSettings::resetPreset()
{
	resetBrushInSlot(d->current);
}

void BrushSettings::resetPresetsInAllSlots()
{
	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		resetBrushInSlot(i);
	}
}

void BrushSettings::resetBrushInSlot(int i)
{
	Preset &preset = d->presetAt(i);
	if(preset.valid) {
		if(preset.attached) {
			preset.changedName = {};
			preset.changedDescription = {};
			preset.changedThumbnail = {};
			preset.changedBrush = {};
			setBrushPresetInSlot(preset, i);
		} else {
			setBrushDetachedInSlot(preset.originalBrush, i);
		}
	}
}

void BrushSettings::changeCurrentPresetName(const QString &name)
{
	Preset &preset = d->currentPreset();
	if(preset.changeName(name)) {
		d->ui.preview->setPresetChanged(preset.hasChanges());
		updateChangesInCurrentBrushPreset();
	}
}

void BrushSettings::changeCurrentPresetDescription(const QString &description)
{
	Preset &preset = d->currentPreset();
	if(preset.changeDescription(description)) {
		d->ui.preview->setPresetChanged(preset.hasChanges());
		updateChangesInCurrentBrushPreset();
	}
}

void BrushSettings::changeCurrentPresetThumbnail(const QPixmap &thumbnail)
{
	Preset &preset = d->currentPreset();
	if(preset.changeThumbnail(thumbnail)) {
		d->ui.preview->setPresetThumbnail(thumbnail);
		d->ui.preview->setPresetChanged(preset.hasChanges());
		updateChangesInCurrentBrushPreset();
	}
}

void BrushSettings::setEraserMode(bool erase)
{
	Q_ASSERT(erase || !isCurrentEraserSlot());

	brushes::ActiveBrush &brush = d->currentBrush();
	brushes::ClassicBrush &classic = brush.classic();
	DP_MyPaintBrush &mypaint = brush.myPaint().brush();
	classic.erase = erase;
	mypaint.erase = erase;

	if(!canvas::blendmode::isValidBrushMode(classic.brush_mode)) {
		qWarning(
			"setEraserMode(%d): wrong classic brush mode %d", erase,
			int(classic.brush_mode));
		classic.brush_mode = DP_BLEND_MODE_NORMAL;
	}
	if(!canvas::blendmode::isValidBrushMode(mypaint.brush_mode)) {
		qWarning(
			"setEraserMode(%d): wrong MyPaint brush mode %d", erase,
			int(mypaint.brush_mode));
		mypaint.brush_mode = DP_BLEND_MODE_NORMAL;
	}
	if(!canvas::blendmode::isValidEraseMode(classic.erase_mode)) {
		qWarning(
			"setEraserMode(%d): wrong classic erase mode %d", erase,
			int(classic.erase_mode));
		classic.erase_mode = DP_BLEND_MODE_ERASE;
	}
	if(!canvas::blendmode::isValidEraseMode(mypaint.erase_mode)) {
		qWarning(
			"setEraserMode(%d): wrong MyPaint erase mode %d", erase,
			int(mypaint.erase_mode));
		mypaint.erase_mode = DP_BLEND_MODE_ERASE;
	}

	updateUi();
}

void BrushSettings::selectEraserSlot(bool eraser)
{
	if(eraser) {
		if(!isCurrentEraserSlot()) {
			d->previousNonEraser = d->current;
			selectBrushSlot(ERASER_SLOT_INDEX);
		}
	} else {
		if(isCurrentEraserSlot()) {
			selectBrushSlot(
				d->previousNonEraser < d->brushSlotCount ? d->previousNonEraser
														 : 0);
		}
	}
}

void BrushSettings::selectNextSlot()
{
	int i;
	if(d->current == ERASER_SLOT_INDEX) {
		i = 0;
	} else {
		i = d->current + 1;
	}
	selectBrushSlot(i);
}

void BrushSettings::selectPreviousSlot()
{
	int i;
	if(d->current == 0) {
		i = d->brushSlotCount;
	} else if(d->current == ERASER_SLOT_INDEX) {
		i = d->brushSlotCount - 1;
	} else {
		i = d->current - 1;
	}
	selectBrushSlot(i);
}

void BrushSettings::swapWithSlot(int i)
{
	Q_ASSERT(i >= 0);
	Q_ASSERT(i < ERASER_SLOT_INDEX);
	if(i != d->current && !isCurrentEraserSlot()) {
		std::swap(d->brushAt(d->current), d->brushAt(i));
		std::swap(d->presetAt(d->current), d->presetAt(i));
		updateUi();

		const Preset &preset = d->currentPreset();
		changePresetBrush(d->currentBrush());
		updateMenuActions();
		emit presetIdChanged(preset.id, preset.attached);
	}
}

void BrushSettings::setGlobalSmoothing(int smoothing)
{
	QSignalBlocker blocker{d->ui.smoothingBox};
	d->ui.smoothingBox->setValue(
		d->ui.smoothingBox->value() - d->globalSmoothing + smoothing);
	d->globalSmoothing = smoothing;
}

bool BrushSettings::isCurrentEraserSlot() const
{
	return d->current == ERASER_SLOT_INDEX;
}

void BrushSettings::changeBrushType(const QAction *action)
{
	if(action == d->brushTypePixelRoundAction) {
		d->brushType = BrushType::PixelRound;
	} else if(action == d->brushTypePixelSquareAction) {
		d->brushType = BrushType::PixelSquare;
	} else if(action == d->brushTypeSoftRoundAction) {
		d->brushType = BrushType::SoftRound;
	} else if(action == d->brushTypeMyPaintAction) {
		d->brushType = BrushType::MyPaint;
	} else {
		qWarning("Unknown brush type selected");
	}
	updateFromUiWith(false);
	updateUi();
}

void BrushSettings::changePaintMode(const QAction *action)
{
	if(action == d->paintModeDirectAction) {
		d->paintMode = DP_PAINT_MODE_DIRECT;
	} else if(action == d->paintModeIndirectWashAction) {
		d->paintMode = DP_PAINT_MODE_INDIRECT_WASH;
	} else if(action == d->paintModeIndirectSoftAction) {
		d->paintMode = DP_PAINT_MODE_INDIRECT_SOFT;
	} else if(action == d->paintModeIndirectNormalAction) {
		d->paintMode = DP_PAINT_MODE_INDIRECT_NORMAL;
	} else {
		qWarning("Unknown paint mode selected");
	}
	updateFromUi();
	updateUi();
}

void BrushSettings::changeSizeSetting(int size)
{
	if(d->currentBrush().activeType() == brushes::ActiveBrush::CLASSIC) {
		emit pixelSizeChanged(clampBrushSize(size));
	}
}

void BrushSettings::changeRadiusLogarithmicSetting(int radiusLogarithmic)
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	if(brush.activeType() == brushes::ActiveBrush::MYPAINT) {
		int size = qRound(myPaintRadiusToPixelSize(brush.myPaint().maxSizeFor(
			radiusLogarithmicToMyPaintRadius(radiusLogarithmic))));
		emit pixelSizeChanged(clampBrushSize(size));
	}
}

void BrushSettings::updateMenuActions()
{
	const Preset &preset = d->currentPreset();
	bool attached = preset.isAttached();
	d->resetBrushAction->setEnabled(preset.valid);
	d->deleteBrushAction->setEnabled(attached);
	d->detachBrushAction->setEnabled(attached);
}

static void setSliderFromMyPaintSetting(
	KisSliderSpinBox *slider, const DP_MyPaintSettings &myPaintSettings,
	MyPaintBrushSetting setting, bool adjustMinToZero = false)
{
	qreal value = myPaintSettings.mappings[setting].base_value;
	// The radius setting starts at -2, but we don't want to present it that way
	// to the user and confuse them horribly. So we offset by the minimum.
	if(adjustMinToZero) {
		value -= mypaint_brush_setting_info(setting)->min;
	}
	slider->setValue(qRound(value * 100.0));
}

static void setMyPaintSettingFromSlider(
	const KisSliderSpinBox *slider, DP_MyPaintSettings &myPaintSettings,
	MyPaintBrushSetting setting, bool adjustMinToZero = false)
{
	float value = slider->value() / 100.0f;
	if(adjustMinToZero) {
		value += mypaint_brush_setting_info(setting)->min;
	}
	myPaintSettings.mappings[setting].base_value = value;
}

void BrushSettings::setAutomaticAlphaPreserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != d->automaticAlphaPreserve) {
		d->automaticAlphaPreserve = automaticAlphaPreserve;
		const brushes::ActiveBrush &brush = d->currentBrush();
		if(brush.activeType() == brushes::ActiveBrush::CLASSIC) {
			const brushes::ClassicBrush &cb = brush.classic();
			int blendMode = getBlendMode();
			QSignalBlocker blocker(d->ui.blendmode);
			d->ui.blendmode->setModel(
				getBrushBlendModesFor(cb.erase, automaticAlphaPreserve));
			int index = searchBlendModeComboIndex(d->ui.blendmode, blendMode);
			if(index != -1) {
				d->ui.blendmode->setCurrentIndex(index);
			}
			d->ui.alphaPreserve->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(blendMode));
		} else {
			QSignalBlocker blocker(d->ui.blendmode);
			d->ui.blendmode->setModel(
				getBrushBlendModesFor(false, automaticAlphaPreserve));
		}
	}
}

void BrushSettings::setPaintModeInUi(int paintMode)
{
	QAction *action;
	switch(paintMode) {
	case DP_PAINT_MODE_DIRECT:
		action = d->paintModeDirectAction;
		break;
	case DP_PAINT_MODE_INDIRECT_WASH:
		action = d->paintModeIndirectWashAction;
		break;
	case DP_PAINT_MODE_INDIRECT_SOFT:
		action = d->paintModeIndirectSoftAction;
		break;
	case DP_PAINT_MODE_INDIRECT_NORMAL:
		action = d->paintModeIndirectNormalAction;
		break;
	default:
		qWarning("Unknown paint mode %d", paintMode);
		return;
	}
	d->paintMode = DP_PaintMode(paintMode);
	d->ui.paintMode->setIcon(action->icon());
	d->ui.paintMode->setStatusTip(action->text());
	d->ui.paintMode->setToolTip(action->text());
}

void BrushSettings::updateAlphaPreserve(bool alphaPreserve)
{
	if(!d->updateInProgress) {
		if(!d->currentIsMyPaint()) {
			int blendMode = getBlendMode();
			canvas::blendmode::adjustAlphaBehavior(blendMode, alphaPreserve);
			int index = searchBlendModeComboIndex(d->ui.blendmode, blendMode);
			if(index != -1) {
				QSignalBlocker blocker(d->ui.blendmode);
				d->ui.blendmode->setCurrentIndex(index);
			}
		}
		updateFromUi();
	}
}

void BrushSettings::updateBlendMode(int index)
{
	if(!d->updateInProgress) {
		int blendMode = d->ui.blendmode->itemData(index).toInt();
		if(d->automaticAlphaPreserve) {
			QSignalBlocker blocker(d->ui.alphaPreserve);
			d->ui.alphaPreserve->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(blendMode));
		} else {
			canvas::blendmode::adjustAlphaBehavior(
				blendMode, d->ui.alphaPreserve->isChecked());
		}

		brushes::ActiveBrush &brush = d->currentBrush();
		brush.setBlendMode(blendMode, brush.isEraser());
		updateUi();
	}
}

void BrushSettings::updateUi()
{
	// Update the UI to match the currently selected brush
	if(d->updateInProgress)
		return;

	d->updateInProgress = true;
	d->finishStrokesAction->setChecked(d->finishStrokes);
	d->useBrushSampleCountAction->setChecked(d->useBrushSampleCount);

	const brushes::ActiveBrush &brush = d->currentBrush();
	const brushes::ClassicBrush &classic = brush.classic();
	const brushes::MyPaintBrush &myPaint = brush.myPaint();

	// Select brush type
	const bool mypaintmode =
		brush.activeType() == brushes::ActiveBrush::MYPAINT;
	const bool softmode =
		mypaintmode || classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;

	if(mypaintmode) {
		d->brushType = BrushType::MyPaint;
		d->ui.brushTypeButton->setIcon(d->brushTypeMyPaintAction->icon());
	} else {
		switch(classic.shape) {
		case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
			d->brushType = BrushType::PixelRound;
			d->ui.brushTypeButton->setIcon(
				d->brushTypePixelRoundAction->icon());
			break;
		case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
			d->brushType = BrushType::PixelSquare;
			d->ui.brushTypeButton->setIcon(
				d->brushTypePixelSquareAction->icon());
			break;
		default:
			d->brushType = BrushType::SoftRound;
			d->ui.brushTypeButton->setIcon(d->brushTypeSoftRoundAction->icon());
			break;
		}
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());
	adjustSettingVisibilities(softmode, mypaintmode);

	// Show correct blending mode
	d->ui.blendmode->setModel(
		getBrushBlendModesFor(brush.isEraser(), d->automaticAlphaPreserve));
	d->ui.modeEraser->setChecked(brush.isEraser());
	d->ui.modeEraser->setEnabled(!isCurrentEraserSlot());

	int blendMode = brush.blendMode();
	d->ui.modeColorpick->setEnabled(blendMode != DP_BLEND_MODE_ERASE);
	int blendModeIndex = searchBlendModeComboIndex(d->ui.blendmode, blendMode);
	if(blendModeIndex != -1) {
		d->ui.blendmode->setCurrentIndex(blendModeIndex);
	}
	d->ui.alphaPreserve->setChecked(
		canvas::blendmode::presentsAsAlphaPreserving(blendMode));
	setPaintModeInUi(int(brush.paintMode()));

	// Set UI elements that are distinct between classic and MyPaint brushes
	// unconditionally, the ones that are shared only for the active type.
	// This is so that when loading a brush from settings, the hidden elements
	// don't get left in their default state.
	d->ui.brushsizeBox->setValue(classic.size.max);
	d->ui.pressureSize->setChecked(
		classic.size_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.pressureOpacity->setChecked(
		classic.opacity_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.smudgingBox->setValue(qRound(classic.smudge.max * 100.0));
	d->ui.pressureSmudging->setChecked(
		classic.smudge_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.colorpickupBox->setValue(classic.resmudge);
	d->ui.brushspacingBox->setValue(qRound(classic.spacing * 100.0));
	d->ui.modeColorpick->setChecked(classic.colorpick);
	if(softmode) {
		d->ui.pressureHardness->setChecked(
			classic.hardness_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	}

	const DP_MyPaintSettings &myPaintSettings = myPaint.constSettings();
	setSliderFromMyPaintSetting(
		d->ui.radiusLogarithmicBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, true);
	setSliderFromMyPaintSetting(
		d->ui.gainBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG);

	bool canChangePaintMode =
		!isLocked() && !canvas::blendmode::directOnly(blendMode);
	if(mypaintmode) {
		setSliderFromMyPaintSetting(
			d->ui.opacityBox, myPaintSettings, MYPAINT_BRUSH_SETTING_OPAQUE);
		setSliderFromMyPaintSetting(
			d->ui.hardnessBox, myPaintSettings, MYPAINT_BRUSH_SETTING_HARDNESS);
	} else {
		d->ui.opacityBox->setValue(qRound(classic.opacity.max * 100.0));
		d->ui.hardnessBox->setValue(qRound(classic.hardness.max * 100.0));
		// Smudging only works right in incremental mode
		if(classic.smudge.max != 0.0) {
			canChangePaintMode = false;
		}
	}
	d->ui.paintMode->setEnabled(canChangePaintMode);
	d->ui.paintMode->setVisible(canChangePaintMode);

	if(d->useBrushSampleCount) {
		brushes::StabilizationMode stabilizationMode =
			brush.stabilizationMode();
		d->stabilizerAction->setChecked(
			stabilizationMode == brushes::Stabilizer);
		d->smoothingAction->setChecked(stabilizationMode == brushes::Smoothing);
		d->ui.stabilizerBox->setValue(brush.stabilizerSampleCount());
		d->ui.smoothingBox->setValue(brush.smoothing() + d->globalSmoothing);
	}

	// Communicate the current size of the brush cursor to the outside. These
	// functions check for their applicability themselves, so we just call both.
	changeSizeSetting(d->ui.brushsizeBox->value());
	changeRadiusLogarithmicSetting(d->ui.radiusLogarithmicBox->value());
	updateStabilizationSettingVisibility();
	updateRadiusLogarithmicLimit();
	emitBlendModeChanged();
	emitBrushModeChanged();

	d->updateInProgress = false;
	d->ui.preview->setBrush(d->currentBrush());

	const Preset &preset = d->currentPreset();
	if(preset.isAttached()) {
		d->ui.preview->setPreset(
			preset.effectiveThumbnail(), preset.hasChanges());
	} else {
		d->ui.preview->clearPreset();
	}

	pushSettings();
}

void BrushSettings::updateFromUi()
{
	updateFromUiWith(true);
}

void BrushSettings::updateFromUiWith(bool updateShared)
{
	if(d->updateInProgress)
		return;

	d->finishStrokes = d->finishStrokesAction->isChecked();
	d->useBrushSampleCount = d->useBrushSampleCountAction->isChecked();

	bool mypaintmode = d->brushType == BrushType::MyPaint;
	d->currentBrush().setActiveType(
		mypaintmode ? brushes::ActiveBrush::MYPAINT
					: brushes::ActiveBrush::CLASSIC);

	// Copy changes from the UI to the brush properties object,
	// then update the brush
	brushes::ActiveBrush &brush = d->currentBrush();
	brushes::ClassicBrush &classic = brush.classic();
	brushes::MyPaintBrush &myPaint = brush.myPaint();

	switch(d->brushType) {
	case BrushType::PixelRound:
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND;
		break;
	case BrushType::PixelSquare:
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
		break;
	default:
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;
		break;
	}

	classic.size.max = d->ui.brushsizeBox->value();
	classic.size_dynamic.type = d->ui.pressureSize->isChecked()
									? classic.lastSizeDynamicType()
									: DP_CLASSIC_BRUSH_DYNAMIC_NONE;

	classic.smudge.max = d->ui.smudgingBox->value() / 100.0;
	classic.smudge_dynamic.type = d->ui.pressureSmudging->isChecked()
									  ? classic.lastSmudgeDynamicType()
									  : DP_CLASSIC_BRUSH_DYNAMIC_NONE;
	classic.resmudge = d->ui.colorpickupBox->value();

	classic.spacing = d->ui.brushspacingBox->value() / 100.0;
	classic.colorpick = d->ui.modeColorpick->isChecked();

	DP_MyPaintSettings &myPaintSettings = myPaint.settings();
	setMyPaintSettingFromSlider(
		d->ui.radiusLogarithmicBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, true);
	setMyPaintSettingFromSlider(
		d->ui.gainBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG);

	// We want to keep MyPaint and classic brush opacity and hardness separate,
	// since they work so differently from each other. So this will be false
	// when switching types as to not overwrite the values with each other.
	if(updateShared) {
		int blendMode = d->ui.blendmode->currentData(Qt::UserRole).toInt();
		canvas::blendmode::adjustAlphaBehavior(
			blendMode, d->ui.alphaPreserve->isChecked());
		brush.setBlendMode(blendMode, brush.isEraser());
		brush.setPaintMode(int(d->paintMode));
		bool canChangePaintMode =
			!isLocked() && !canvas::blendmode::directOnly(blendMode);
		if(mypaintmode) {
			setMyPaintSettingFromSlider(
				d->ui.opacityBox, myPaintSettings,
				MYPAINT_BRUSH_SETTING_OPAQUE);
			setMyPaintSettingFromSlider(
				d->ui.hardnessBox, myPaintSettings,
				MYPAINT_BRUSH_SETTING_HARDNESS);
		} else {
			classic.opacity.max = d->ui.opacityBox->value() / 100.0;
			classic.opacity_dynamic.type =
				d->ui.pressureOpacity->isChecked()
					? classic.lastOpacityDynamicType()
					: DP_CLASSIC_BRUSH_DYNAMIC_NONE;
			classic.hardness.max = d->ui.hardnessBox->value() / 100.0;
			if(classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND) {
				classic.hardness_dynamic.type =
					d->ui.pressureHardness->isChecked()
						? classic.lastHardnessDynamicType()
						: DP_CLASSIC_BRUSH_DYNAMIC_NONE;
			}
			// Smudging only works right in incremental mode
			if(classic.smudge.max != 0.0) {
				canChangePaintMode = false;
			}
		}
		d->ui.paintMode->setEnabled(canChangePaintMode);
		d->ui.paintMode->setVisible(canChangePaintMode);
	}

	if(d->useBrushSampleCount) {
		brush.setStabilizationMode(d->stabilizationMode());
		brush.setStabilizerSampleCount(d->ui.stabilizerBox->value());
		brush.setSmoothing(d->ui.smoothingBox->value() - d->globalSmoothing);
	}

	d->ui.preview->setBrush(brush);

	changePresetBrush(brush);
	pushSettings();

	adjustSettingVisibilities(
		mypaintmode || classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND,
		mypaintmode);
	emitBlendModeChanged();
	emitBrushModeChanged();
}

void BrushSettings::changePresetBrush(const brushes::ActiveBrush &brush)
{
	Preset &preset = d->currentPreset();
	bool inEraserSlot = isCurrentEraserSlot();
	preset.changeBrush(brush, inEraserSlot);
	if(preset.isAttached()) {
		d->ui.preview->setPresetChanged(preset.hasChanges());
	}
	updateChangesInCurrentBrushPreset();
}

void BrushSettings::updateChangesInCurrentBrushPreset()
{
	updateChangesInBrushPresetInSlot(d->current);
}

void BrushSettings::updateChangesInBrushPresetInSlot(int i)
{
	const Preset &preset = d->presetAt(i);
	if(d->brushPresets && preset.isAttached()) {
		d->brushPresets->changePreset(
			preset.id, preset.changedName, preset.changedDescription,
			preset.changedThumbnail, preset.changedBrush,
			i == ERASER_SLOT_INDEX);
	}
}

void BrushSettings::updateStabilizationSettingVisibility()
{
	brushes::StabilizationMode stabilizationMode = d->stabilizationMode();
	d->ui.stabilizerBox->setVisible(false);
	d->ui.smoothingBox->setVisible(false);
	d->ui.stabilizerBox->setVisible(stabilizationMode == brushes::Stabilizer);
	d->ui.smoothingBox->setVisible(stabilizationMode == brushes::Smoothing);
}

void BrushSettings::adjustSettingVisibilities(bool softmode, bool mypaintmode)
{
	utils::ScopedUpdateDisabler disabler(getUi());
	Lock lock = getLock();
	bool locked = lock != Lock::None;
	QPair<QWidget *, bool> widgetVisibilities[] = {
		{d->ui.modeColorpick, !locked && !mypaintmode},
		{d->ui.alphaPreserve, !locked},
		{d->ui.modeEraser, !locked},
		{d->ui.paintMode, d->ui.paintMode->isEnabled() && !locked},
		{d->ui.blendmode, !locked},
		{d->ui.pressureHardness, !locked && softmode && !mypaintmode},
		{d->ui.hardnessBox, !locked && softmode},
		{d->ui.brushsizeBox, !locked && !mypaintmode},
		{d->ui.pressureSize, !locked && !mypaintmode},
		{d->ui.radiusLogarithmicBox, !locked && mypaintmode},
		{d->ui.opacityBox, !locked},
		{d->ui.pressureOpacity, !mypaintmode && !locked},
		{d->ui.smudgingBox, !locked && !mypaintmode},
		{d->ui.pressureSmudging, !locked && !mypaintmode},
		{d->ui.colorpickupBox, !locked && !mypaintmode},
		{d->ui.brushspacingBox, !locked && !mypaintmode},
		{d->ui.gainBox, !locked && mypaintmode},
		{d->ui.stabilizationWrapper, !locked},
		{d->ui.stabilizerButton, !locked},
	};

	d->ui.preview->setDisabled(locked);
	d->ui.noPermissionLabel->setVisible(locked);
	if(locked) {
		d->ui.noPermissionLabel->setText(getLockDescription(lock));
	}

	// To avoid the dock resizing itself to demand more space than it actually
	// requires, first do the hiding, then the showing. That way we don't end up
	// in a temporary state where we got too many widgets at once. It seems like
	// disabling updates should prevent this, but it doesn't actually.
	for(const QPair<QWidget *, bool> &pair : widgetVisibilities) {
		if(!pair.second) {
			pair.first->hide();
		}
	}
	for(const QPair<QWidget *, bool> &pair : widgetVisibilities) {
		if(pair.second) {
			pair.first->show();
		}
	}
}

void BrushSettings::emitBlendModeChanged()
{
	int currentBlendMode = getBlendMode();
	if(d->previousBlendMode != currentBlendMode) {
		d->previousBlendMode = currentBlendMode;
		emit blendModeChanged(currentBlendMode);
	}
}

void BrushSettings::emitBrushModeChanged()
{
	BrushMode currentBrushMode = getBrushMode();
	if(d->previousBrushMode != currentBrushMode) {
		d->previousBrushMode = currentBrushMode;
		emit brushModeChanged(currentBrushMode);
	}
}

void BrushSettings::pushSettings()
{
	tools::ToolController *ctrl = controller();
	ctrl->setActiveBrush(d->ui.preview->brush());
	ctrl->setFinishStrokes(d->finishStrokes);
	ctrl->setStabilizationMode(d->stabilizationMode());
	ctrl->setStabilizerSampleCount(d->ui.stabilizerBox->value());
	ctrl->setSmoothing(d->ui.smoothingBox->value() - d->globalSmoothing);
	ctrl->setStabilizerUseBrushSampleCount(d->useBrushSampleCount);
}

namespace toolprop {
static const ToolProperties::RangedValue<int>
	activeSlot =
		{QStringLiteral("active"), 0, 0, BrushSettings::TOTAL_SLOT_COUNT - 1},
	stabilizationMode{
		QString("stabilizationmode"), 0, 0,
		int(brushes::LastStabilizationMode)},
	stabilizer = {QStringLiteral("stabilizer"), 0, 0, 1000},
	smoothing = {
		QStringLiteral("smoothing"), 0, 0, libclient::settings::maxSmoothing};
static const ToolProperties::Value<bool>
	finishStrokes = {QStringLiteral("finishstrokes"), true},
	useBrushSampleCount = {QStringLiteral("usebrushsamplecount"), true};
// Plus some dynamic tool properties for the brush slots.
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());

	cfg.setValue(toolprop::activeSlot, d->current);
	cfg.setValue(toolprop::finishStrokes, d->finishStrokes);
	cfg.setValue(toolprop::useBrushSampleCount, d->useBrushSampleCount);
	cfg.setValue(toolprop::stabilizationMode, int(d->stabilizationMode()));
	cfg.setValue(toolprop::stabilizer, d->ui.stabilizerBox->value());
	cfg.setValue(
		toolprop::smoothing, d->ui.smoothingBox->value() - d->globalSmoothing);

	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		int cfgIndex = translateBrushSlotConfigIndex(i);
		cfg.setValue(
			ToolProperties::Value<QByteArray>{
				QStringLiteral("brush%1").arg(cfgIndex), QByteArray()},
			d->brushAt(i).toJson(true));

		const Preset &preset = d->presetAt(i);
		cfg.setValue(
			ToolProperties::Value<QByteArray>{
				QStringLiteral("last%1").arg(cfgIndex), QByteArray()},
			preset.valid ? preset.originalBrush.toJson() : QByteArray());
		cfg.setValue(
			ToolProperties::Value<int>{
				QStringLiteral("preset%1").arg(cfgIndex), 0},
			preset.valid && (preset.attached || preset.reattach) ? preset.id
																 : 0);

		cfg.setValue(
			ToolProperties::Value<QColor>{
				QStringLiteral("bg%1").arg(cfgIndex), QColor(Qt::white)},
			d->backgroundColorAt(i));
	}

	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{
	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		brushes::ActiveBrush &brush = d->brushAt(i);
		Preset &preset = d->presetAt(i);
		QColor &backgroundColor = d->backgroundColorAt(i);

		int cfgIndex = translateBrushSlotConfigIndex(i);
		QByteArray brushData = cfg.value(ToolProperties::Value<QByteArray>{
			QStringLiteral("brush%1").arg(cfgIndex), QByteArray()});

		const QJsonObject o =
			QJsonDocument::fromJson(
				brushData.isEmpty() ? getDefaultBrushForSlot(i) : brushData)
				.object();
		brush = brushes::ActiveBrush::fromJson(o, true);

		const QJsonObject lo =
			QJsonDocument::fromJson(cfg.value(ToolProperties::Value<QByteArray>{
										QStringLiteral("last%1").arg(cfgIndex),
										getDefaultBrushForSlot(i)}))
				.object();
		if(!lo.isEmpty()) {
			preset = Preset::makeDetached(brushes::ActiveBrush::fromJson(lo));
		}

		preset.id = cfg.value(ToolProperties::Value<int>{
			QStringLiteral("preset%1").arg(cfgIndex), 0});
		if(brushData.isEmpty() && preset.id == 0) {
			preset.id = getDefaultPresetIdForSlot(i);
			preset.overwrite = true;
		}

		backgroundColor = cfg.value(ToolProperties::Value<QColor>{
			QStringLiteral("bg%1").arg(cfgIndex), QColor(Qt::white)});

		if(!d->shareBrushSlotColor) {
			d->buttonAt(i)->setColorSwatch(brush.qColor());
			d->buttonAt(i)->setBackgroundSwatch(backgroundColor);
		}
	}

	brushes::ActiveBrush &eraser = d->eraserSlot.brush;
	eraser.classic().erase = true;
	eraser.myPaint().brush().erase = true;

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = isCurrentEraserSlot() ? 0 : d->current;
	d->finishStrokes = cfg.value(toolprop::finishStrokes);
	d->useBrushSampleCount = cfg.value(toolprop::useBrushSampleCount);
	if(!d->useBrushSampleCount) {
		if(cfg.value(toolprop::stabilizationMode) == brushes::Smoothing) {
			d->smoothingAction->setChecked(true);
		} else {
			d->stabilizerAction->setChecked(true);
		}
		d->ui.stabilizerBox->setValue(cfg.value(toolprop::stabilizer));
		d->ui.smoothingBox->setValue(
			cfg.value(toolprop::smoothing) + d->globalSmoothing);
	}
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::LINE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_LINE);
		break;
	case tools::Tool::RECTANGLE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_RECTANGLE);
		break;
	case tools::Tool::ELLIPSE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_ELLIPSE);
		break;
	default:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_STROKE);
		break;
	}

	if(tool == tools::Tool::ERASER) {
		selectEraserSlot(true);
	} else {
		selectEraserSlot(false);
	}
}

void BrushSettings::setForeground(const QColor &color)
{
	if(color != d->currentBrush().qColor()) {
		if(d->shareBrushSlotColor) {
			for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
				d->brushAt(i).setQColor(color);
			}
		} else {
			Q_ASSERT(d->current >= 0 && d->current < TOTAL_SLOT_COUNT);
			Slot &slot = d->currentSlot();
			slot.brush.setQColor(color);
			slot.button->setColorSwatch(color);
		}
		d->ui.preview->setBrush(d->currentBrush());
		pushSettings();
	}
}

void BrushSettings::setBackground(const QColor &color)
{
	if(color != d->currentBackgroundColor()) {
		if(d->shareBrushSlotColor) {
			for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
				d->backgroundColorAt(i) = color;
			}
		} else {
			Q_ASSERT(d->current >= 0 && d->current < TOTAL_SLOT_COUNT);
			Slot &slot = d->currentSlot();
			slot.backgroundColor = color;
			slot.button->setBackgroundSwatch(color);
		}
		pushSettings();
	}
}

void BrushSettings::quickAdjust1(qreal adjustment)
{
	// Adjust the currently visible box. Classic brush size gets increased
	// exponentially, MyPaint brush size is already logarithmic.
	if(d->currentIsMyPaint()) {
		quickAdjustOn(d->ui.radiusLogarithmicBox, adjustment * 2.0);
	} else {
		KisSliderSpinBox *brushsizeBox = d->ui.brushsizeBox;
		quickAdjustOn(
			brushsizeBox,
			qMax(1.0, std::cbrt(brushsizeBox->value())) * adjustment);
	}
}

void BrushSettings::stepAdjust1(bool increase)
{
	if(d->currentIsMyPaint()) {
		KisSliderSpinBox *radiusLogarithmicBox = d->ui.radiusLogarithmicBox;
		adjustSizeSlider(
			radiusLogarithmicBox, stepLinear(
									  radiusLogarithmicBox->minimum(),
									  radiusLogarithmicBox->maximum(),
									  radiusLogarithmicBox->value(), increase));
	} else {
		KisSliderSpinBox *brushsizeBox = d->ui.brushsizeBox;
		adjustSizeSlider(
			brushsizeBox, stepLogarithmic(
							  brushsizeBox->minimum(), brushsizeBox->maximum(),
							  brushsizeBox->value(), increase));
	}
}

void BrushSettings::quickAdjustOn(KisSliderSpinBox *box, qreal adjustment)
{
	d->quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(d->quickAdjust1, &i);
	if(int(i)) {
		d->quickAdjust1 = f;
		adjustSizeSlider(box, box->value() + int(i));
	}
}

void BrushSettings::handlePresetChanged(
	int presetId, const QString &name, const QString &description,
	const QPixmap &thumbnail, const brushes::ActiveBrush &brush)
{
	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		Preset &preset = d->presetAt(i);
		if(preset.isAttached() && preset.id == presetId) {
			preset.originalName = name;
			preset.originalDescription = description;
			preset.originalThumbnail = thumbnail;
			preset.originalBrush = brush;
			if(preset.changedName.has_value()) {
				preset.changeName(preset.changedName.value());
			}
			if(preset.changedDescription.has_value()) {
				preset.changeDescription(preset.changedDescription.value());
			}
			if(preset.changedThumbnail.has_value()) {
				preset.changeThumbnail(preset.changedThumbnail.value());
			}
			preset.changeBrush(d->brushAt(i), i == ERASER_SLOT_INDEX);
			if(i == d->current) {
				d->ui.preview->setPreset(thumbnail, preset.hasChanges());
			}
		}
	}
}

void BrushSettings::handlePresetRemoved(int presetId)
{
	for(int i = 0; i < TOTAL_SLOT_COUNT; ++i) {
		Preset &preset = d->presetAt(i);
		if(preset.valid && preset.id == presetId) {
			preset.id = 0;
			preset.attached = false;
			preset.reattach = false;
			if(i == d->current) {
				d->ui.preview->clearPreset();
				updateMenuActions();
			}
		}
	}
}

void BrushSettings::detachCurrentSlot()
{
	Preset &preset = d->currentPreset();
	if(preset.isAttached()) {
		preset.attached = false;
		d->ui.preview->clearPreset();
		updateMenuActions();
		emit presetIdChanged(preset.id, false);
	}
}

int BrushSettings::getSize() const
{
	if(d->currentIsMyPaint()) {
		return radiusLogarithmicToPixelSize(
			d->ui.radiusLogarithmicBox->value());
	} else {
		return d->ui.brushsizeBox->value();
	}
}

bool BrushSettings::getSubpixelMode() const
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	return brush.activeType() != brushes::ActiveBrush::CLASSIC ||
		   brush.classic().shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;
}

bool BrushSettings::isSquare() const
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	return brush.activeType() == brushes::ActiveBrush::CLASSIC &&
		   brush.classic().shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
}

int BrushSettings::getBlendMode() const
{
	return d->currentBrush().blendMode();
}

BrushSettings::BrushMode BrushSettings::getBrushMode() const
{
	if(isCurrentEraserSlot()) {
		return UnknownMode;
	} else {
		const brushes::ActiveBrush &brush = d->currentBrush();
		if(brush.isEraser()) {
			return EraseMode;
		} else if(canvas::blendmode::preservesAlpha(brush.blendMode())) {
			return AlphaLockMode;
		} else {
			return NormalMode;
		}
	}
}

void BrushSettings::resetBrushMode()
{
	brushes::ActiveBrush &brush = d->currentBrush();
	brush.setEraser(false);
	brush.setBlendMode(DP_BLEND_MODE_NORMAL, false);
	updateUi();
}

void BrushSettings::triggerUpdate()
{
	emit blendModeChanged(getBlendMode());
	emit brushModeChanged(getBrushMode());
}

void BrushSettings::updateRadiusLogarithmicLimit()
{
	KisSliderSpinBox *radiusLogarithmicBox = d->ui.radiusLogarithmicBox;
	int brushSizeLimit = d->brushSizeLimit;
	if(brushSizeLimit < 0) {
		radiusLogarithmicBox->setSoftRange(0, 0);
		radiusLogarithmicBox->setSuffix(QString());
	} else {
		int minimum = radiusLogarithmicBox->minimum();
		int radiusLogarithmicLimit = qMax(
			minimum, qRound(myPaintRadiusToRadiusLogarithmic(
						 d->currentBrush().myPaint().baseValueForMaxSize(
							 pixelSizeToMyPaintRadius(brushSizeLimit)))));
		radiusLogarithmicBox->setSoftRange(minimum, radiusLogarithmicLimit);
		//: Limit suffix for the brush radius slider, %1 is the size limit. So
		//: it will look something like "200/400". Unless your language uses a
		//: different slash symbol or something, leave this unchanged.
		radiusLogarithmicBox->setSuffix(tr("/%1").arg(radiusLogarithmicLimit));
	}
}

void BrushSettings::adjustSizeSlider(KisSliderSpinBox *slider, int value)
{
	if(slider->isSoftRangeActive()) {
		slider->setValue(
			qBound(slider->softMinimum(), value, slider->softMaximum()));
	} else {
		slider->setValue(value);
	}
}

int BrushSettings::clampBrushSize(int size) const
{
	int limit = d->brushSizeLimit;
	return qMin(size, limit < 0 ? DP_BRUSH_SIZE_MAX : limit);
}

float BrushSettings::myPaintRadiusToRadiusLogarithmic(float myPaintRadius)
{
	return (myPaintRadius * 100.0f) + 200.0f;
}

float BrushSettings::radiusLogarithmicToMyPaintRadius(int radiusLogarithmic)
{
	return float(radiusLogarithmic - 200) / 100.0f;
}

float BrushSettings::myPaintRadiusToPixelSize(float myPaintRadius)
{
	return std::exp(myPaintRadius) * 2.0f;
}

float BrushSettings::pixelSizeToMyPaintRadius(float pixelSize)
{
	return std::log(pixelSize / 2.0f);
}

double BrushSettings::radiusLogarithmicToPixelSize(int radiusLogarithmic)
{
	return std::exp(radiusLogarithmic / 100.0 - 2.0) * 2.0;
}

int BrushSettings::translateBrushSlotConfigIndex(int i)
{
	// There used to be a limit of 5 brush slots, with the eraser at
	// index 6. To avoid doing settings migrations and retain backward
	// compatibility, we swap the eraser and slot 6.
	switch(i) {
	case 6:
		return ERASER_SLOT_INDEX;
	case ERASER_SLOT_INDEX:
		return 6;
	default:
		return i;
	}
}

int BrushSettings::getDefaultPresetIdForSlot(int i)
{
	switch(i) {
	case 0:
		return 198; // Drawpile Sketch
	case 1:
		return 201; // Drawpile Pixel 1
	case 2:
		return 137; // MyPaint Ramon Sketch 1
	case 3:
		return 5; // MyPaint Classic Knife
	case 4:
		return 51; // MyPaint Deevad Thin Glazing
	case 5:
		return 142; // MyPaint Ramon PenBrush
	case 6:
		return 172; // MyPaint Tanda Charcoal 03
	case 7:
		return 34; // MyPaint Classic Smudge
	case 8:
		return 126; // MyPaint Kaerhorn Classic SK
	case ERASER_SLOT_INDEX:
		return 200; // Drawpile Paint 2
	default:
		qWarning("Unknown slot for default preset %d", i);
		return 0;
	}
}

QByteArray BrushSettings::getDefaultBrushForSlot(int i)
{
	switch(i) {
	case 0:
	case 5:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.800000011920929,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":4,\"sizecurve\":\"0,0;0.25,0.0625;0.5,0.25;0."
			   "75,0.5625;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 1:
	case 6:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":1,\"hardcurve\":\"0,0;1,1;"
			   "\",\"hardp\":true,\"opacity\":1,\"opacitycurve\":\"0,0;1,1;\","
			   "\"resmudge\":1,\"shape\":\"round-soft\",\"size\":3,"
			   "\"sizecurve\":\"0,0;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.5,"
			   "\"stabilizationmode\":0,\"stabilizer\":100},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 2:
	case 7:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.699999988079071,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":20,\"sizecurve\":\"0,0;1,1;\",\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.20000000298023224,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 3:
	case 8:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.8999999761581421,"
			   "\"hardcurve\":\"0,0;1,1;\",\"indirect\":true,\"opacity\":1,"
			   "\"opacitycurve\":\"0,0;1,1;\",\"opacityp\":true,\"resmudge\":1,"
			   "\"shape\":\"round-soft\",\"size\":20,\"sizecurve\":\"0,0;1,1;"
			   "\",\"smoothing\":0,\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0."
			   "05000000074505806,\"stabilizationmode\":0,\"stabilizer\":0},"
			   "\"type\":\"dp-classic\",\"version\":1}";
	case 4:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.8899999856948853,"
			   "\"hardcurve\":\"0,0;1,1;\",\"indirect\":true,\"opacity\":1,"
			   "\"opacitycurve\":\"0,0;1,1;\",\"opacityp\":true,\"resmudge\":1,"
			   "\"shape\":\"round-pixel\",\"size\":4,\"size2\":1,\"sizecurve\":"
			   "\"0,0;1,1;\",\"sizep\":true,\"smoothing\":0,\"smudgecurve\":"
			   "\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case ERASER_SLOT_INDEX:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":true,\"hard\":0.800000011920929,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":20,\"sizecurve\":\"0,0;0.25,0.0625;0.5,0.25;0."
			   "75,0.5625;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	default:
		qWarning("Unknown slot for default brush %d", i);
		return QByteArray();
	};
}

}

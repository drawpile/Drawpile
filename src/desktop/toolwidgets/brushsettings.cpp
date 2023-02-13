/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

extern "C" {
#include <dpmsg/blend_mode.h>
#include <dpengine/brush.h>
}

#include "brushes/brush.h"
#include "brushsettings.h"
#include "canvas/blendmodes.h"
#include "canvas/inputpresetmodel.h"
#include "main.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "ui_brushdock.h"
#include "ui_brushdockpopup.h"
#include "widgets/inputsettings.h"
#include "widgets/popup.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <mypaint-brush-settings.h>

namespace tools {

static const int BRUSH_COUNT = 6; // Last is the dedicated eraser slot
static const int ERASER_SLOT = 5; // Index of the dedicated erser slot

namespace {
	struct ToolSlot {
		brushes::ActiveBrush brush;

		// For remembering previous selection when switching between normal/erase mode
		DP_BlendMode normalMode = DP_BLEND_MODE_NORMAL;
		DP_BlendMode eraserMode = DP_BLEND_MODE_ERASE;

		QString inputPresetId;
	};
}

struct BrushSettings::Private {
	Ui_BrushDock ui;
	input::PresetModel *presetModel;
	widgets::InputSettings *inputSettingsPopup;

	QStandardItemModel *blendModes, *eraseModes;

	ToolSlot toolSlots[BRUSH_COUNT];
	widgets::GroupedToolButton *brushSlotButton[BRUSH_COUNT];
	QWidget *brushSlotWidget = nullptr;

	widgets::Popup *popup;
	Ui_BrushDockPopup popupUi;
	QWidget *popupTarget = nullptr;

	int current = 0;
	int previousNonEraser = 0;

	qreal quickAdjust1 = 0.0;

	bool shareBrushSlotColor = false;
	bool updateInProgress = false;

	inline brushes::ActiveBrush &currentBrush() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return toolSlots[current].brush;
	}

	inline ToolSlot &currentTool() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return toolSlots[current];
	}

	Private(BrushSettings *b)
	{
		presetModel = input::PresetModel::getSharedInstance();

		blendModes = new QStandardItemModel(0, 1, b);
		for(const auto &bm : canvas::blendmode::brushModeNames()) {
			auto item = new QStandardItem(bm.second);
			item->setData(int(bm.first), Qt::UserRole);
			blendModes->appendRow(item);
		}

		eraseModes = new QStandardItemModel(0, 1, b);
		auto erase1 = new QStandardItem(QApplication::tr("Erase"));
		erase1->setData(QVariant(int(DP_BLEND_MODE_ERASE)), Qt::UserRole);
		eraseModes->appendRow(erase1);

		auto erase2 = new QStandardItem(QApplication::tr("Color Erase"));
		erase2->setData(QVariant(int(DP_BLEND_MODE_COLOR_ERASE)), Qt::UserRole);
		eraseModes->appendRow(erase2);
	}

	void updateInputPresetUuid(ToolSlot &tool)
	{
		const input::Preset *preset = presetFor(tool);
		if(!preset) {
			preset = presetModel->at(0);
			if(preset)
				tool.inputPresetId = preset->id;
		}
	}

	const input::Preset *currentPreset()
	{
		const input::Preset *p = presetFor(currentTool());
		if(!p) {
			if(presetModel->rowCount()>0) {
				p = presetModel->at(0);
				currentTool().inputPresetId = p->id;
			}
		}
		return p;
	}

	const input::Preset *presetFor(const ToolSlot &tool)
	{
		return presetModel->searchPresetById(tool.inputPresetId);
	}
};

BrushSettings::BrushSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), d(new Private(this))
{
}

BrushSettings::~BrushSettings()
{
	delete d;
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	d->brushSlotWidget = new QWidget(parent);
	auto brushSlotWidgetLayout = new QHBoxLayout;
	brushSlotWidgetLayout->setSpacing(0);
	brushSlotWidgetLayout->setContentsMargins(0, 0, 0, 0);

	d->brushSlotWidget->setLayout(brushSlotWidgetLayout);

	// A spacer for centering the tool slots in the title bar
	{
		const int iconSize = parent->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, parent);
		int marginSize = parent->style()->pixelMetric(QStyle::PM_DockWidgetTitleBarButtonMargin, nullptr, parent);
		brushSlotWidgetLayout->addSpacing(marginSize+iconSize);
	}

	brushSlotWidgetLayout->addStretch();

	for(int i=0;i<BRUSH_COUNT;++i) {
		d->brushSlotButton[i] = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupCenter,
			d->brushSlotWidget
			);
		d->brushSlotButton[i]->setCheckable(true);
		d->brushSlotButton[i]->setAutoExclusive(true);
		d->brushSlotButton[i]->setText(QString::number(i+1));
		brushSlotWidgetLayout->addWidget(d->brushSlotButton[i]);

		connect(d->brushSlotButton[i], &QToolButton::clicked,
			this, [this, i]() { selectBrushSlot(i); });
	}

	brushSlotWidgetLayout->addStretch();

	d->brushSlotButton[0]->setGroupPosition(widgets::GroupedToolButton::GroupLeft);
	d->brushSlotButton[BRUSH_COUNT-1]->setGroupPosition(widgets::GroupedToolButton::GroupRight);
	d->brushSlotButton[ERASER_SLOT]->setIcon(icon::fromTheme("draw-eraser"));

	QWidget *widget = new QWidget(parent);
	d->ui.setupUi(widget);
	d->ui.inputPreset->setModel(d->presetModel);

	d->inputSettingsPopup = new widgets::InputSettings(widget);
	connect(d->inputSettingsPopup, &widgets::InputSettings::activePresetModified,
			this, &BrushSettings::emitPresetChanges);
	connect(d->inputSettingsPopup, &widgets::InputSettings::currentIndexChanged,
			d->ui.inputPreset, &QComboBox::setCurrentIndex);
	connect(d->ui.inputPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
			d->inputSettingsPopup, &widgets::InputSettings::setCurrentIndex);
	connect(d->inputSettingsPopup, &widgets::Popup::targetChanged, [this](QWidget *widget){
		d->ui.configureInput->setChecked(widget != nullptr);
	});
	connect(d->ui.configureInput, &QAbstractButton::toggled, [this](bool checked) {
		if(checked) {
			d->inputSettingsPopup->setCurrentPreset(d->currentTool().inputPresetId);
		}
		d->inputSettingsPopup->setVisibleOn(d->ui.configureInput, checked);
	});

	// The blend mode combo is ever so slightly higher than the buttons, so
	// hiding it causes some jerking normally, so we'll tell it to retain its
	// size. The space it's taking up is supposed to be empty anyway.
	QSizePolicy blendmodeSizePolicy = d->ui.blendmode->sizePolicy();
	blendmodeSizePolicy.setRetainSizeWhenHidden(true);
	d->ui.blendmode->setSizePolicy(blendmodeSizePolicy);

	// Outside communication
	connect(this, SIGNAL(pixelSizeChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(d->ui.preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));

	// Internal updates
	connect(d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::changeSizeSetting);
	connect(d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::changeRadiusLogarithmicSetting);

	connect(d->ui.blendmode, QOverload<int>::of(&QComboBox::activated), this, &BrushSettings::selectBlendMode);
	connect(d->ui.modeEraser, &QToolButton::clicked, this, &BrushSettings::setEraserMode);

	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::changeBrushType);
	connect(d->ui.squareMode, &QToolButton::clicked, this, &BrushSettings::changeBrushType);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::changeBrushType);
	connect(d->ui.mypaintMode, &QToolButton::clicked, this, &BrushSettings::changeBrushType);

	connect(d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.opacityBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.hardnessBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.smudgingBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);

	connect(d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.colorpickupBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.brushspacingBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.gainBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.slowTrackingBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);

	connect(d->ui.modeIncremental, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.modeColorpick, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.modeLockAlpha, &QToolButton::clicked, this, &BrushSettings::updateFromUi);

	connect(d->ui.inputPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BrushSettings::updateFromUi);

	// By default, the docker shows all settings at once, making it bigger on
	// startup and messing up the application layout. This will hide the excess
	// UI elements and bring the docker to a reasonable size to begin with.
	adjustSettingVisibilities(false, false);

	d->popup = new widgets::Popup{widget};
	d->popupUi.setupUi(d->popup);
	connect(d->popup, &widgets::Popup::targetChanged, this, &BrushSettings::togglePopup);
	connect(d->popupUi.pressureBox, &QCheckBox::stateChanged, this, &BrushSettings::updateFromPopupUi);
	connect(d->popupUi.minSpinner, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromPopupUi);
	connect(d->ui.pressureSize, &QToolButton::toggled, [this](bool checked){
		d->popup->setVisibleOn(d->ui.pressureSize, checked);
	});
	connect(d->ui.pressureOpacity, &QToolButton::toggled, [this](bool checked){
		d->popup->setVisibleOn(d->ui.pressureOpacity, checked);
	});
	connect(d->ui.pressureHardness, &QToolButton::toggled, [this](bool checked){
		d->popup->setVisibleOn(d->ui.pressureHardness, checked);
	});
	connect(d->ui.pressureSmudging, &QToolButton::toggled, [this](bool checked){
		d->popup->setVisibleOn(d->ui.pressureSmudging, checked);
	});

	return widget;
}

QWidget *BrushSettings::getHeaderWidget()
{
	return d->brushSlotWidget;
}

void BrushSettings::setShareBrushSlotColor(bool sameColor)
{
	d->shareBrushSlotColor = sameColor;
	for(int i=0;i<BRUSH_COUNT;++i) {
		d->brushSlotButton[i]->setColorSwatch(
			sameColor ? QColor() : d->toolSlots[i].brush.qColor()
		);
	}
}

void BrushSettings::setCurrentBrush(brushes::ActiveBrush brush)
{
	brush.setQColor(d->currentBrush().qColor());

	brushes::ActiveBrush &currentBrush = d->currentBrush();
	brushes::ActiveBrush::ActiveType activeType = brush.activeType();
	switch(activeType) {
	case brushes::ActiveBrush::CLASSIC:
		if(d->current == ERASER_SLOT) {
			brush.classic().mode = d->currentBrush().classic().mode;
		}
		currentBrush.setClassic(brush.classic());
		break;
	case brushes::ActiveBrush::MYPAINT:
		if(d->current == ERASER_SLOT) {
			brush.myPaint().brush().erase = true;
		}
		currentBrush.setMyPaint(brush.myPaint());
		break;
	default:
		qWarning("Invalid brush type %d", static_cast<int>(brush.activeType()));
		return;
	}

	currentBrush.setActiveType(activeType);
	updateUi();
}

brushes::ActiveBrush BrushSettings::currentBrush() const
{
	return d->currentBrush();
}

int BrushSettings::currentBrushSlot() const
{
	return d->current;
}

void BrushSettings::selectBrushSlot(int i)
{
	if(i<0 || i>= BRUSH_COUNT) {
		qWarning("selectBrushSlot(%d): invalid slot index!", i);
		return;
	}
	const int previousSlot = d->current;

	d->brushSlotButton[i]->setChecked(true);
	d->current = i;

	if(!d->shareBrushSlotColor)
		emit colorChanged(d->currentBrush().qColor());

	d->updateInputPresetUuid(d->currentTool());

	updateUi();

	if((previousSlot==ERASER_SLOT) != (i==ERASER_SLOT))
		emit eraseModeChanged(i==ERASER_SLOT);
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
			if(cb.mode == DP_BLEND_MODE_RECOLOR) {
				cb.mode = DP_BLEND_MODE_NORMAL;
			} else {
				cb.mode = DP_BLEND_MODE_RECOLOR;
			}
			break;
		}
		case brushes::ActiveBrush::MYPAINT: {
			DP_MyPaintBrush &mpb = brush.myPaint().brush();
			mpb.lock_alpha = !mpb.lock_alpha;
			break;
		}
		default:
			break;
		}
		updateUi();
	}
}

void BrushSettings::setEraserMode(bool erase)
{
	Q_ASSERT(!isCurrentEraserSlot());

	ToolSlot &tool = d->currentTool();
	brushes::ClassicBrush &classic = tool.brush.classic();
	classic.mode = erase ? tool.eraserMode : tool.normalMode;
	tool.brush.myPaint().brush().erase = erase;

	if(classic.isEraser() != erase) {
		// Uh oh, an inconsistency. Try to fix it.
		// This can happen if the settings data was broken
		qWarning("setEraserMode(%d): wrong mode %d", erase, int(classic.mode));
		classic.mode = erase ? DP_BLEND_MODE_ERASE : DP_BLEND_MODE_NORMAL;
	}

	updateUi();
}

void BrushSettings::selectEraserSlot(bool eraser)
{
	if(eraser) {
		if(!isCurrentEraserSlot()) {
			d->previousNonEraser = d->current;
			selectBrushSlot(ERASER_SLOT);
		}
	} else {
		if(isCurrentEraserSlot()) {
			selectBrushSlot(d->previousNonEraser);
		}
	}
}

bool BrushSettings::isCurrentEraserSlot() const
{
	return d->current == ERASER_SLOT;
}

void BrushSettings::changeBrushType()
{
	updateFromUiWith(false);
	updateUi();
}

void BrushSettings::changeSizeSetting(int size)
{
	if(d->ui.brushsizeBox->isVisible()) {
		emit pixelSizeChanged(size);
	}
}

void BrushSettings::changeRadiusLogarithmicSetting(int radiusLogarithmic)
{
	if(d->ui.radiusLogarithmicBox->isVisible()) {
		emit pixelSizeChanged(radiusLogarithmicToPixelSize(radiusLogarithmic));
	}
}

void BrushSettings::selectBlendMode(int modeIndex)
{
	const DP_BlendMode mode = DP_BlendMode(d->ui.blendmode->model()->index(modeIndex,0).data(Qt::UserRole).toInt());
	ToolSlot &tool = d->currentTool();
	brushes::ClassicBrush &classic = tool.brush.classic();

	classic.mode = mode;
	if(classic.isEraser())
		tool.eraserMode = mode;
	else
		tool.normalMode = mode;

	updateUi();
}

void BrushSettings::updateUi()
{
	// Update the UI to match the currently selected brush
	if(d->updateInProgress)
		return;

	d->updateInProgress = true;

	const ToolSlot &tool = d->currentTool();
	const brushes::ActiveBrush &brush = tool.brush;
	const brushes::ClassicBrush &classic = brush.classic();
	const brushes::MyPaintBrush &myPaint = brush.myPaint();

	// Select brush type
	const bool mypaintmode = brush.activeType() == brushes::ActiveBrush::MYPAINT;
	const bool softmode = mypaintmode || classic.shape == DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND;

	if(mypaintmode) {
		d->ui.mypaintMode->setChecked(true);
	} else {
		switch(classic.shape) {
		case DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND: d->ui.hardedgeMode->setChecked(true); break;
		case DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE: d->ui.squareMode->setChecked(true); break;
		case DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND: d->ui.softedgeMode->setChecked(true); break;
		}
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());
	adjustSettingVisibilities(softmode, mypaintmode);

	// Smudging only works right in incremental mode
	d->ui.modeIncremental->setEnabled(classic.smudge.max == 0.0);

	// Show correct blending mode
	d->ui.blendmode->setModel(classic.isEraser() ? d->eraseModes : d->blendModes);
	d->ui.modeEraser->setChecked(brush.isEraser());
	d->ui.modeEraser->setEnabled(d->current != ERASER_SLOT);

	for(int i=0;i<d->ui.blendmode->model()->rowCount();++i) {
		if(d->ui.blendmode->model()->index(i,0).data(Qt::UserRole) == int(brush.classic().mode)) {
			d->ui.blendmode->setCurrentIndex(i);
			break;
		}
	}

	// Set UI elements that are distinct between classic and MyPaint brushes
	// unconditionally, the ones that are shared only for the active type.
	// This is so that when loading a brush from settings, the hidden elements
	// don't get left in their default state.
	d->ui.brushsizeBox->setValue(classic.size.max);
	d->ui.smudgingBox->setValue(classic.smudge.max * 100);
	d->ui.colorpickupBox->setValue(classic.resmudge);
	d->ui.brushspacingBox->setValue(classic.spacing * 100);
	d->ui.modeIncremental->setChecked(classic.incremental);
	d->ui.modeColorpick->setChecked(classic.colorpick);
	updateSpinnerLabels();

	const DP_MyPaintSettings &myPaintSettings = myPaint.constSettings();
	d->ui.radiusLogarithmicBox->setValue(
		(myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC].base_value + 2.0) * 100.0);
	d->ui.gainBox->setValue(qRound(
		myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG].base_value * 100.0));
	d->ui.slowTrackingBox->setValue(qRound(
		myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_SLOW_TRACKING].base_value * 10.0));
	d->ui.modeLockAlpha->setChecked(myPaint.constBrush().lock_alpha);

	if(mypaintmode) {
		d->ui.opacityBox->setValue(qRound(
			myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_OPAQUE].base_value * 100.0));
		d->ui.hardnessBox->setValue(qRound(
			myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_HARDNESS].base_value * 100.0));
	} else {
		d->ui.opacityBox->setValue(classic.opacity.max * 100);
		d->ui.hardnessBox->setValue(classic.hardness.max * 100);
	}

	const int presetIndex = d->presetModel->searchIndexById(tool.inputPresetId);
	if(presetIndex >= 0) {
		d->ui.inputPreset->setCurrentIndex(presetIndex);
		emitPresetChanges(d->presetModel->at(presetIndex));
	}

	// Update pressure settings popup if it's currently visible.
	bool pressure = false;
	float minRatio = -1.0f;
	if(d->popupTarget == d->ui.pressureSize) {
		pressure = classic.size_pressure;
		minRatio = classic.size.min_ratio;
	} else if(d->popupTarget == d->ui.pressureOpacity) {
		pressure = classic.opacity_pressure;
		minRatio = classic.opacity.min_ratio;
	} else if(d->popupTarget == d->ui.pressureHardness) {
		pressure = classic.hardness_pressure;
		minRatio = classic.hardness.min_ratio;
	} else if(d->popupTarget == d->ui.pressureSmudging) {
		pressure = classic.smudge_pressure;
		minRatio = classic.smudge.min_ratio;
	}

	if(minRatio >= 0.0f) {
		d->popupUi.pressureBox->setChecked(pressure);
		d->popupUi.minSpinner->setEnabled(pressure);
		d->popupUi.minSpinner->setValue(qRound(minRatio * 100.0f));
	}

	// Communicate the current size of the brush cursor to the outside. These
	// functions check for their applicability themselves, so we just call both.
	changeSizeSetting(d->ui.brushsizeBox->value());
	changeRadiusLogarithmicSetting(d->ui.radiusLogarithmicBox->value());

	d->updateInProgress = false;
	d->ui.preview->setBrush(d->currentBrush());
	pushSettings();
}

void BrushSettings::updateFromUi()
{
	updateFromUiWith(true);
}

void BrushSettings::updateFromPopupUi()
{
	updateFromUiWith(true);
	d->popupUi.minSpinner->setEnabled(d->popupUi.pressureBox->isChecked());
}

void BrushSettings::updateFromUiWith(bool updateShared)
{
	if(d->updateInProgress)
		return;

	bool mypaintmode = d->ui.mypaintMode->isChecked();
	d->currentTool().brush.setActiveType(
		mypaintmode ? brushes::ActiveBrush::MYPAINT : brushes::ActiveBrush::CLASSIC);

	// Copy changes from the UI to the brush properties object,
	// then update the brush
	brushes::ActiveBrush &brush = d->currentBrush();
	brushes::ClassicBrush &classic = brush.classic();
	brushes::MyPaintBrush &myPaint = brush.myPaint();

	if(d->ui.hardedgeMode->isChecked())
		classic.shape = DP_CLASSIC_BRUSH_SHAPE_PIXEL_ROUND;
	else if(d->ui.squareMode->isChecked())
		classic.shape = DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE;
	else
		classic.shape = DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND;

	classic.size.max = d->ui.brushsizeBox->value();
	classic.smudge.max = d->ui.smudgingBox->value() / 100.0;
	classic.smudge_pressure = d->ui.pressureSmudging->isChecked();
	classic.resmudge = d->ui.colorpickupBox->value();
	classic.spacing = d->ui.brushspacingBox->value() / 100.0;
	classic.incremental = d->ui.modeIncremental->isChecked();
	classic.colorpick = d->ui.modeColorpick->isChecked();
	classic.mode = DP_BlendMode(d->ui.blendmode->currentData(Qt::UserRole).toInt());

	if(d->popupTarget == d->ui.pressureSize) {
		classic.size_pressure = d->popupUi.pressureBox->isChecked();
		classic.size.min_ratio = d->popupUi.minSpinner->value() / 100.0f;
	} else if(d->popupTarget == d->ui.pressureOpacity) {
		classic.opacity_pressure = d->popupUi.pressureBox->isChecked();
		classic.opacity.min_ratio = d->popupUi.minSpinner->value() / 100.0f;
	} else if(d->popupTarget == d->ui.pressureHardness) {
		classic.hardness_pressure = d->popupUi.pressureBox->isChecked();
		classic.hardness.min_ratio = d->popupUi.minSpinner->value() / 100.0f;
	} else if(d->popupTarget == d->ui.pressureSmudging) {
		classic.smudge_pressure = d->popupUi.pressureBox->isChecked();
		classic.smudge.min_ratio = d->popupUi.minSpinner->value() / 100.0f;
	}

	DP_MyPaintSettings &myPaintSettings = myPaint.settings();
	myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC].base_value =
		d->ui.radiusLogarithmicBox->value() / 100.0 - 2.0;
	myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG].base_value =
		d->ui.gainBox->value() / 100.0;
	myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_SLOW_TRACKING].base_value =
		d->ui.slowTrackingBox->value() / 10.0;
	myPaint.brush().lock_alpha = d->ui.modeLockAlpha->isChecked();

	// We want to keep MyPaint and classic brush opacity and hardness separate,
	// since they work so differently from each other. So this will be false
	// when switching types as to not overwrite the values with each other.
	if(updateShared) {
		if(mypaintmode) {
			myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_OPAQUE].base_value =
				d->ui.opacityBox->value() / 100.0;
			myPaintSettings.mappings[MYPAINT_BRUSH_SETTING_HARDNESS].base_value =
				d->ui.hardnessBox->value() / 100.0;
		} else {
			classic.opacity.max = d->ui.opacityBox->value() / 100.0;
			classic.hardness.max = d->ui.hardnessBox->value() / 100.0;
		}
	}

	chooseInputPreset(d->ui.inputPreset->currentIndex());

	d->ui.preview->setBrush(brush);

	d->ui.modeIncremental->setEnabled(classic.smudge.max == 0.0);

	updateSpinnerLabels();
	pushSettings();
}

void BrushSettings::updateSpinnerLabels()
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	bool isClassic = brush.activeType() == brushes::ActiveBrush::CLASSIC;
	const brushes::ClassicBrush &classic = brush.classic();

	if(isClassic) {
		if(classic.size_pressure) {
			int size = qRound(classic.size.min_ratio * classic.size.max);
			d->ui.brushsizeBox->setPrefix(tr("Size: %1 - ").arg(size));
		} else {
			d->ui.brushsizeBox->setPrefix(tr("Size: "));
		}

		if(classic.smudge_pressure) {
			int smudge = qRound(classic.smudge.min_ratio * classic.smudge.max * 100.0f);
			d->ui.smudgingBox->setPrefix(tr("Smudging: %1 - ").arg(smudge));
		} else {
			d->ui.smudgingBox->setPrefix(tr("Smudging: "));
		}
	}

	if(isClassic && classic.opacity_pressure) {
		int opacity = qRound(classic.opacity.min_ratio * classic.opacity.max * 100.0f);
		d->ui.opacityBox->setPrefix(tr("Opacity: %1 - ").arg(opacity));
	} else {
		d->ui.opacityBox->setPrefix(tr("Opacity: "));
	}

	if(isClassic && classic.hardness_pressure) {
		int hardness = qRound(classic.hardness.min_ratio * classic.hardness.max * 100.0f);
		d->ui.hardnessBox->setPrefix(tr("Hardness: %1 - ").arg(hardness));
	} else {
		d->ui.hardnessBox->setPrefix(tr("Hardness: "));
	}
}

void BrushSettings::adjustSettingVisibilities(bool softmode, bool mypaintmode)
{
	// Hide certain features based on the brush type
	QPair<QWidget *, bool> widgetVisibilities[] = {
		{d->ui.modeColorpick, !mypaintmode},
		{d->ui.modeLockAlpha, mypaintmode},
		{d->ui.modeIncremental, !mypaintmode},
		{d->ui.blendmode, !mypaintmode},
		{d->ui.pressureHardness, softmode && !mypaintmode},
		{d->ui.hardnessBox, softmode},
		{d->ui.brushsizeBox, !mypaintmode},
		{d->ui.pressureSize, !mypaintmode},
		{d->ui.radiusLogarithmicBox, mypaintmode},
		{d->ui.pressureOpacity, !mypaintmode},
		{d->ui.smudgingBox, !mypaintmode},
		{d->ui.pressureSmudging, !mypaintmode},
		{d->ui.colorpickupBox, !mypaintmode},
		{d->ui.brushspacingBox, !mypaintmode},
		{d->ui.gainBox, mypaintmode},
		{d->ui.slowTrackingBox, mypaintmode},
	};
	// First hide them all so that the docker doesn't end up bigger temporarily
	// and messes up the layout. Then afterwards show the applicable ones.
	for (const QPair<QWidget *, bool> &pair : widgetVisibilities) {
		pair.first->setVisible(false);
	}
	for (const QPair<QWidget *, bool> &pair : widgetVisibilities) {
		pair.first->setVisible(pair.second);
	}
	d->ui.blendmode->setEnabled(!mypaintmode);
}

void BrushSettings::pushSettings()
{
	controller()->setActiveBrush(d->ui.preview->brush());
}

void BrushSettings::chooseInputPreset(int index)
{
	ToolSlot &tool = d->currentTool();
	const input::Preset *preset = d->presetModel->at(index);
	if(preset && tool.inputPresetId != preset->id) {
		tool.inputPresetId = preset->id;
		emitPresetChanges(preset);
	}
}

void BrushSettings::togglePopup(QWidget *widget)
{
	QSignalBlocker blocker{this};
	QSignalBlocker popupBlocker{d->popup};
	d->popupTarget = widget;
	d->ui.pressureSize->setChecked(false);
	d->ui.pressureOpacity->setChecked(false);
	d->ui.pressureHardness->setChecked(false);
	d->ui.pressureSmudging->setChecked(false);

	widgets::GroupedToolButton *button;
	QString title;
	QString prefix;
	if(widget == d->ui.pressureSize) {
		button = d->ui.pressureSize;
		title = tr("Size");
		prefix = tr("Minimum Size: ");
	} else if(widget == d->ui.pressureOpacity) {
		button = d->ui.pressureOpacity;
		title = tr("Opacity");
		prefix = tr("Minimum Opacity: ");
	} else if(widget == d->ui.pressureHardness) {
		button = d->ui.pressureHardness;
		title = tr("Hardness");
		prefix = tr("Minimum Hardness: ");
	} else if(widget == d->ui.pressureSmudging) {
		button = d->ui.pressureSmudging;
		title = tr("Smudging");
		prefix = tr("Minimum Smudging: ");
	} else {
		return;
	}

	button->setChecked(true);
	d->popup->setWindowTitle(title);
	d->popupUi.minSpinner->setPrefix(prefix);
	updateUi();
}

void BrushSettings::emitPresetChanges(const input::Preset *preset)
{
	if(preset) {
		emit smoothingChanged(preset->smoothing);
		emit pressureMappingChanged(preset->curve);
	}
}

int BrushSettings::getSmoothing() const
{
	const input::Preset *preset = d->currentPreset();
	return preset ? preset->smoothing : 0;
}

PressureMapping BrushSettings::getPressureMapping() const
{
	const input::Preset *preset = d->currentPreset();
	return preset ? preset->curve : PressureMapping{};
}

namespace toolprop {
	static const ToolProperties::RangedValue<int>
		activeSlot = {QStringLiteral("active"), 0, 0, BRUSH_COUNT-1}
		;
	// dynamic toolprops: brush[0-5] (serialized JSON)
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());

	cfg.setValue(toolprop::activeSlot, d->current);

	for(int i=0;i<BRUSH_COUNT;++i) {
		const ToolSlot &tool = d->toolSlots[i];
		QJsonObject b = tool.brush.toJson();

		b["_slot"] = QJsonObject {
			{"normalMode", int(tool.normalMode)},
			{"eraserMode", int(tool.eraserMode)},
			{"color", tool.brush.qColor().name()},
			{"inputPresetId", tool.inputPresetId}
		};

		cfg.setValue(
			ToolProperties::Value<QByteArray> {
				QStringLiteral("brush%1").arg(i),
				QByteArray()
			},
			QJsonDocument(b).toJson(QJsonDocument::Compact)
		);
	}

	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{

	for(int i=0;i<BRUSH_COUNT;++i) {
		ToolSlot &tool = d->toolSlots[i];

		const QJsonObject o = QJsonDocument::fromJson(
			cfg.value(ToolProperties::Value<QByteArray> {
				QStringLiteral("brush%1").arg(i),
				QByteArray()
				})
			).object();
		const QJsonObject s = o["_slot"].toObject();

		tool.brush = brushes::ActiveBrush::fromJson(o);
		const auto color = QColor(s["color"].toString());
		tool.brush.setQColor(color.isValid() ? color : Qt::black);
		tool.normalMode = DP_BlendMode(s["normalMode"].toInt());
		tool.eraserMode = DP_BlendMode(s["eraserMode"].toInt());
		tool.inputPresetId = s["inputPresetId"].toString();
		d->updateInputPresetUuid(tool);

		if(!d->shareBrushSlotColor)
			d->brushSlotButton[i]->setColorSwatch(tool.brush.qColor());
	}

	if(!d->toolSlots[ERASER_SLOT].brush.isEraser()) {
		d->toolSlots[ERASER_SLOT].brush.classic().mode = DP_BLEND_MODE_ERASE;
		d->toolSlots[ERASER_SLOT].brush.myPaint().brush().erase = true;
	}

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = d->current != ERASER_SLOT ? d->current : 0;
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::LINE: d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_LINE); break;
	case tools::Tool::RECTANGLE: d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_RECTANGLE); break;
	case tools::Tool::ELLIPSE: d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_ELLIPSE); break;
	default: d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_STROKE); break;
	}

	if(tool == tools::Tool::ERASER) {
		selectEraserSlot(true);
		for(int i=0;i<BRUSH_COUNT-1;++i)
			d->brushSlotButton[i]->setEnabled(false);
	} else {
		for(int i=0;i<BRUSH_COUNT-1;++i)
			d->brushSlotButton[i]->setEnabled(true);

		selectEraserSlot(false);
	}
}

void BrushSettings::setForeground(const QColor& color)
{
	if(color != d->currentBrush().qColor()) {
		if(d->shareBrushSlotColor) {
			for(int i=0;i<BRUSH_COUNT;++i)
				d->toolSlots[i].brush.setQColor(color);

		} else {
			Q_ASSERT(d->current>=0 && d->current < BRUSH_COUNT);
			d->currentBrush().setQColor(color);
			d->brushSlotButton[d->current]->setColorSwatch(color);
		}

		d->ui.preview->setBrush(d->currentBrush());
		pushSettings();
	}
}

void BrushSettings::quickAdjust1(qreal adjustment)
{
	// Adjust the currently visible box. Since the MyPaint radius has a much
	// larger range, we double the adjustment so that it feels responsive.
	if(d->ui.brushsizeBox->isVisible()) {
		quickAdjustOn(d->ui.brushsizeBox, adjustment);
	} else {
		quickAdjustOn(d->ui.radiusLogarithmicBox, adjustment * 2.0);
	}
}

void BrushSettings::stepAdjust1(bool increase)
{
	QSpinBox *brushsizeBox = d->ui.brushsizeBox;
	if(brushsizeBox->isVisible()) {
		brushsizeBox->setValue(stepLogarithmic(
			brushsizeBox->minimum(), brushsizeBox->maximum(),
			brushsizeBox->value(), increase));
	} else {
		QSpinBox *radiusLogarithmicBox = d->ui.radiusLogarithmicBox;
		radiusLogarithmicBox->setValue(stepLinear(
			radiusLogarithmicBox->minimum(), radiusLogarithmicBox->maximum(),
			radiusLogarithmicBox->value(), increase));
	}
}

void BrushSettings::quickAdjustOn(QSpinBox *box, qreal adjustment)
{
	d->quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(d->quickAdjust1, &i);
	if(int(i)) {
		d->quickAdjust1 = f;
		box->setValue(box->value() + int(i));
	}
}

int BrushSettings::getSize() const
{
	if(d->ui.brushsizeBox->isVisible()) {
		return d->ui.brushsizeBox->value();
	} else {
		return radiusLogarithmicToPixelSize(d->ui.radiusLogarithmicBox->value());
	}
}

bool BrushSettings::getSubpixelMode() const
{
	return d->currentBrush().classic().shape == DP_CLASSIC_BRUSH_SHAPE_SOFT_ROUND;
}

bool BrushSettings::isSquare() const
{
	return d->currentBrush().classic().shape == DP_CLASSIC_BRUSH_SHAPE_PIXEL_SQUARE;
}

double BrushSettings::radiusLogarithmicToPixelSize(int radiusLogarithmic)
{
	return std::exp(radiusLogarithmic / 100.0 - 2.0) * 2.0;
}

}

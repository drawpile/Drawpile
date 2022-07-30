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

#include "brushsettings.h"
#include "main.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "brushes/brush.h"
#include "dialogs/inputsettings.h"

#include "canvas/inputpresetmodel.h"
#include "canvas/blendmodes.h"
#include "ui_brushdock.h"

#include "../rustpile/rustpile.h"

#include <QKeyEvent>
#include <QPainter>
#include <QMimeData>
#include <QSettings>
#include <QStandardItemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <qapplication.h>
#include <qglobal.h>

namespace tools {

using brushes::ClassicBrush;

static const int BRUSH_COUNT = 6; // Last is the dedicated eraser slot
static const int ERASER_SLOT = 5; // Index of the dedicated erser slot

namespace {
	struct ToolSlot {
		ClassicBrush brush;

		// For remembering previous selection when switching between normal/erase mode
		rustpile::Blendmode normalMode = rustpile::Blendmode::Normal;
		rustpile::Blendmode eraserMode = rustpile::Blendmode::Erase;

		QString inputPresetId;
	};
}

struct BrushSettings::Private {
	Ui_BrushDock ui;
	QPointer<dialogs::InputSettings> inputSettingsDialog = nullptr;
	input::PresetModel *presetModel;

	QStandardItemModel *blendModes, *eraseModes;

	ToolSlot toolSlots[BRUSH_COUNT];
	widgets::GroupedToolButton *brushSlotButton[BRUSH_COUNT];
	QWidget *brushSlotWidget = nullptr;

	int current = 0;
	int previousNonEraser = 0;

	qreal quickAdjust1 = 0.0;

	bool shareBrushSlotColor = false;
	bool updateInProgress = false;

	inline ClassicBrush &currentBrush() {
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
		erase1->setData(QVariant(int(rustpile::Blendmode::Erase)), Qt::UserRole);
		eraseModes->appendRow(erase1);

		auto erase2 = new QStandardItem(QApplication::tr("Color Erase"));
		erase2->setData(QVariant(int(rustpile::Blendmode::ColorErase)), Qt::UserRole);
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

const int BrushSettings::MAX_BRUSH_SIZE = 255;
const int BrushSettings::MAX_BRUSH_SPACING = 999;
const int BrushSettings::DEFAULT_BRUSH_SIZE = 128;
const int BrushSettings::DEFAULT_BRUSH_SPACING = 50;

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

	connect(d->ui.configureInput, &QAbstractButton::clicked, [this, parent]() {
		if(!d->inputSettingsDialog) {
			d->inputSettingsDialog = new dialogs::InputSettings(parent);
			d->inputSettingsDialog->setAttribute(Qt::WA_DeleteOnClose);
			connect(d->inputSettingsDialog, &dialogs::InputSettings::activePresetModified,
					this, &BrushSettings::emitPresetChanges);
			connect(d->inputSettingsDialog, &dialogs::InputSettings::currentIndexChanged,
					d->ui.inputPreset, &QComboBox::setCurrentIndex);
			connect(d->ui.inputPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
					d->inputSettingsDialog, &dialogs::InputSettings::setCurrentIndex);
		}
		d->inputSettingsDialog->setCurrentPreset(d->currentTool().inputPresetId);
		d->inputSettingsDialog->show();
	});

	// Outside communication
	connect(d->ui.brushsizeBox, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(d->ui.preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));

	// Internal updates
	connect(d->ui.blendmode, QOverload<int>::of(&QComboBox::activated), this, &BrushSettings::selectBlendMode);
	connect(d->ui.modeEraser, &QToolButton::clicked, this, &BrushSettings::setEraserMode);

	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.squareMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.squareMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);

	connect(d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureSize, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushopacity, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureOpacity, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushhardness, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureHardness, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushsmudging, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureSmudging, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.colorpickup, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.brushspacingBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &BrushSettings::updateFromUi);
	connect(d->ui.modeIncremental, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.modeColorpick, &QToolButton::clicked, this, &BrushSettings::updateFromUi);

	connect(d->ui.inputPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BrushSettings::updateFromUi);

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

void BrushSettings::setCurrentBrush(ClassicBrush brush)
{
	brush.setQColor(d->currentBrush().qColor());

	if(d->current == ERASER_SLOT && !brush.isEraser())
		brush.mode = d->currentBrush().mode;

	d->currentBrush() = brush;
	updateUi();
}

ClassicBrush BrushSettings::currentBrush() const
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

void BrushSettings::setEraserMode(bool erase)
{
	Q_ASSERT(!isCurrentEraserSlot());

	auto &tool = d->currentTool();
	if(erase)
		tool.brush.mode = tool.eraserMode;
	else
		tool.brush.mode = tool.normalMode;

	if(tool.brush.isEraser() != erase) {
		// Uh oh, an inconsistency. Try to fix it.
		// This can happen if the settings data was broken
		qWarning("setEraserMode(%d): wrong mode %d", erase, int(tool.brush.mode));
		if(erase)
			tool.brush.mode = rustpile::Blendmode::Erase;
		else
			tool.brush.mode = rustpile::Blendmode::Normal;
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

void BrushSettings::selectBlendMode(int modeIndex)
{
	const auto mode = rustpile::Blendmode(d->ui.blendmode->model()->index(modeIndex,0).data(Qt::UserRole).toInt());
	auto &tool = d->currentTool();

	tool.brush.mode = mode;
	if(tool.brush.isEraser())
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
	const ClassicBrush &brush = tool.brush;

	// Select brush type
	const bool softmode = brush.shape == rustpile::ClassicBrushShape::RoundSoft;

	switch(brush.shape) {
	case rustpile::ClassicBrushShape::RoundPixel: d->ui.hardedgeMode->setChecked(true); break;
	case rustpile::ClassicBrushShape::SquarePixel: d->ui.squareMode->setChecked(true); break;
	case rustpile::ClassicBrushShape::RoundSoft: d->ui.softedgeMode->setChecked(true); break;
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());

	// Hide certain features based on the brush type
	d->ui.brushhardness->setVisible(softmode);
	d->ui.pressureHardness->setVisible(softmode);
	d->ui.hardnessLabel->setVisible(softmode);
	d->ui.hardnessBox->setVisible(softmode);

	// Smudging only works right in incremental mode
	d->ui.modeIncremental->setEnabled(brush.smudge.max == 0.0);

	// Show correct blending mode
	if(brush.isEraser())
		d->ui.blendmode->setModel(d->eraseModes);
	else
		d->ui.blendmode->setModel(d->blendModes);
	d->ui.modeEraser->setChecked(brush.isEraser());
	d->ui.modeEraser->setEnabled(d->current != ERASER_SLOT);

	for(int i=0;i<d->ui.blendmode->model()->rowCount();++i) {
		if(d->ui.blendmode->model()->index(i,0).data(Qt::UserRole) == int(brush.mode)) {
			d->ui.blendmode->setCurrentIndex(i);
			break;
		}
	}

	// Set values
	d->ui.brushsizeBox->setValue(brush.size.max);
	d->ui.pressureSize->setChecked(brush.size_pressure);

	d->ui.brushopacity->setValue(brush.opacity.max * 100);
	d->ui.pressureOpacity->setChecked(brush.opacity_pressure);

	d->ui.brushhardness->setValue(brush.hardness.max * 100);
	d->ui.pressureHardness->setChecked(softmode && brush.hardness_pressure);

	d->ui.brushsmudging->setValue(brush.smudge.max * 100);
	d->ui.pressureSmudging->setChecked(brush.smudge_pressure);

	d->ui.colorpickup->setValue(brush.resmudge);

	d->ui.brushspacingBox->setValue(brush.spacing * 100);
	d->ui.modeIncremental->setChecked(brush.incremental);
	d->ui.modeColorpick->setChecked(brush.colorpick);

	const int presetIndex = d->presetModel->searchIndexById(tool.inputPresetId);
	if(presetIndex >= 0) {
		d->ui.inputPreset->setCurrentIndex(presetIndex);
		emitPresetChanges(d->presetModel->at(presetIndex));
	}

	d->updateInProgress = false;
	d->ui.preview->setBrush(d->currentBrush());
	pushSettings();
}

void BrushSettings::updateFromUi()
{
	if(d->updateInProgress)
		return;

	// Copy changes from the UI to the brush properties object,
	// then update the brush
	auto &brush = d->currentBrush();

	if(d->ui.hardedgeMode->isChecked())
		brush.shape = rustpile::ClassicBrushShape::RoundPixel;
	else if(d->ui.squareMode->isChecked())
		brush.shape = rustpile::ClassicBrushShape::SquarePixel;
	else 
		brush.shape = rustpile::ClassicBrushShape::RoundSoft;

	brush.size.max = d->ui.brushsizeBox->value();
	brush.size_pressure = d->ui.pressureSize->isChecked();

	brush.opacity.max = d->ui.brushopacity->value() / 100.0;
	brush.opacity_pressure = d->ui.pressureOpacity->isChecked();

	brush.hardness.max = d->ui.brushhardness->value() / 100.0;
	brush.hardness_pressure = d->ui.pressureHardness->isChecked();

	brush.smudge.max = d->ui.brushsmudging->value() / 100.0;
	brush.smudge_pressure = d->ui.pressureSmudging->isChecked();
	brush.resmudge = d->ui.colorpickup->value();

	brush.spacing = d->ui.brushspacingBox->value() / 100.0;
	brush.incremental = d->ui.modeIncremental->isChecked();
	brush.colorpick = d->ui.modeColorpick->isChecked();
	brush.mode = rustpile::Blendmode(d->ui.blendmode->currentData(Qt::UserRole).toInt());

	chooseInputPreset(d->ui.inputPreset->currentIndex());

	d->ui.preview->setBrush(brush);

	d->ui.modeIncremental->setEnabled(brush.smudge.max == 0.0);

	pushSettings();
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

void BrushSettings::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/brushsliderlimits");
	d->ui.brushsizeSlider->setMaximum(qMin(
			cfg.value("size", DEFAULT_BRUSH_SIZE).toInt(),
			MAX_BRUSH_SIZE));
	d->ui.brushspacingSlider->setMaximum(qMin(
			cfg.value("spacing", DEFAULT_BRUSH_SPACING).toInt(),
			MAX_BRUSH_SPACING));
	cfg.endGroup();
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

		tool.brush = ClassicBrush::fromJson(o);
		const auto color = QColor(s["color"].toString());
		tool.brush.setQColor(color.isValid() ? color : Qt::black);
		tool.normalMode = rustpile::Blendmode(s["normalMode"].toInt());
		tool.eraserMode = rustpile::Blendmode(s["eraserMode"].toInt());
		tool.inputPresetId = s["inputPresetId"].toString();
		d->updateInputPresetUuid(tool);

		if(!d->shareBrushSlotColor)
			d->brushSlotButton[i]->setColorSwatch(tool.brush.qColor());
	}

	if(!d->toolSlots[ERASER_SLOT].brush.isEraser())
		d->toolSlots[ERASER_SLOT].brush.mode = rustpile::Blendmode::Erase;

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = d->current != ERASER_SLOT ? d->current : 0;
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::LINE: d->ui.preview->setPreviewShape(rustpile::BrushPreviewShape::Line); break;
	case tools::Tool::RECTANGLE: d->ui.preview->setPreviewShape(rustpile::BrushPreviewShape::Rectangle); break;
	case tools::Tool::ELLIPSE: d->ui.preview->setPreviewShape(rustpile::BrushPreviewShape::Ellipse); break;
	default: d->ui.preview->setPreviewShape(rustpile::BrushPreviewShape::Stroke); break;
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
	d->quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(d->quickAdjust1, &i);
	if(int(i)) {
		d->quickAdjust1 = f;
		d->ui.brushsizeBox->setValue(d->ui.brushsizeBox->value() + int(i));
	}
}

int BrushSettings::getSize() const
{
	return d->ui.brushsizeBox->value();
}

bool BrushSettings::getSubpixelMode() const
{
	return d->currentBrush().shape == rustpile::ClassicBrushShape::RoundSoft;
}

bool BrushSettings::isSquare() const
{
	return d->currentBrush().shape == rustpile::ClassicBrushShape::SquarePixel;
}

}

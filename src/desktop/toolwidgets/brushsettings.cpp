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

#include "brushsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "brushes/brush.h"

#include "ui_brushdock.h"

#include <QKeyEvent>
#include <QPainter>
#include <QMimeData>
#include <QSettings>
#include <QStandardItemModel>
#include <QJsonDocument>
#include <QJsonObject>

namespace tools {

using brushes::ClassicBrush;

static const int BRUSH_COUNT = 6; // Last is the dedicated eraser slot
static const int ERASER_SLOT = 5; // Index of the dedicated erser slot

namespace {
	struct ToolSlot {
		ClassicBrush brush;

		// For remembering previuous selection when switching between normal/erase mode
		paintcore::BlendMode::Mode normalMode = paintcore::BlendMode::MODE_NORMAL;
		paintcore::BlendMode::Mode eraserMode = paintcore::BlendMode::MODE_ERASE;
	};
}

struct BrushSettings::Private {
	Ui_BrushDock ui;

	QStandardItemModel *blendModes, *eraseModes;

	ToolSlot toolSlots[BRUSH_COUNT];
	int current = 0;
	int previousNonEraser = 0;

	bool shareBrushSlotColor = false;
	bool updateInProgress = false;

	qreal quickAdjust1 = 0.0;

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
		blendModes = new QStandardItemModel(0, 1, b);
		for(const auto &bm : paintcore::getBlendModeNames(paintcore::BlendMode::BrushMode)) {
			auto item = new QStandardItem(bm.second);
			item->setData(bm.first, Qt::UserRole);
			blendModes->appendRow(item);
		}

		eraseModes = new QStandardItemModel(0, 1, b);
		auto erase1 = new QStandardItem(QApplication::tr("Erase"));
		erase1->setData(QVariant(paintcore::BlendMode::MODE_ERASE), Qt::UserRole);
		eraseModes->appendRow(erase1);

		auto erase2 = new QStandardItem(QApplication::tr("Color Erase"));
		erase2->setData(QVariant(paintcore::BlendMode::MODE_COLORERASE), Qt::UserRole);
		eraseModes->appendRow(erase2);
	}

	widgets::GroupedToolButton *brushSlotButton(int i)
	{
		static_assert (BRUSH_COUNT == 6, "update brushSlottButton");
		switch(i) {
		case 0: return ui.slot1;
		case 1: return ui.slot2;
		case 2: return ui.slot3;
		case 3: return ui.slot4;
		case 4: return ui.slot5;
		case 5: return ui.slotEraser;
		default: qFatal("brushSlotButton(%d): no such button", i);
		}
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
	QWidget *widget = new QWidget(parent);
	d->ui.setupUi(widget);

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

	// Brush slot buttons
	for(int i=0;i<BRUSH_COUNT;++i) {
		connect(d->brushSlotButton(i), &QToolButton::clicked, this, [this, i]() { selectBrushSlot(i); });
	}

	return widget;
}

void BrushSettings::setShareBrushSlotColor(bool sameColor)
{
	d->shareBrushSlotColor = sameColor;
	for(int i=0;i<BRUSH_COUNT;++i) {
		d->brushSlotButton(i)->setColorSwatch(
			sameColor ? QColor() : d->toolSlots[i].brush.color()
		);
	}
}

void BrushSettings::setCurrentBrush(ClassicBrush brush)
{
	brush.setColor(d->currentBrush().color());

	if(d->current == ERASER_SLOT && !brush.isEraser())
		brush.setBlendingMode(d->currentBrush().blendingMode());

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

	d->brushSlotButton(i)->setChecked(true);
	d->current = i;

	if(!d->shareBrushSlotColor)
		emit colorChanged(d->currentBrush().color());

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
		tool.brush.setBlendingMode(tool.eraserMode);
	else
		tool.brush.setBlendingMode(tool.normalMode);

	if(tool.brush.isEraser() != erase) {
		// Uh oh, an inconsistency. Try to fix it.
		// This can happen if the settings data was broken
		qWarning("setEraserMode(%d): wrong mode %d", erase, tool.brush.blendingMode());
		if(erase)
			tool.brush.setBlendingMode(paintcore::BlendMode::MODE_ERASE);
		else
			tool.brush.setBlendingMode(paintcore::BlendMode::MODE_NORMAL);
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
	const auto mode = paintcore::BlendMode::Mode(d->ui.blendmode->model()->index(modeIndex,0).data(Qt::UserRole).toInt());
	auto &tool = d->currentTool();

	tool.brush.setBlendingMode(mode);
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
	const bool softmode = brush.shape() == ClassicBrush::ROUND_SOFT;

	switch(brush.shape()) {
	case ClassicBrush::ROUND_PIXEL: d->ui.hardedgeMode->setChecked(true); break;
	case ClassicBrush::SQUARE_PIXEL: d->ui.squareMode->setChecked(true); break;
	case ClassicBrush::ROUND_SOFT: d->ui.softedgeMode->setChecked(true); break;
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());

	// Hide certain features based on the brush type
	d->ui.brushhardness->setVisible(softmode);
	d->ui.pressureHardness->setVisible(softmode);
	d->ui.hardnessLabel->setVisible(softmode);
	d->ui.hardnessBox->setVisible(softmode);

	// Smudging only works right in incremental mode
	d->ui.modeIncremental->setEnabled(brush.smudge1() == 0.0);

	// Show correct blending mode
	if(brush.isEraser())
		d->ui.blendmode->setModel(d->eraseModes);
	else
		d->ui.blendmode->setModel(d->blendModes);
	d->ui.modeEraser->setChecked(brush.isEraser());
	d->ui.modeEraser->setEnabled(d->current != ERASER_SLOT);

	for(int i=0;i<d->ui.blendmode->model()->rowCount();++i) {
		if(d->ui.blendmode->model()->index(i,0).data(Qt::UserRole) == int(brush.blendingMode())) {
			d->ui.blendmode->setCurrentIndex(i);
			break;
		}
	}

	// Set values
	d->ui.brushsizeBox->setValue(brush.size1());
	d->ui.pressureSize->setChecked(brush.useSizePressure());

	d->ui.brushopacity->setValue(brush.opacity1() * 100);
	d->ui.pressureOpacity->setChecked(brush.useOpacityPressure());

	d->ui.brushhardness->setValue(brush.hardness1() * 100);
	d->ui.pressureHardness->setChecked(softmode && brush.useHardnessPressure());

	d->ui.brushsmudging->setValue(brush.smudge1() * 100);
	d->ui.pressureSmudging->setChecked(brush.useSmudgePressure());

	d->ui.colorpickup->setValue(brush.resmudge());

	d->ui.brushspacingBox->setValue(brush.spacing() * 100);
	d->ui.modeIncremental->setChecked(brush.incremental());
	d->ui.modeColorpick->setChecked(brush.isColorPickMode());

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
		brush.setShape(ClassicBrush::ROUND_PIXEL);
	else if(d->ui.squareMode->isChecked())
		brush.setShape(ClassicBrush::SQUARE_PIXEL);
	else 
		brush.setShape(ClassicBrush::ROUND_SOFT);

	brush.setSize(d->ui.brushsizeBox->value());
	brush.setSizePressure(d->ui.pressureSize->isChecked());

	brush.setOpacity(d->ui.brushopacity->value() / 100.0);
	brush.setOpacityPressure(d->ui.pressureOpacity->isChecked());

	brush.setHardness(d->ui.brushhardness->value() / 100.0);
	brush.setHardnessPressure(d->ui.pressureHardness->isChecked());

	brush.setSmudge(d->ui.brushsmudging->value() / 100.0);
	brush.setSmudgePressure(d->ui.pressureSmudging->isChecked());
	brush.setResmudge(d->ui.colorpickup->value());

	brush.setSpacing(d->ui.brushspacingBox->value() / 100.0);
	brush.setIncremental(d->ui.modeIncremental->isChecked());
	brush.setColorPickMode(d->ui.modeColorpick->isChecked());
	brush.setBlendingMode(paintcore::BlendMode::Mode(d->ui.blendmode->currentData(Qt::UserRole).toInt()));

	d->ui.preview->setBrush(brush);

	d->ui.modeIncremental->setEnabled(brush.smudge1() == 0.0);

	pushSettings();
}

void BrushSettings::pushSettings()
{
	controller()->setActiveBrush(d->ui.preview->brush());
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
			{"normalMode", tool.normalMode},
			{"eraserMode", tool.eraserMode},
			{"color", tool.brush.color().name()}
		};

		cfg.setValue(
			ToolProperties::Value<QByteArray> {
				QStringLiteral("brush%1").arg(i),
				QByteArray()
			},
			QJsonDocument(b).toBinaryData()
		);
	}

	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{

	for(int i=0;i<BRUSH_COUNT;++i) {
		ToolSlot &tool = d->toolSlots[i];

		const QJsonObject o = QJsonDocument::fromBinaryData(
			cfg.value(ToolProperties::Value<QByteArray> {
				QStringLiteral("brush%1").arg(i),
				QByteArray()
				})
			).object();
		const QJsonObject s = o["_slot"].toObject();

		tool.brush = ClassicBrush::fromJson(o);
		const auto color = QColor(s["color"].toString());
		tool.brush.setColor(color.isValid() ? color : Qt::black);
		tool.normalMode = paintcore::BlendMode::Mode(s["normalMode"].toInt());
		tool.eraserMode = paintcore::BlendMode::Mode(s["eraserMode"].toInt());

		if(!d->shareBrushSlotColor)
			d->brushSlotButton(i)->setColorSwatch(tool.brush.color());
	}

	if(!d->toolSlots[ERASER_SLOT].brush.isEraser())
		d->toolSlots[ERASER_SLOT].brush.setBlendingMode(paintcore::BlendMode::MODE_ERASE);

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = d->current != ERASER_SLOT ? d->current : 0;
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::LINE: d->ui.preview->setPreviewShape(widgets::BrushPreview::Line); break;
	case tools::Tool::RECTANGLE: d->ui.preview->setPreviewShape(widgets::BrushPreview::Rectangle); break;
	case tools::Tool::ELLIPSE: d->ui.preview->setPreviewShape(widgets::BrushPreview::Ellipse); break;
	default: d->ui.preview->setPreviewShape(widgets::BrushPreview::Stroke); break;
	}

	if(tool == tools::Tool::ERASER) {
		selectEraserSlot(true);
		for(int i=0;i<BRUSH_COUNT-1;++i)
			d->brushSlotButton(i)->setEnabled(false);
	} else {
		for(int i=0;i<BRUSH_COUNT-1;++i)
			d->brushSlotButton(i)->setEnabled(true);

		selectEraserSlot(false);
	}
}

void BrushSettings::setForeground(const QColor& color)
{
	if(color != d->currentBrush().color()) {
		if(d->shareBrushSlotColor) {
			for(int i=0;i<BRUSH_COUNT;++i)
				d->toolSlots[i].brush.setColor(color);

		} else {
			d->currentBrush().setColor(color);
			d->brushSlotButton(d->current)->setColorSwatch(color);
		}

		d->ui.preview->setBrush(d->currentBrush());
		pushSettings();
	}
}

void BrushSettings::quickAdjust1(qreal adjustment)
{
	d->quickAdjust1 += adjustment;
	if(qAbs(d->quickAdjust1) >= 1.0) {
		qreal i;
		d->quickAdjust1 = modf(d->quickAdjust1, &i);
		d->ui.brushsizeBox->setValue(d->ui.brushsizeBox->value() + int(i));
	}
}

int BrushSettings::getSize() const
{
	return d->ui.brushsizeBox->value();
}

bool BrushSettings::getSubpixelMode() const
{
	return d->currentBrush().shape() == ClassicBrush::ROUND_SOFT;
}

bool BrushSettings::isSquare() const
{
	return d->currentBrush().shape() == ClassicBrush::SQUARE_PIXEL;
}

}

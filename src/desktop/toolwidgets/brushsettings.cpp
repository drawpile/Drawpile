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
#include "core/brushmask.h"
#include "brushes/brush.h"
#include "brushes/classicbrushpainter.h"
#include "brushes/pixelbrushpainter.h"
#include "utils/icon.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/groupedtoolbutton.h"
#include "widgets/brushpreview.h"
using widgets::BrushPreview;
using widgets::GroupedToolButton;

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
	BrushPresetModel *presets;

	ToolSlot toolSlots[BRUSH_COUNT];
	int current = 0;
	int previousNonEraser = 0;

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

	inline QColor currentColor() {
		if(shareBrushSlotColor)
			return ui.preview->brushColor();
		else
			return currentTool().brush.color();
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

		presets = BrushPresetModel::getSharedInstance();
	}

	void updateBrush()
	{
		ui.preview->setBrush(currentBrush());
		if(!shareBrushSlotColor)
			ui.preview->setColor(currentColor());
	}

	GroupedToolButton *brushSlotButton(int i)
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
	connect(d->ui.preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	// Internal updates
	connect(d->ui.blendmode, QOverload<int>::of(&QComboBox::activated), this, &BrushSettings::selectBlendMode);
	connect(d->ui.modeEraser, &QToolButton::clicked, this, &BrushSettings::setEraserMode);

	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.squareMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.squareMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.watercolorMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.watercolorMode, &QToolButton::clicked, this, &BrushSettings::updateUi);

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

void BrushSettings::setCurrentBrushSettings(const ToolProperties &brushProps)
{
#if 0 // TODO
	d->currentBrush() = brushProps;
	updateUi();
#endif
}

ToolProperties BrushSettings::getCurrentBrushSettings() const
{
	// TODO
	return ToolProperties(); //d->currentBrush();
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

	if(d->shareBrushSlotColor)
		d->currentBrush().setColor(d->currentColor());
	else
		emit colorChanged(d->currentColor());

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
	const bool watercolor = brush.smudge1() > 0;
	const bool softmode = brush.shape() == ClassicBrush::ROUND_SOFT;

	switch(brush.shape()) {
	case ClassicBrush::ROUND_PIXEL: d->ui.hardedgeMode->setChecked(true); break;
	case ClassicBrush::SQUARE_PIXEL: d->ui.squareMode->setChecked(true); break;
	case ClassicBrush::ROUND_SOFT:
		// TODO decouple watercolor mode from brush shape
		if(watercolor)
			d->ui.watercolorMode->setChecked(true);
		else
			d->ui.softedgeMode->setChecked(true);
		break;
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());

	// Hide certain features based on the brush type
	d->ui.brushhardness->setVisible(softmode);
	d->ui.pressureHardness->setVisible(softmode);
	d->ui.hardnessLabel->setVisible(softmode);
	d->ui.hardnessBox->setVisible(softmode);

	d->ui.brushsmudging->setVisible(watercolor);
	d->ui.pressureSmudging->setVisible(watercolor);
	d->ui.smudgingLabel->setVisible(watercolor);
	d->ui.smudgingBox->setVisible(watercolor);

	d->ui.colorpickup->setVisible(watercolor);
	d->ui.colorpickupLabel->setVisible(watercolor);
	d->ui.colorpickupBox->setVisible(watercolor);

	d->ui.modeIncremental->setEnabled(!watercolor);

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
	d->ui.pressureSmudging->setChecked(watercolor && brush.useSmudgePressure());

	d->ui.colorpickup->setValue(brush.resmudge());

	d->ui.brushspacingBox->setValue(brush.spacing() * 100);
	d->ui.modeIncremental->setChecked(brush.incremental());
	d->ui.modeColorpick->setChecked(brush.isColorPickMode());

	d->updateInProgress = false;
	d->updateBrush();
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

	// TODO: decouple watercolor mode from brush shape
	const bool watercolor = d->ui.watercolorMode->isChecked();

	brush.setSize(d->ui.brushsizeBox->value());
	brush.setSizePressure(d->ui.pressureSize->isChecked());

	brush.setOpacity(d->ui.brushopacity->value() / 100.0);
	brush.setOpacityPressure(d->ui.pressureOpacity->isChecked());

	brush.setHardness(d->ui.brushhardness->value() / 100.0);
	brush.setHardnessPressure(d->ui.pressureHardness->isChecked());

	if(watercolor) {
		// an ugly hack until TODO: minimum smudging value when watercolor
		// mode is enabled is 1. A nonzero value reveals the smudging
		// controls in the UI.
		brush.setSmudge(d->ui.brushsmudging->value() / 100.0);
		brush.setSmudgePressure(d->ui.pressureSmudging->isChecked());
		brush.setResmudge(d->ui.colorpickup->value());
	} else {
		brush.setSmudge(0);
	}

	brush.setSpacing(d->ui.brushspacingBox->value() / 100.0);
	brush.setIncremental(d->ui.modeIncremental->isChecked());
	brush.setColorPickMode(d->ui.modeColorpick->isChecked());
	brush.setBlendingMode(paintcore::BlendMode::Mode(d->ui.blendmode->currentData(Qt::UserRole).toInt()));

	d->updateBrush();
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
		tool.brush.setColor(QColor(s["color"].toString()));
		tool.normalMode = paintcore::BlendMode::Mode(s["normalMode"].toInt());
		tool.eraserMode = paintcore::BlendMode::Mode(s["eraserMode"].toInt());

		if(!d->shareBrushSlotColor)
			d->brushSlotButton(i)->setColorSwatch(tool.brush.color());
	}

	if(d->shareBrushSlotColor)
		d->ui.preview->setColor(d->toolSlots[0].brush.color());

	if(!d->toolSlots[ERASER_SLOT].brush.isEraser())
		d->toolSlots[ERASER_SLOT].brush.setBlendingMode(paintcore::BlendMode::MODE_ERASE);

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = d->current != ERASER_SLOT ? d->current : 0;
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {

	case tools::Tool::LINE: d->ui.preview->setPreviewShape(BrushPreview::Line); break;
	case tools::Tool::RECTANGLE: d->ui.preview->setPreviewShape(BrushPreview::Rectangle); break;
	case tools::Tool::ELLIPSE: d->ui.preview->setPreviewShape(BrushPreview::Ellipse); break;
	default: d->ui.preview->setPreviewShape(BrushPreview::Stroke); break;
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
	if(color != d->currentColor()) {
		d->currentBrush().setColor(color);
		if(!d->shareBrushSlotColor)
			d->brushSlotButton(d->current)->setColorSwatch(color);
		d->ui.preview->setColor(color);
	}
}

void BrushSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		d->ui.brushsizeBox->setValue(d->ui.brushsizeBox->value() + adj);
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

//// BRUSH PRESET PALETTE MODEL ////

static constexpr int BRUSH_ICON_SIZE = 42;

struct BrushPresetModel::Private {
	QList<ToolProperties> presets;
	mutable QList<QPixmap> iconcache;

	QPixmap getIcon(int idx) const {
#if 0 // TODO
		Q_ASSERT(idx >=0 && idx < presets.size());
		Q_ASSERT(presets.size() == iconcache.size());

		if(iconcache.at(idx).isNull()) {

			const brushes::ClassicBrush brush = brushFromProps(presets[idx], ToolProperties(), QColor());
			const int brushmode = presets[idx].value(brushprop::brushmode);
			paintcore::BrushMask mask;
			Q_ASSERT(brushmode>=0 && brushmode<=3);
			switch(brushmode) {
				case 0:
					mask = brushes::makeRoundPixelBrushMask(brush.size1(), brush.opacity1()*255);
					break;
				case 1:
					mask = brushes::makeSquarePixelBrushMask(brush.size1(), brush.opacity1()*255);
					break;
				case 2:
				case 3:
					mask = brushes::makeGimpStyleBrushStamp(QPointF(), brush.size1(), brush.hardness1(), brush.opacity1()).mask;
					break;
				default: return QPixmap();
			}

			const int maskdia = mask.diameter();
			QImage icon(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE, QImage::Format_ARGB32_Premultiplied);

			const QRgb color = (brushmode==3) ? 0x001d99f3 : (icon::isDarkThemeSelected() ? 0x00ffffff : 0);

			if(maskdia > BRUSH_ICON_SIZE) {
				// Clip to fit
				const int clip = (maskdia - BRUSH_ICON_SIZE);
				const uchar *m = mask.data() + (clip/2*maskdia) + clip/2;
				for(int y=0;y<BRUSH_ICON_SIZE;++y) {
					quint32 *scanline = reinterpret_cast<quint32*>(icon.scanLine(y));
					for(int x=0;x<BRUSH_ICON_SIZE;++x,++m) {
						*(scanline++) = qPremultiply((*m << 24) | color);
					}
					m += clip;
				}

			} else {
				// Center in the icon
				icon.fill(Qt::transparent);
				const uchar *m = mask.data();
				const int offset = (BRUSH_ICON_SIZE - maskdia)/2;
				for(int y=0;y<maskdia;++y) {
					quint32 *scanline = reinterpret_cast<quint32*>(icon.scanLine(y+offset)) + offset;
					for(int x=0;x<maskdia;++x,++m) {
						*(scanline++) = qPremultiply((*m << 24) | color);
					}
				}
			}

			iconcache[idx] = QPixmap::fromImage(icon);
		}
		return iconcache.at(idx);
#endif
		return QPixmap();
	}
};

BrushPresetModel::BrushPresetModel(QObject *parent)
	: QAbstractListModel(parent), d(new Private)
{
	loadBrushes();
	if(d->presets.isEmpty())
		makeDefaultBrushes();
}

BrushPresetModel::~BrushPresetModel()
{
	delete d;
}

BrushPresetModel *BrushPresetModel::getSharedInstance()
{
	static BrushPresetModel *m;
	if(!m)
		m = new BrushPresetModel;
	return m;
}

int BrushPresetModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return d->presets.size();
}

QVariant BrushPresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < d->presets.size()) {
		switch(role) {
		case Qt::DecorationRole: return d->getIcon(index.row());
		case Qt::SizeHintRole: return QSize(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE);
		//case Qt::ToolTipRole: return d->presets.at(index.row()).value(brushprop::label);
		case ToolPropertiesRole: return QVariant::fromValue(d->presets.at(index.row()));
		}
	}
	return QVariant();
}

Qt::ItemFlags BrushPresetModel::flags(const QModelIndex &index) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < d->presets.size()) {
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
	}
	return Qt::ItemIsSelectable | Qt::ItemIsEnabled |  Qt::ItemIsDropEnabled;
}

QMap<int,QVariant> BrushPresetModel::itemData(const QModelIndex &index) const
{
	QMap<int,QVariant> roles;
	if(index.isValid() && index.row()>=0 && index.row()<d->presets.size()) {
		roles[ToolPropertiesRole] = QVariant::fromValue(d->presets[index.row()]);
	}
	return roles;
}

bool BrushPresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid() || index.row()<0 || index.row()>=d->presets.size())
		return false;
	switch(role) {
		case ToolPropertiesRole:
			d->presets[index.row()] = value.value<ToolProperties>();
			d->iconcache[index.row()] = QPixmap();
			emit dataChanged(index, index);
			saveBrushes();
			return true;
	}
	return false;
}

bool BrushPresetModel::insertRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;
	if(row<0 || count<=0 || row > d->presets.size())
		return false;
	beginInsertRows(QModelIndex(), row, row+count-1);
	for(int i=0;i<count;++i) {
		d->presets.insert(row, ToolProperties());
		d->iconcache.insert(row, QPixmap());
	}
	endInsertRows();
	return true;
}

bool BrushPresetModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;
	if(row<0 || count<=0 || row+count > d->presets.size())
		return false;
	beginRemoveRows(QModelIndex(), row, row+count-1);
	d->presets.erase(d->presets.begin()+row, d->presets.begin()+row+count);
	d->iconcache.erase(d->iconcache.begin()+row, d->iconcache.begin()+row+count);
	endRemoveRows();
	saveBrushes();
	return true;
}

Qt::DropActions BrushPresetModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void BrushPresetModel::addBrush(const ToolProperties &brushProps)
{
	beginInsertRows(QModelIndex(), d->presets.size(), d->presets.size());
	d->presets.append(brushProps);
	d->iconcache.append(QPixmap());
	endInsertRows();
	saveBrushes();
}

void BrushPresetModel::loadBrushes()
{
#if 0 // TODO
	QSettings cfg;
	cfg.beginGroup("tools/brushpresets");
	int size = cfg.beginReadArray("preset");
	QList<ToolProperties> props;
	QList<QPixmap> iconcache;
	for(int i=0;i<size;++i) {
		cfg.setArrayIndex(i);
		props.append(ToolProperties::load(cfg));
		iconcache.append(QPixmap());
	}
	beginResetModel();
	d->presets = props;
	d->iconcache = iconcache;
	endResetModel();
#endif
}

void BrushPresetModel::saveBrushes() const
{
#if 0 // TODO
	QSettings cfg;
	cfg.beginGroup("tools/brushpresets");
	cfg.beginWriteArray("preset", d->presets.size());
	for(int i=0;i<d->presets.size();++i) {
		cfg.setArrayIndex(i);
		d->presets.at(i).save(cfg);
	}
	cfg.endArray();
#endif
}

void BrushPresetModel::makeDefaultBrushes()
{
#if 0
	QList<ToolProperties> brushes;
	ToolProperties tp;

	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 0);
		tp.setValue(brushprop::size, 16);
		tp.setValue(brushprop::opacity, 100);
		tp.setValue(brushprop::spacing, 15);
		tp.setValue(brushprop::sizePressure, true);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 2);
		tp.setValue(brushprop::size, 10);
		tp.setValue(brushprop::opacity, 100);
		tp.setValue(brushprop::hard, 80);
		tp.setValue(brushprop::spacing, 15);
		tp.setValue(brushprop::sizePressure, true);
		tp.setValue(brushprop::opacityPressure, true);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 2);
		tp.setValue(brushprop::size, 30);
		tp.setValue(brushprop::opacity, 34);
		tp.setValue(brushprop::hard, 100);
		tp.setValue(brushprop::spacing, 18);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 0);
		tp.setValue(brushprop::incremental, false);
		tp.setValue(brushprop::size, 32);
		tp.setValue(brushprop::opacity, 65);
		tp.setValue(brushprop::spacing, 15);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 0);
		tp.setValue(brushprop::incremental, false);
		tp.setValue(brushprop::size, 70);
		tp.setValue(brushprop::opacity, 42);
		tp.setValue(brushprop::spacing, 15);
		tp.setValue(brushprop::opacityPressure, true);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 2);
		tp.setValue(brushprop::size, 113);
		tp.setValue(brushprop::opacity, 60);
		tp.setValue(brushprop::hard, 1);
		tp.setValue(brushprop::spacing, 19);
		tp.setValue(brushprop::opacityPressure, true);
		brushes << tp;
	}
	{
		ToolProperties tp;
		tp.setValue(brushprop::brushmode, 3);
		tp.setValue(brushprop::size, 43);
		tp.setValue(brushprop::opacity, 30);
		tp.setValue(brushprop::hard, 100);
		tp.setValue(brushprop::spacing, 25);
		tp.setValue(brushprop::smudge, 100);
		tp.setValue(brushprop::resmudge, 1);
		tp.setValue(brushprop::opacityPressure, true);
		brushes << tp;
	}

	// Make presets
	beginInsertRows(QModelIndex(), d->presets.size(), d->presets.size()+brushes.size()-1);
	d->presets << brushes;
	for(int i=0;i<brushes.size();++i)
		d->iconcache << QPixmap();
	endInsertRows();
#endif
}

}


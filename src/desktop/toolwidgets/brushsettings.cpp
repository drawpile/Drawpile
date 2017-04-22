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

#include "brushsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "core/brushmask.h"

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

namespace tools {

namespace brushprop {
	// Brush properties
	static const QString
		LABEL = QStringLiteral("label"),
		SIZE = QStringLiteral("size"),
		SIZE_PRESSURE = QStringLiteral("sizep"),
		OPACITY = QStringLiteral("opacity"),
		OPACITY_PRESSURE = QStringLiteral("opacityp"),
		HARD = QStringLiteral("hard"),
		HARD_PRESSURE = QStringLiteral("hardp"),
		SMUDGE = QStringLiteral("smudge"),
		SMUDGE_PRESSURE = QStringLiteral("smudgep"),
		RESMUDGE = QStringLiteral("resmudge"),
		SPACING = QStringLiteral("spacing"),
		INCREMENTAL = QStringLiteral("incremental"),
		BRUSHMODE = QStringLiteral("brushmode") /* 0: hard edge, 1: soft edge, 2: watercolor */
		;
}

namespace toolprop {
	// Tool properties
	static const QString
		COLOR = QStringLiteral("color"),
		BLENDMODE = QStringLiteral("blendmode"),
		ERASEMODE = QStringLiteral("erasemode"),
		USE_ERASEMODE = QStringLiteral("use_erasemode")
		;
}

static paintcore::Brush brushFromProps(const ToolProperties &bp, const ToolProperties &tp)
{
	const int brushMode = bp.intValue(brushprop::BRUSHMODE, 0);

	paintcore::Brush b;
	b.setSize(bp.intValue(brushprop::SIZE, 1, 1, 255));
	if(bp.boolValue(brushprop::SIZE_PRESSURE, false))
		b.setSize2(1);
	else
		b.setSize2(b.size1());

	b.setOpacity(bp.intValue(brushprop::OPACITY, 100, 1, 100) / 100.0);
	if(bp.boolValue(brushprop::OPACITY_PRESSURE, false))
		b.setOpacity2(0);
	else
		b.setOpacity2(b.opacity1());

	if(brushMode == 0) {
		// Hard edge mode: hardness at full and no antialiasing
		b.setHardness(1);
		b.setHardness2(1);
		b.setSubpixel(false);

	} else {
		b.setHardness(bp.intValue(brushprop::HARD, 100, 1, 100) / 100.0);
		if(bp.boolValue(brushprop::HARD_PRESSURE, false))
			b.setHardness2(0);
		else
			b.setHardness2(b.hardness1());
		b.setSubpixel(true);
	}

	if(brushMode == 2) {
		b.setSmudge(bp.intValue(brushprop::SMUDGE, 0, 0, 100) / 100.0);
		if(bp.boolValue(brushprop::SMUDGE_PRESSURE, false))
			b.setSmudge2(0);
		else
			b.setSmudge2(b.smudge1());

		b.setResmudge(bp.intValue(brushprop::RESMUDGE, 3, 0, 255));

		// Watercolor mode requires incremental drawing
		b.setIncremental(true);

	} else {
		b.setSmudge(0);
		b.setSmudge2(0);
		b.setResmudge(0);
		b.setIncremental(bp.boolValue(brushprop::INCREMENTAL, true));
	}

	b.setSpacing(bp.intValue(brushprop::SPACING, 10, 0, 100));

	b.setColor(tp.value(toolprop::COLOR, QColor(Qt::black)).value<QColor>());

	if(tp.boolValue(toolprop::USE_ERASEMODE, false)) {
		b.setBlendingMode(paintcore::BlendMode::Mode(tp.intValue(toolprop::ERASEMODE, paintcore::BlendMode::MODE_ERASE)));
	} else {
		b.setBlendingMode(paintcore::BlendMode::Mode(tp.intValue(toolprop::BLENDMODE, paintcore::BlendMode::MODE_NORMAL)));
	}
	return b;
}

static const int BRUSH_COUNT = 6; // Last is the dedicated eraser slot
static const int ERASER_SLOT = 5; // Index of the dedicated erser slot

struct BrushSettings::Private {
	Ui_BrushDock ui;

	QStandardItemModel *blendModes, *eraseModes;
	BrushSettings *basicSettings;
	BrushPresetModel *presets;

	ToolProperties brushProps[BRUSH_COUNT];
	ToolProperties toolProps[BRUSH_COUNT];
	int current;
	int previousNonEraser;

	bool updateInProgress;

	inline ToolProperties &currentBrush() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return brushProps[current];
	}
	inline ToolProperties &currentTool() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return toolProps[current];
	}

	inline QColor currentColor() {
		return currentTool().value(toolprop::COLOR, QColor(Qt::black)).value<QColor>();
	}

	Private(BrushSettings *b)
		: current(0), previousNonEraser(0), updateInProgress(false)
	{
		blendModes = new QStandardItemModel(0, 1, b);
		for(const auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::BrushMode)) {
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

	void updateBrush()
	{
		// Update brush object from current properties
		paintcore::Brush b = brushFromProps(currentBrush(), currentTool());

		ui.preview->setBrush(b);
	}

	GroupedToolButton *brushSlotButton(int i)
	{
		Q_ASSERT(i>=0 && i < BRUSH_COUNT);
		switch(i) {
		case 0: return ui.slot1;
		case 1: return ui.slot2;
		case 2: return ui.slot3;
		case 3: return ui.slot4;
		case 4: return ui.slot5;
		case 5: return ui.slotEraser;
		default:
			qFatal("brushSlotButton(%d): no such button", i);
			return nullptr;
		}
	}
};

BrushSettings::BrushSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), d(new Private(this))
{
	d->basicSettings = this;
	d->presets = BrushPresetModel::getSharedInstance();
}

BrushSettings::~BrushSettings()
{
	delete d;
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	d->ui.setupUi(widget);

	QFontMetrics fm(d->ui.blendmode->font());
	int maxWidth = 0;
	for(const auto bm : paintcore::getBlendModeNames(0)) {
		maxWidth = qMax(maxWidth, fm.width(bm.second));
	}
	d->ui.blendmode->setFixedWidth(maxWidth + 20);

	// Outside communication
	connect(d->ui.brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(d->ui.preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	connect(d->ui.preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	// Internal updates
	connect(d->ui.blendmode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, &BrushSettings::selectBlendMode);
	connect(d->ui.modeEraser, &QToolButton::clicked, this, &BrushSettings::setEraserMode);

	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.hardedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.softedgeMode, &QToolButton::clicked, this, &BrushSettings::updateUi);
	connect(d->ui.watercolorMode, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.watercolorMode, &QToolButton::clicked, this, &BrushSettings::updateUi);

	connect(d->ui.brushsize, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureSize, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushopacity, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureOpacity, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushhardness, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureHardness, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.brushsmudging, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.pressureSmudging, &QToolButton::toggled, this, &BrushSettings::updateFromUi);

	connect(d->ui.colorpickup, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.brushspacing, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.modeIncremental, &QToolButton::clicked, this, &BrushSettings::updateFromUi);
	connect(d->ui.modeIndirect, &QToolButton::clicked, this, &BrushSettings::updateFromUi);

	// Brush slot buttons
	for(int i=0;i<BRUSH_COUNT;++i) {
		connect(d->brushSlotButton(i), &QToolButton::clicked, this, [this, i]() { selectBrushSlot(i); });
	}

	return widget;
}

void BrushSettings::setCurrentBrushSettings(const ToolProperties &brushProps)
{
	d->currentBrush() = brushProps;
	updateUi();
}

ToolProperties BrushSettings::getCurrentBrushSettings() const
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
	updateUi();

	emit colorChanged(d->currentColor());

	if((previousSlot==ERASER_SLOT) != (i==ERASER_SLOT))
		emit eraseModeChanged(i==ERASER_SLOT);
}

void BrushSettings::toggleEraserMode()
{
	if(d->current != ERASER_SLOT) {
		// Eraser mode is fixed in dedicated eraser slot
		d->currentTool().setValue(toolprop::USE_ERASEMODE, !d->currentTool().boolValue(toolprop::USE_ERASEMODE, false));
		updateUi();
	}
}

void BrushSettings::setEraserMode(bool erase)
{
	d->currentTool().setValue(toolprop::USE_ERASEMODE, erase);
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
	QString prop;
	if(d->currentTool().boolValue(toolprop::USE_ERASEMODE, false))
		prop = toolprop::ERASEMODE;
	else
		prop = toolprop::BLENDMODE;
	d->currentTool().setValue(prop, d->ui.blendmode->model()->index(modeIndex,0).data(Qt::UserRole).toInt());
	updateUi();
}

void BrushSettings::updateUi()
{
	// Update the UI to match the currently selected brush
	if(d->updateInProgress)
		return;

	d->updateInProgress = true;

	const ToolProperties &brush = d->currentBrush();
	const ToolProperties &tool = d->currentTool();

	// Select brush type
	const int brushMode = brush.intValue(brushprop::BRUSHMODE, 0);
	switch(brushMode) {
	case 1: d->ui.softedgeMode->setChecked(true); break;
	case 2: d->ui.watercolorMode->setChecked(true); break;
	case 0:
	default: d->ui.hardedgeMode->setChecked(true); break;
	}

	// Hide certain features based on the brush type
	d->ui.brushhardness->setVisible(brushMode != 0);
	d->ui.pressureHardness->setVisible(brushMode != 0);
	d->ui.hardnessLabel->setVisible(brushMode != 0);
	d->ui.hardnessBox->setVisible(brushMode != 0);

	d->ui.brushsmudging->setVisible(brushMode == 2);
	d->ui.pressureSmudging->setVisible(brushMode == 2);
	d->ui.smudgingLabel->setVisible(brushMode == 2);
	d->ui.smudgingBox->setVisible(brushMode == 2);

	d->ui.colorpickup->setVisible(brushMode == 2);
	d->ui.colorpickupLabel->setVisible(brushMode == 2);
	d->ui.colorpickupBox->setVisible(brushMode == 2);

	d->ui.modeIncremental->setEnabled(brushMode != 2);
	d->ui.modeIndirect->setEnabled(brushMode != 2);

	d->ui.brushsize->setValue(brush.intValue(brushprop::SIZE, 1));
	d->ui.brushopacity->setValue(brush.intValue(brushprop::OPACITY, 100));

	// Show correct blending mode
	int blendmode;
	const bool erasemode = tool.boolValue(toolprop::USE_ERASEMODE, false);
	if(erasemode) {
		d->ui.blendmode->setModel(d->eraseModes);
		blendmode = tool.intValue(toolprop::ERASEMODE, paintcore::BlendMode::MODE_ERASE);
	} else {
		d->ui.blendmode->setModel(d->blendModes);
		blendmode = tool.intValue(toolprop::BLENDMODE, paintcore::BlendMode::MODE_NORMAL);
	}
	d->ui.modeEraser->setChecked(erasemode);
	d->ui.modeEraser->setEnabled(d->current != ERASER_SLOT);

	for(int i=0;i<d->ui.blendmode->model()->rowCount();++i) {
		if(d->ui.blendmode->model()->index(i,0).data(Qt::UserRole) == blendmode) {
			d->ui.blendmode->setCurrentIndex(i);
			break;
		}
	}

	// Set values
	d->ui.brushsize->setValue(brush.intValue(brushprop::SIZE, 1));
	d->ui.pressureSize->setChecked(brush.boolValue(brushprop::SIZE_PRESSURE, false));

	d->ui.brushopacity->setValue(brush.intValue(brushprop::OPACITY, 100));
	d->ui.pressureOpacity->setChecked(brush.boolValue(brushprop::OPACITY_PRESSURE, false));

	d->ui.brushhardness->setValue(brush.intValue(brushprop::HARD, 100));
	d->ui.pressureHardness->setChecked(brushMode != 0 && brush.boolValue(brushprop::HARD_PRESSURE, false));

	d->ui.brushsmudging->setValue(brush.intValue(brushprop::SMUDGE, 50));
	d->ui.pressureSmudging->setChecked(brushMode == 2 && brush.boolValue(brushprop::SMUDGE_PRESSURE, false));

	d->ui.colorpickup->setValue(brush.intValue(brushprop::RESMUDGE, 3, 0, 255));

	d->ui.brushspacing->setValue(brush.intValue(brushprop::SPACING, 10));
	if(brush.boolValue(brushprop::INCREMENTAL, true))
		d->ui.modeIncremental->setChecked(true);
	else
		d->ui.modeIndirect->setChecked(true);

	d->updateInProgress = false;
	d->updateBrush();
}

void BrushSettings::updateFromUi()
{
	if(d->updateInProgress)
		return;

	// Copy changes from the UI to the brush properties object,
	// then update the brush
	ToolProperties &brush = d->currentBrush();

	if(d->ui.hardedgeMode->isChecked())
		brush.setValue(brushprop::BRUSHMODE, 0);
	else if(d->ui.softedgeMode->isChecked())
		brush.setValue(brushprop::BRUSHMODE, 1);
	else
		brush.setValue(brushprop::BRUSHMODE, 2);

	brush.setValue(brushprop::SIZE, d->ui.brushsize->value());
	brush.setValue(brushprop::SIZE_PRESSURE, d->ui.pressureSize->isChecked());

	brush.setValue(brushprop::OPACITY, d->ui.brushopacity->value());
	brush.setValue(brushprop::OPACITY_PRESSURE, d->ui.pressureOpacity->isChecked());

	brush.setValue(brushprop::HARD, d->ui.brushhardness->value());
	brush.setValue(brushprop::HARD_PRESSURE, d->ui.pressureHardness->isChecked());

	brush.setValue(brushprop::SMUDGE, d->ui.brushsmudging->value());
	brush.setValue(brushprop::SMUDGE_PRESSURE, d->ui.pressureSmudging->isChecked());

	brush.setValue(brushprop::RESMUDGE, d->ui.colorpickup->value());
	brush.setValue(brushprop::SPACING, d->ui.brushspacing->value());
	brush.setValue(brushprop::INCREMENTAL, d->ui.modeIncremental->isChecked());

	if(d->current == ERASER_SLOT)
		d->currentTool().setValue(toolprop::USE_ERASEMODE, true);
	else
		d->currentTool().setValue(toolprop::USE_ERASEMODE, d->ui.modeEraser->isChecked());

	d->updateBrush();
}

void BrushSettings::pushSettings()
{
	controller()->setActiveBrush(d->ui.preview->brush());
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	for(int i=0;i<BRUSH_COUNT;++i) {
		cfg.setValue(QString("brush%1").arg(i), d->brushProps[i].asVariant());
		cfg.setValue(QString("tool%1").arg(i), d->toolProps[i].asVariant());

	}
	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{
	d->current = qBound(0, cfg.value("active", 0).toInt(), BRUSH_COUNT-1);
	d->previousNonEraser = d->current;
	for(int i=0;i<BRUSH_COUNT;++i) {
		QVariantHash brush = cfg.value(QString("brush%1").arg(i)).toHash();
		QVariantHash tool = cfg.value(QString("tool%1").arg(i)).toHash();

		d->brushProps[i] = ToolProperties::fromVariant(brush);
		d->toolProps[i] = ToolProperties::fromVariant(tool);
		d->brushSlotButton(i)->setColorSwatch( d->toolProps[i].value(toolprop::COLOR, QColor(Qt::black)).value<QColor>());
	}
	d->toolProps[ERASER_SLOT].setValue(toolprop::USE_ERASEMODE, true);

	updateUi();
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {

	case tools::Tool::LINE: d->ui.preview->setPreviewShape(BrushPreview::Line); break;
	case tools::Tool::RECTANGLE: d->ui.preview->setPreviewShape(BrushPreview::Rectangle); break;
	case tools::Tool::ELLIPSE: d->ui.preview->setPreviewShape(BrushPreview::Ellipse); break;
	default: d->ui.preview->setPreviewShape(BrushPreview::Stroke); break;
	}
}

void BrushSettings::setForeground(const QColor& color)
{
	if(color != d->currentColor()) {
		d->currentTool().setValue(toolprop::COLOR, color);
		d->brushSlotButton(d->current)->setColorSwatch(color);
		d->ui.preview->setColor(color);
	}
}

void BrushSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		d->ui.brushsize->setValue(d->ui.brushsize->value() + adj);
}

int BrushSettings::getSize() const
{
	return d->ui.brushsize->value();
}

//// BRUSH PRESET PALETTE MODEL ////

static constexpr int BRUSH_ICON_SIZE = 42;

struct BrushPresetModel::Private {
	QList<ToolProperties> presets;
	mutable QList<QPixmap> iconcache;

	QPixmap getIcon(int idx) const {
		Q_ASSERT(idx >=0 && idx < presets.size());
		Q_ASSERT(presets.size() == iconcache.size());

		if(iconcache.at(idx).isNull()) {

			const paintcore::Brush brush = brushFromProps(presets[idx], ToolProperties());
			const paintcore::BrushStamp stamp = paintcore::makeGimpStyleBrushStamp(brush, paintcore::Point(0, 0, 1));
			const int maskdia = stamp.mask.diameter();
			QImage icon(BRUSH_ICON_SIZE, BRUSH_ICON_SIZE, QImage::Format_ARGB32_Premultiplied);

			const QRgb color = (presets[idx].intValue(brushprop::BRUSHMODE, 0)==2) ? 0x001d99f3 : 0;

			if(maskdia > BRUSH_ICON_SIZE) {
				// Clip to fit
				const int clip = (maskdia - BRUSH_ICON_SIZE);
				const uchar *m = stamp.mask.data() + (clip/2*maskdia) + clip/2;
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
				const uchar *m = stamp.mask.data();
				for(int y=0;y<maskdia;++y) {
					quint32 *scanline = reinterpret_cast<quint32*>(icon.scanLine(y+(BRUSH_ICON_SIZE-maskdia)/2)) + (BRUSH_ICON_SIZE-maskdia)/2;
					for(int x=0;x<maskdia;++x,++m) {
						*(scanline++) = qPremultiply((*m << 24) | color);
					}
				}
			}

			iconcache[idx] = QPixmap::fromImage(icon);
		}
		return iconcache.at(idx);
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
		case Qt::ToolTipRole: return d->presets.at(index.row()).value(brushprop::LABEL);
		case ToolPropertiesRole: return d->presets.at(index.row()).asVariant();
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
		roles[ToolPropertiesRole] = d->presets[index.row()].asVariant();
	}
	return roles;
}

bool BrushPresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid() || index.row()<0 || index.row()>=d->presets.size())
		return false;
	switch(role) {
		case ToolPropertiesRole:
			d->presets[index.row()] = ToolProperties::fromVariant(value.toHash());
			d->iconcache[index.row()] = QPixmap();
			emit dataChanged(index, index);
			return true;
	}
	return false;
}

bool BrushPresetModel::setItemData(const QModelIndex &index, const QMap<int,QVariant> &roles)
{
	if(!index.isValid() || index.row()<0 || index.row()>=d->presets.size())
		return false;

	if(roles.contains(ToolPropertiesRole)) {
		d->presets[index.row()] = ToolProperties::fromVariant(roles[ToolPropertiesRole].toHash());
		d->iconcache[index.row()] = QPixmap();
		emit dataChanged(index, index);
		saveBrushes();
	}
	return true;
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
}

void BrushPresetModel::saveBrushes() const
{
	QSettings cfg;
	cfg.beginGroup("tools/brushpresets");
	cfg.beginWriteArray("preset", d->presets.size());
	for(int i=0;i<d->presets.size();++i) {
		cfg.setArrayIndex(i);
		d->presets.at(i).save(cfg);
	}
	cfg.endArray();
}

void BrushPresetModel::makeDefaultBrushes()
{
	QList<ToolProperties> brushes;
	ToolProperties tp;

	// Generate a few pixel brushes
	tp.setValue(brushprop::BRUSHMODE, 0);
	tp.setValue(brushprop::SPACING, 15);
	tp.setValue(brushprop::SIZE, 0);
	tp.setValue(brushprop::SIZE_PRESSURE, true);
	for(int i=0;i<3;++i) {
		tp.setValue(brushprop::SIZE, i*8);
		brushes << tp;
	}

	// Generate a few soft brushes
	tp.setValue(brushprop::BRUSHMODE, 1);
	tp.setValue(brushprop::HARD, 80);
	for(int i=0;i<3;++i) {
		tp.setValue(brushprop::SIZE, i*8);
		brushes << tp;
	}

	// Generate a few watercolor brushes
	tp.setValue(brushprop::BRUSHMODE, 2);
	tp.setValue(brushprop::SMUDGE, 25);
	tp.setValue(brushprop::RESMUDGE, 6);
	for(int i=0;i<3;++i) {
		tp.setValue(brushprop::SIZE, i*8);
		brushes << tp;
	}

	// Make presets
	beginInsertRows(QModelIndex(), d->presets.size(), d->presets.size()+brushes.size()-1);
	d->presets << brushes;
	for(int i=0;i<brushes.size();++i)
		d->iconcache << QPixmap();
	endInsertRows();
}

}


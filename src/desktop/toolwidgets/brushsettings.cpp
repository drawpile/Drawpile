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
#include "ui_brushsettings.h"

#include <QMenu>
#include <QKeyEvent>
#include <QPainter>
#include <QMimeData>
#include <QSettings>

namespace tools {

namespace brushprop {
	// Brush properties
	static const QString
		LABEL = QStringLiteral("label"),
		SIZE = QStringLiteral("size"),
		SIZE2 = QStringLiteral("size2"),
		SIZE_PRESSURE = QStringLiteral("sizep"),
		OPACITY = QStringLiteral("opacity"),
		OPACITY2 = QStringLiteral("opacity2"),
		OPACITY_PRESSURE = QStringLiteral("opacityp"),
		HARD = QStringLiteral("hard"),
		HARD2 = QStringLiteral("hard2"),
		HARD_PRESSURE = QStringLiteral("hardp"),
		SMUDGE = QStringLiteral("smudge"),
		SMUDGE2 = QStringLiteral("smudge2"),
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
		b.setSize2(bp.intValue(brushprop::SIZE2, 1, 1, 255));
	else
		b.setSize2(b.size1());

	b.setOpacity(bp.intValue(brushprop::OPACITY, 100, 1, 100) / 100.0);
	if(bp.boolValue(brushprop::OPACITY_PRESSURE, false))
		b.setOpacity2(bp.intValue(brushprop::OPACITY2, 100, 1, 100) / 100.0);
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
			b.setHardness2(bp.intValue(brushprop::HARD2, 100, 1, 100) / 100.0);
		else
			b.setHardness2(b.hardness1());
		b.setSubpixel(true);
	}

	if(brushMode == 2) {
		b.setSmudge(bp.intValue(brushprop::SMUDGE, 0, 0, 100) / 100.0);
		if(bp.boolValue(brushprop::SMUDGE_PRESSURE, false))
			b.setSmudge2(bp.intValue(brushprop::SMUDGE, 0, 0, 100) / 100.0);
		else
			b.setSmudge2(b.smudge1());

		b.setResmudge(bp.intValue(brushprop::RESMUDGE, 1, 0, 255));

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

static const int BRUSH_COUNT = 5;

struct BrushSettings::Private {
	Ui_BrushDock ui;
	Ui_BrushSettings advUi;

	QMenu *blendModes, *eraseModes;
	BrushSettings *basicSettings;
	AdvancedBrushSettings *advancedSettings;
	BrushPresetModel *presets;

	ToolProperties brushProps[BRUSH_COUNT];
	ToolProperties toolProps[BRUSH_COUNT];
	int current;

	bool updateInProgress;

	inline ToolProperties &currentBrush() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return brushProps[current];
	}
	inline ToolProperties &currentTool() {
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return toolProps[current];
	}

	Private()
		: current(0), updateInProgress(false)
	{
		blendModes = new QMenu;
		for(const auto bm : paintcore::getBlendModeNames(paintcore::BlendMode::BrushMode)) {
			QAction *a = blendModes->addAction(bm.second);
			a->setProperty("blendmode", QVariant(bm.first));
		}

		eraseModes = new QMenu;
		QAction *e1 = eraseModes->addAction(QApplication::tr("Erase"));
		e1->setProperty("blendmode", QVariant(paintcore::BlendMode::MODE_ERASE));

		QAction *e2 = eraseModes->addAction(QApplication::tr("Color erase"));
		e2->setProperty("blendmode", QVariant(paintcore::BlendMode::MODE_COLORERASE));
	}

	void updateBrush()
	{
		// Update brush object from current properties
		paintcore::Brush b = brushFromProps(currentBrush(), currentTool());

		ui.preview->setBrush(b);
	}
};

BrushSettings::BrushSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), d(new Private)
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

	// Internal updtes
	connect(d->blendModes, &QMenu::triggered, this, &BrushSettings::selectBlendMode);
	connect(d->eraseModes, &QMenu::triggered, this, &BrushSettings::selectBlendMode);

	connect(d->ui.brushsize, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.brushopacity, &QSlider::valueChanged, this, &BrushSettings::updateFromUi);
	connect(d->ui.erasermode, &QToolButton::toggled, this, &BrushSettings::setEraserMode);

	// Brush slot buttons
	connect(d->ui.slot1, &QToolButton::clicked, this, [this]() { selectBrushSlot(0); });
	connect(d->ui.slot2, &QToolButton::clicked, this, [this]() { selectBrushSlot(1); });
	connect(d->ui.slot3, &QToolButton::clicked, this, [this]() { selectBrushSlot(2); });
	connect(d->ui.slot4, &QToolButton::clicked, this, [this]() { selectBrushSlot(3); });
	connect(d->ui.slot5, &QToolButton::clicked, this, [this]() { selectBrushSlot(4); });

	// Advanced settings
	d->advancedSettings = new AdvancedBrushSettings(this, widget);
	d->advancedSettings->setVisible(false);
	connect(d->ui.brushsettings, &QToolButton::clicked, this, &BrushSettings::showAdvancedSettings);

	// Brush presets
	d->ui.brushPaletteView->setModel(d->presets);
	d->ui.brushPaletteView->setDragEnabled(true);
	d->ui.brushPaletteView->viewport()->setAcceptDrops(true);
	connect(d->ui.brushPaletteView, &QAbstractItemView::clicked, this, [this](const QModelIndex &index) {
		QVariant v = index.data(BrushPresetModel::ToolPropertiesRole);
		if(v.isNull()) {
			qWarning("Brush preset was null!");
			return;
		}
		// Load preset
		ToolProperties tp = ToolProperties::fromVariant(v.toHash());
		d->currentBrush() = tp;
		updateUi();
		if(d->advancedSettings->isVisible())
			d->advancedSettings->updateUi();
	});

	connect(d->ui.presetAdd, &QAbstractButton::clicked, this, [this]() {
		d->presets->addBrush(d->currentBrush());
	});
	connect(d->ui.presetSave, &QAbstractButton::clicked, this, [this]() {
		auto sel = d->ui.brushPaletteView->selectionModel()->selectedIndexes();
		if(sel.isEmpty()) {
			qWarning("Cannot save brush preset: no selection!");
			return;
		}
		d->presets->setData(sel.first(), d->currentBrush().asVariant(), BrushPresetModel::ToolPropertiesRole);
	});
	connect(d->ui.presetDelete, &QAbstractButton::clicked, this, [this]() {
		auto sel = d->ui.brushPaletteView->selectionModel()->selectedIndexes();
		if(sel.isEmpty()) {
			qWarning("No brush preset selection to delete!");
			return;
		}
		d->presets->removeRow(sel.first().row());
	});

	return widget;
}

void BrushSettings::showAdvancedSettings()
{
	d->advancedSettings->showAt(d->ui.preview->mapToGlobal(QPoint(d->ui.preview->width()/2, d->ui.preview->height())));
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
	switch(i) {
	case 0: d->ui.slot1->setChecked(true); break;
	case 1: d->ui.slot2->setChecked(true); break;
	case 2: d->ui.slot3->setChecked(true); break;
	case 3: d->ui.slot4->setChecked(true); break;
	case 4: d->ui.slot5->setChecked(true); break;
	}

	d->current = i;
	updateUi();
	if(d->advancedSettings->isVisible())
		d->advancedSettings->updateUi();
}

bool BrushSettings::eraserMode() const
{
	return d->currentTool().boolValue(toolprop::USE_ERASEMODE, false);
}

void BrushSettings::setEraserMode(bool erase)
{
	d->currentTool().setValue(toolprop::USE_ERASEMODE, erase);
	updateUi();
}

void BrushSettings::selectBlendMode(QAction *modeSelectionAction)
{
	QString prop;
	if(d->currentTool().boolValue(toolprop::USE_ERASEMODE, false))
		prop = toolprop::ERASEMODE;
	else
		prop = toolprop::BLENDMODE;
	d->currentTool().setValue(prop, modeSelectionAction->property("blendmode"));
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

	d->ui.brushsize->setValue(brush.intValue(brushprop::SIZE, 1));
	d->ui.brushopacity->setValue(brush.intValue(brushprop::OPACITY, 100));

	int blendmode;
	if(tool.boolValue(toolprop::USE_ERASEMODE, false)) {
		d->ui.blendmode->setMenu(d->eraseModes);
		d->ui.erasermode->setChecked(true);
		blendmode = tool.intValue(toolprop::ERASEMODE, paintcore::BlendMode::MODE_ERASE);
	} else {
		d->ui.blendmode->setMenu(d->blendModes);
		d->ui.erasermode->setChecked(false);
		blendmode = tool.intValue(toolprop::BLENDMODE, paintcore::BlendMode::MODE_NORMAL);
	}
	d->ui.blendmode->setText(QApplication::tr(paintcore::findBlendMode(blendmode).name));

	if(d->advancedSettings->isVisible())
		d->advancedSettings->updateUi();

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
	brush.setValue(brushprop::SIZE, d->ui.brushsize->value());
	brush.setValue(brushprop::OPACITY, d->ui.brushopacity->value());

	if(d->advancedSettings->isVisible())
		d->advancedSettings->updateUi();

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
	for(int i=0;i<BRUSH_COUNT;++i) {
		QVariantHash brush = cfg.value(QString("brush%1").arg(i)).toHash();
		QVariantHash tool = cfg.value(QString("tool%1").arg(i)).toHash();

		d->brushProps[i] = ToolProperties::fromVariant(brush);
		d->toolProps[i] = ToolProperties::fromVariant(tool);
	}
	d->current = qBound(0, cfg.value("active", 0).toInt(), BRUSH_COUNT-1);
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
	d->currentTool().setValue(toolprop::COLOR, color);
	d->ui.preview->setColor(color);
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


///// ADVANCED BRUSH SETTINGS CLASS //////

AdvancedBrushSettings::AdvancedBrushSettings(BrushSettings *brushSettings, QWidget *parent)
	: QWidget(parent, Qt::Tool), d(brushSettings->d)
{
	d->advUi.setupUi(this);

	auto updateBrushMode = [this]() {
		updateFromUi();
		updateUi();
	};

	connect(d->advUi.hardedgeMode, &QToolButton::clicked, this, updateBrushMode);
	connect(d->advUi.softedgeMode, &QToolButton::clicked, this, updateBrushMode);
	connect(d->advUi.watercolorMode, &QToolButton::clicked, this, updateBrushMode);

	connect(d->advUi.brushLabel, &QLineEdit::textEdited, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushsize, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushsize0, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.sizePressure, &QCheckBox::toggled, this, &AdvancedBrushSettings::updateFromUi);

	connect(d->advUi.brushopacity, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushopacity0, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.opacityPressure, &QCheckBox::toggled, this, &AdvancedBrushSettings::updateFromUi);

	connect(d->advUi.brushhardness, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushhardness0, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.hardnessPressure, &QCheckBox::toggled, this, &AdvancedBrushSettings::updateFromUi);

	connect(d->advUi.brushsmudging, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushsmudging0, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.smudgingPressure, &QCheckBox::toggled, this, &AdvancedBrushSettings::updateFromUi);

	connect(d->advUi.colorpickup, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.brushspacing, &QSlider::valueChanged, this, &AdvancedBrushSettings::updateFromUi);
	connect(d->advUi.modeIncremental, &QCheckBox::clicked, this, &AdvancedBrushSettings::updateFromUi);

}

void AdvancedBrushSettings::showAt(const QPoint &point)
{
	updateUi();
	show();
	move(point.x() - width()/2, point.y());
	updateUi();
}

void AdvancedBrushSettings::keyPressEvent(QKeyEvent *e)
{
	if(e->matches(QKeySequence::Cancel)) {
		hide();
	} else {
		QWidget::keyPressEvent(e);
	}
}

void AdvancedBrushSettings::updateUi()
{
	if(d->updateInProgress)
		return;

	d->updateInProgress = true;
	const ToolProperties &brush = d->currentBrush();
	const int brushMode = brush.intValue(brushprop::BRUSHMODE, 0);
	switch(brushMode) {
	case 1: d->advUi.softedgeMode->setChecked(true); break;
	case 2: d->advUi.watercolorMode->setChecked(true); break;
	case 0:
	default: d->advUi.hardedgeMode->setChecked(true); break;
	}

	d->advUi.brushLabel->setText(brush.value(brushprop::LABEL).toString());

	// Hide certain features based on the mode
	d->advUi.brushhardness->setVisible(brushMode != 0);
	d->advUi.brushhardness0->setVisible(brushMode != 0  && brush.boolValue(brushprop::HARD_PRESSURE, false));
	d->advUi.hardnessPressure->setVisible(brushMode != 0);
	d->advUi.hardnessLabel->setVisible(brushMode != 0);
	d->advUi.hardnessBox->setVisible(brushMode != 0);
	d->advUi.hardnessSeparator->changeSize(10, brushMode != 0 ? 10 : 0, QSizePolicy::Ignored, QSizePolicy::Fixed);

	d->advUi.brushsmudging->setVisible(brushMode == 2);
	d->advUi.brushsmudging0->setVisible(brushMode == 2 && brush.boolValue(brushprop::SMUDGE_PRESSURE, false));
	d->advUi.smudgingPressure->setVisible(brushMode == 2);
	d->advUi.smudgingLabel->setVisible(brushMode == 2);
	d->advUi.smudgingBox->setVisible(brushMode == 2);
	d->advUi.smudgingSeparator->changeSize(10, brushMode == 2 ? 10 : 0, QSizePolicy::Ignored, QSizePolicy::Fixed);

	d->advUi.colorpickup->setVisible(brushMode == 2);
	d->advUi.colorpickupLabel->setVisible(brushMode == 2);
	d->advUi.colorpickupBox->setVisible(brushMode == 2);
	d->advUi.colorpickupSeparator->changeSize(10, brushMode == 2 ? 10 : 0, QSizePolicy::Ignored, QSizePolicy::Fixed);

	d->advUi.modeLabel->setVisible(brushMode != 2);
	d->advUi.modeIncremental->setVisible(brushMode != 2);

	// Set values
	d->advUi.brushsize->setValue(brush.intValue(brushprop::SIZE, 1));
	d->advUi.brushsize0->setValue(brush.intValue(brushprop::SIZE2, 1));
	d->advUi.sizePressure->setChecked(brush.boolValue(brushprop::SIZE_PRESSURE, false));

	d->advUi.brushopacity->setValue(brush.intValue(brushprop::OPACITY, 100));
	d->advUi.brushopacity0->setValue(brush.intValue(brushprop::OPACITY2, 100));
	d->advUi.opacityPressure->setChecked(brush.boolValue(brushprop::OPACITY_PRESSURE, false));

	d->advUi.brushhardness->setValue(brush.intValue(brushprop::HARD, 100));
	d->advUi.brushhardness0->setValue(brush.intValue(brushprop::HARD2, 100));
	d->advUi.hardnessPressure->setChecked(brushMode != 0 && brush.boolValue(brushprop::HARD_PRESSURE, false));

	d->advUi.brushsmudging->setValue(brush.intValue(brushprop::SMUDGE, 100));
	d->advUi.brushsmudging0->setValue(brush.intValue(brushprop::SMUDGE2, 100));
	d->advUi.smudgingPressure->setChecked(brushMode == 2 && brush.boolValue(brushprop::SMUDGE_PRESSURE, false));

	d->advUi.colorpickup->setValue(brush.intValue(brushprop::RESMUDGE, 1, 0));

	d->advUi.brushspacing->setValue(brush.intValue(brushprop::SPACING, 10));
	d->advUi.modeIncremental->setChecked(brush.boolValue(brushprop::INCREMENTAL, true));

	d->updateInProgress = false;
}

void AdvancedBrushSettings::updateFromUi()
{
	if(d->updateInProgress)
		return;

	ToolProperties &brush = d->currentBrush();

	brush.setValue(brushprop::LABEL, d->advUi.brushLabel->text());

	if(d->advUi.hardedgeMode->isChecked())
		brush.setValue(brushprop::BRUSHMODE, 0);
	else if(d->advUi.softedgeMode->isChecked())
		brush.setValue(brushprop::BRUSHMODE, 1);
	else
		brush.setValue(brushprop::BRUSHMODE, 2);

	brush.setValue(brushprop::SIZE, d->advUi.brushsize->value());
	brush.setValue(brushprop::SIZE2, d->advUi.brushsize0->value());
	brush.setValue(brushprop::SIZE_PRESSURE, d->advUi.sizePressure->isChecked());

	brush.setValue(brushprop::OPACITY, d->advUi.brushopacity->value());
	brush.setValue(brushprop::OPACITY2, d->advUi.brushopacity0->value());
	brush.setValue(brushprop::OPACITY_PRESSURE, d->advUi.opacityPressure->isChecked());

	brush.setValue(brushprop::HARD, d->advUi.brushhardness->value());
	brush.setValue(brushprop::HARD2, d->advUi.brushhardness0->value());
	brush.setValue(brushprop::HARD_PRESSURE, d->advUi.hardnessPressure->isChecked());

	brush.setValue(brushprop::SMUDGE, d->advUi.brushsmudging->value());
	brush.setValue(brushprop::SMUDGE2, d->advUi.brushsmudging0->value());
	brush.setValue(brushprop::SMUDGE_PRESSURE, d->advUi.smudgingPressure->isChecked());

	brush.setValue(brushprop::RESMUDGE, d->advUi.colorpickup->value());
	brush.setValue(brushprop::SPACING, d->advUi.brushspacing->value());
	brush.setValue(brushprop::INCREMENTAL, d->advUi.modeIncremental->isChecked());

	d->basicSettings->updateUi();
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
	tp.setValue(brushprop::SIZE2, 0);
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


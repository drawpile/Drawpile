/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/groupedtoolbutton.h"
#include "widgets/brushpreview.h"
using widgets::BrushPreview;
using widgets::GroupedToolButton;

#include "ui_pensettings.h"
#include "ui_brushsettings.h"
#include "ui_smudgesettings.h"
#include "ui_erasersettings.h"

namespace tools {

PenSettings::PenSettings(QString name, QString title, ToolController *ctrl)
	: ToolSettings(name, title, "draw-freehand", ctrl), _ui(nullptr)
{
}

PenSettings::~PenSettings()
{
	delete _ui;
}

QWidget *PenSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_PenSettings;
	_ui->setupUi(widget);

	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);
	return widget;
}

void PenSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->preview->setHardness(100);

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->preview->setSubpixel(false);
}

ToolProperties PenSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	return cfg;
}

void PenSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor(color);
}

void PenSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

int PenSettings::getSize() const
{
	return _ui->brushsize->value();
}

EraserSettings::EraserSettings(QString name, QString title, ToolController *ctrl)
	: ToolSettings(name, title, "draw-eraser", ctrl), _ui(nullptr)
{
}

EraserSettings::~EraserSettings()
{
	delete _ui;
}

QWidget *EraserSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_EraserSettings();
	_ui->setupUi(widget);

	_ui->preview->setBlendingMode(paintcore::BlendMode::MODE_ERASE);

	parent->connect(_ui->paintmodeHardedge, &QToolButton::toggled, [this](bool hard) {
		_ui->brushhardness->setEnabled(!hard);
		_ui->spinHardness->setEnabled(!hard);
		_ui->pressurehardness->setEnabled(!hard);
	});
	parent->connect(_ui->paintmodeHardedge, SIGNAL(toggled(bool)), parent, SLOT(updateSubpixelMode()));
	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	parent->connect(_ui->colorEraseMode, &QToolButton::toggled, [this](bool color) {
		_ui->preview->setBlendingMode(color ? paintcore::BlendMode::MODE_COLORERASE : paintcore::BlendMode::MODE_ERASE);
	});

	return widget;
}

ToolProperties EraserSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	cfg.setValue("hardedge", _ui->paintmodeHardedge->isChecked());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("colorerase", _ui->colorEraseMode->isChecked());
	return cfg;
}

void EraserSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->brushsize->setValue(cfg.intValue("size", 10));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	if(cfg.boolValue("hardedge", false))
		_ui->paintmodeHardedge->setChecked(true);
	else
		_ui->paintmodeSoftedge->setChecked(true);


	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->colorEraseMode->setChecked(cfg.boolValue("colorerase", false));

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());
}

void EraserSettings::setForeground(const QColor& color)
{
	// Foreground color is used only in color-erase mode
	_ui->preview->setColor(color);
}

void EraserSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

int EraserSettings::getSize() const
{
	return _ui->brushsize->value();
}

bool EraserSettings::getSubpixelMode() const
{
	return !_ui->paintmodeHardedge->isChecked();
}

BrushSettings::BrushSettings(QString name, QString title, ToolController *ctrl)
	: ToolSettings(name, title, "draw-brush", ctrl), _ui(nullptr)
{
}

BrushSettings::~BrushSettings()
{
	delete _ui;
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_BrushSettings;
	_ui->setupUi(widget);
	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	return widget;
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	_ui->preview->setSubpixel(true);
}

void BrushSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor(color);
}

void BrushSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

int BrushSettings::getSize() const
{
	return _ui->brushsize->value();
}

SmudgeSettings::SmudgeSettings(QString name, QString title, ToolController *ctrl)
	: ToolSettings(name, title, "draw-watercolor", ctrl), _ui(nullptr)
{
}

SmudgeSettings::~SmudgeSettings()
{
	delete _ui;
}

QWidget *SmudgeSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_SmudgeSettings;
	_ui->setupUi(widget);

	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	// Hardcoded value for now
	_ui->preview->setSmudgeFrequency(2);

	return widget;
}

ToolProperties SmudgeSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("smudge", _ui->brushsmudge->value());
	cfg.setValue("pressuresize", _ui->pressuresize->isChecked());
	cfg.setValue("pressureopacity", _ui->pressureopacity->isChecked());
	cfg.setValue("pressurehardness", _ui->pressurehardness->isChecked());
	cfg.setValue("pressuresmudge", _ui->pressuresmudge->isChecked());
	return cfg;
}

void SmudgeSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushsmudge->setValue(cfg.intValue("smudge", 50));
	_ui->preview->setSmudge(_ui->brushsmudge->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	_ui->pressuresize->setChecked(cfg.boolValue("pressuresize",false));
	_ui->preview->setSizePressure(_ui->pressuresize->isChecked());

	_ui->pressureopacity->setChecked(cfg.boolValue("pressureopacity",false));
	_ui->preview->setOpacityPressure(_ui->pressureopacity->isChecked());

	_ui->pressurehardness->setChecked(cfg.boolValue("pressurehardness",false));
	_ui->preview->setHardnessPressure(_ui->pressurehardness->isChecked());

	_ui->pressuresmudge->setChecked(cfg.boolValue("pressuresmudge",false));
	_ui->preview->setSmudgePressure(_ui->pressuresmudge->isChecked());

	_ui->preview->setSubpixel(true);
}

void SmudgeSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor(color);
}

void SmudgeSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

int SmudgeSettings::getSize() const
{
	return _ui->brushsize->value();
}

}


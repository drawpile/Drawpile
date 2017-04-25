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

#include "fillsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "tools/floodfill.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/brushpreview.h"
using widgets::BrushPreview;

#include "ui_fillsettings.h"

namespace tools {

FillSettings::FillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), _ui(nullptr)
{
}

FillSettings::~FillSettings()
{
	delete _ui;
}

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	_ui = new Ui_FillSettings;
	_ui->setupUi(uiwidget);

	connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	connect(_ui->tolerance, &QSlider::valueChanged, this, &FillSettings::pushSettings);
	connect(_ui->sizelimit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(_ui->expand, &QSlider::valueChanged, this, &FillSettings::pushSettings);
	connect(_ui->samplemerged, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->fillunder, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->erasermode, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->erasermode, &QAbstractButton::toggled, this, [this](bool erase) {
			_ui->preview->setPreviewShape(erase ? BrushPreview::FloodErase : BrushPreview::FloodFill);
			_ui->fillunder->setEnabled(!erase);
			_ui->samplemerged->setEnabled(!erase);
			_ui->preview->setTransparentBackground(!erase);
	});
	return uiwidget;
}

void FillSettings::pushSettings()
{
	const bool erase = _ui->erasermode->isChecked();
	auto *tool = static_cast<FloodFill*>(controller()->getTool(Tool::FLOODFILL));
	tool->setTolerance(_ui->tolerance->value());
	tool->setExpansion(_ui->expand->value());
	tool->setSizeLimit(_ui->sizelimit->value() * _ui->sizelimit->value() * 10 * 10);
	tool->setSampleMerged(erase ? false : _ui->samplemerged->isChecked());
	tool->setUnderFill(_ui->fillunder->isChecked());
	tool->setEraseMode(erase);
}

void FillSettings::toggleEraserMode()
{
	_ui->erasermode->toggle();
}

ToolProperties FillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue("tolerance", _ui->tolerance->value());
	cfg.setValue("expand", _ui->expand->value());
	cfg.setValue("samplemerged", _ui->samplemerged->isChecked());
	cfg.setValue("underfill", _ui->fillunder->isChecked());
	cfg.setValue("erasermode", _ui->erasermode->isChecked());
	return cfg;
}

void FillSettings::setForeground(const QColor &color)
{
	_ui->preview->setColor(color);
	paintcore::Brush b;
	b.setColor(color);
	controller()->setActiveBrush(b);
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->tolerance->setValue(cfg.value("tolerance", 0).toInt());
	_ui->expand->setValue(cfg.value("expand", 0).toInt());
	_ui->sizelimit->setValue(cfg.value("sizelimit", 50).toDouble());
	_ui->samplemerged->setChecked(cfg.value("samplemerged", true).toBool());
	_ui->fillunder->setChecked(cfg.value("underfill", true).toBool());
	_ui->erasermode->setChecked(cfg.value("erasermode", false).toBool());
	pushSettings();
}

void FillSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->tolerance->setValue(_ui->tolerance->value() + adj);
}

}


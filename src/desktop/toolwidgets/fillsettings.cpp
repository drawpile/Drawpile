/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2018 Calle Laakkonen

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

namespace props {
	static const ToolProperties::RangedValue<int>
		tolerance { QStringLiteral("tolerance"), 0, 0, 100 },
		expand { QStringLiteral("expand"), 0, 0, 100 }
		;
	static const ToolProperties::RangedValue<double>
		sizelimit { QStringLiteral("sizelimit"), 50.0, 0.0, 1000.0 }
		;
	static const ToolProperties::Value<bool>
		samplemerged { QStringLiteral("samplemerged"), true },
		underfill { QStringLiteral("underfill"), true },
		erasermode { QStringLiteral("erasermode"), false }
		;
}

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
	connect(_ui->sizelimit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FillSettings::pushSettings);
	connect(_ui->expand, &QSlider::valueChanged, this, &FillSettings::pushSettings);
	connect(_ui->samplemerged, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->fillunder, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->erasermode, &QAbstractButton::toggled, this, &FillSettings::pushSettings);
	connect(_ui->erasermode, &QAbstractButton::toggled, this, [this](bool erase) {
			_ui->preview->setPreviewShape(erase ? BrushPreview::FloodErase : BrushPreview::FloodFill);
			_ui->fillunder->setEnabled(!erase);
			_ui->samplemerged->setEnabled(!erase);
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
	cfg.setValue(props::tolerance, _ui->tolerance->value());
	cfg.setValue(props::expand, _ui->expand->value());
	cfg.setValue(props::samplemerged, _ui->samplemerged->isChecked());
	cfg.setValue(props::underfill, _ui->fillunder->isChecked());
	cfg.setValue(props::erasermode, _ui->erasermode->isChecked());
	return cfg;
}

void FillSettings::setForeground(const QColor &color)
{
	brushes::ClassicBrush b;
	b.setSize(1);
	b.setColor(color);
	_ui->preview->setBrush(b);
	controller()->setActiveBrush(b);
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->tolerance->setValue(cfg.value(props::tolerance));
	_ui->expand->setValue(cfg.value(props::expand));
	_ui->sizelimit->setValue(cfg.value(props::sizelimit));
	_ui->samplemerged->setChecked(cfg.value(props::samplemerged));
	_ui->fillunder->setChecked(cfg.value(props::underfill));
	_ui->erasermode->setChecked(cfg.value(props::erasermode));
	pushSettings();
}

void FillSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->tolerance->setValue(_ui->tolerance->value() + adj);
}

}


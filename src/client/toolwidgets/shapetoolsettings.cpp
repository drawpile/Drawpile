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

#include "shapetoolsettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/groupedtoolbutton.h"
#include "widgets/brushpreview.h"
using widgets::BrushPreview;
using widgets::GroupedToolButton;

#include "ui_simplesettings.h"

namespace tools {

SimpleSettings::SimpleSettings(const QString &name, const QString &title, const QString &icon, Type type, bool sp, ToolController *ctrl)
	: ToolSettings(name, title, icon, ctrl), _ui(nullptr), _type(type), _subpixel(sp)
{
}

SimpleSettings::~SimpleSettings()
{
	delete _ui;
}

QWidget *SimpleSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_SimpleSettings;
	_ui->setupUi(widget);

	populateBlendmodeBox(_ui->blendmode, _ui->preview);

	// Connect size change signal
	parent->connect(_ui->brushsize, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	parent->connect(_ui->paintmodeHardedge, &QToolButton::toggled, [this](bool hard) {
		_ui->brushhardness->setEnabled(!hard);
		_ui->spinHardness->setEnabled(!hard);
	});

	// Set proper preview shape
	if(_type==Line)
		_ui->preview->setPreviewShape(BrushPreview::Line);
	else if(_type==Rectangle)
		_ui->preview->setPreviewShape(BrushPreview::Rectangle);
	else if(_type==Ellipse)
		_ui->preview->setPreviewShape(BrushPreview::Ellipse);

	_ui->preview->setSubpixel(_subpixel);

	if(!_subpixel) {
		// Hard edge mode is always enabled for tools that do not support antialiasing
		_ui->paintmodeHardedge->hide();
		_ui->paintmodeSmoothedge->hide();
	} else {
		parent->connect(_ui->paintmodeHardedge, SIGNAL(toggled(bool)), parent, SLOT(updateSubpixelMode()));
	}

	parent->connect(_ui->preview, SIGNAL(requestColorChange()), parent, SLOT(changeForegroundColor()));
	parent->connect(_ui->preview, &BrushPreview::brushChanged, controller(), &ToolController::setActiveBrush);

	return widget;
}

ToolProperties SimpleSettings::saveToolSettings()
{
	ToolProperties cfg;
	cfg.setValue("blendmode", _ui->blendmode->currentIndex());
	cfg.setValue("size", _ui->brushsize->value());
	cfg.setValue("opacity", _ui->brushopacity->value());
	cfg.setValue("hardness", _ui->brushhardness->value());
	cfg.setValue("spacing", _ui->brushspacing->value());
	cfg.setValue("hardedge", _ui->paintmodeHardedge->isChecked());
	cfg.setValue("incremental", _ui->paintmodeIncremental->isChecked());
	return cfg;
}

void SimpleSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->blendmode->setCurrentIndex(cfg.intValue("blendmode", 0));

	_ui->brushsize->setValue(cfg.intValue("size", 0));
	_ui->preview->setSize(_ui->brushsize->value());

	_ui->brushopacity->setValue(cfg.intValue("opacity", 100));
	_ui->preview->setOpacity(_ui->brushopacity->value());

	_ui->brushhardness->setValue(cfg.intValue("hardness", 50));
	_ui->preview->setHardness(_ui->brushhardness->value());

	_ui->brushspacing->setValue(cfg.intValue("spacing", 15));
	_ui->preview->setSpacing(_ui->brushspacing->value());

	if(cfg.boolValue("hardedge", false))
		_ui->paintmodeHardedge->setChecked(true);
	else
		_ui->paintmodeSmoothedge->setChecked(true);

	if(cfg.boolValue("incremental", true))
		_ui->paintmodeIncremental->setChecked(true);
	else
		_ui->paintmodeIndirect->setChecked(true);

	_ui->preview->setIncremental(_ui->paintmodeIncremental->isChecked());
}

void SimpleSettings::setForeground(const QColor& color)
{
	_ui->preview->setColor(color);
}

void SimpleSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->brushsize->setValue(_ui->brushsize->value() + adj);
}

int SimpleSettings::getSize() const
{
	return _ui->brushsize->value();
}

bool SimpleSettings::getSubpixelMode() const
{
	return !_ui->paintmodeHardedge->isChecked();
}

}


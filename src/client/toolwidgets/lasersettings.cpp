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

#include "lasersettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "tools/laser.h"

// Work around lack of namespace support in Qt designer (TODO is the problem in our plugin?)
#include "widgets/colorbutton.h"
using widgets::ColorButton;

#include "ui_lasersettings.h"

namespace tools {

LaserPointerSettings::LaserPointerSettings(const QString &name, const QString &title, ToolController *ctrl)
	: QObject(), ToolSettings(name, title, "cursor-arrow", ctrl), _ui(nullptr)
{
}

LaserPointerSettings::~LaserPointerSettings()
{
	delete _ui;
}

void LaserPointerSettings::updateTool()
{
	auto *tool = static_cast<LaserPointer*>(controller()->getTool(Tool::LASERPOINTER));
	tool->setPersistence(_ui->persistence->value());

	QColor c;
	if(_ui->color0->isChecked())
		c = _ui->color0->color();
	else if(_ui->color1->isChecked())
		c = _ui->color1->color();
	else if(_ui->color2->isChecked())
		c = _ui->color2->color();
	else if(_ui->color3->isChecked())
		c = _ui->color3->color();

	paintcore::Brush b;
	b.setColor(c);
	controller()->setActiveBrush(b);
}

QWidget *LaserPointerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_LaserSettings;
	_ui->setupUi(widget);

	connect(_ui->trackpointer, SIGNAL(clicked(bool)), this, SIGNAL(pointerTrackingToggled(bool)));
	connect(_ui->persistence, &QSlider::valueChanged, this, &LaserPointerSettings::updateTool);
	connect(_ui->color0, &QAbstractButton::toggled, this, &LaserPointerSettings::updateTool);
	connect(_ui->color1, &QAbstractButton::toggled, this, &LaserPointerSettings::updateTool);
	connect(_ui->color2, &QAbstractButton::toggled, this, &LaserPointerSettings::updateTool);
	connect(_ui->color3, &QAbstractButton::toggled, this, &LaserPointerSettings::updateTool);

	return widget;
}

ToolProperties LaserPointerSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue("tracking", _ui->trackpointer->isChecked());
	cfg.setValue("persistence", _ui->persistence->value());

	int color=0;

	if(_ui->color1->isChecked())
		color=1;
	else if(_ui->color2->isChecked())
		color=2;
	else if(_ui->color3->isChecked())
		color=3;
	cfg.setValue("color", color);

	return cfg;
}

void LaserPointerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->trackpointer->setChecked(cfg.boolValue("tracking", true));
	_ui->persistence->setValue(cfg.intValue("persistence", 1));

	switch(cfg.intValue("color", 0, 0, 3)) {
	case 0: _ui->color0->setChecked(true); break;
	case 1: _ui->color1->setChecked(true); break;
	case 2: _ui->color2->setChecked(true); break;
	case 3: _ui->color3->setChecked(true); break;
	}
}

bool LaserPointerSettings::pointerTracking() const
{
	return _ui->trackpointer->isChecked();
}

void LaserPointerSettings::setForeground(const QColor &color)
{
	_ui->color0->setColor(color);
	updateTool();
}

void LaserPointerSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		_ui->persistence->setValue(_ui->persistence->value() + adj);
}

}


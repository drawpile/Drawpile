// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/toolwidgets/lasersettings.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/laser.h"

#include "ui_lasersettings.h"

namespace tools {

namespace props {
	static const ToolProperties::RangedValue<int>
		persistence { QStringLiteral("persistence"), 1, 1, 15 },
		color { QStringLiteral("color"), 0, 0, 3}
		;
	static const ToolProperties::Value<bool>
		tracking { QStringLiteral("tracking"), true }
		;
}

LaserPointerSettings::LaserPointerSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), _ui(nullptr)
{
}

LaserPointerSettings::~LaserPointerSettings()
{
	delete _ui;
}

void LaserPointerSettings::pushSettings()
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

	brushes::ActiveBrush b;
	b.setQColor(c);
	controller()->setActiveBrush(b);
}

QWidget *LaserPointerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	_ui = new Ui_LaserSettings;
	_ui->setupUi(widget);

	connect(_ui->trackpointer, SIGNAL(clicked(bool)), this, SIGNAL(pointerTrackingToggled(bool)));
	connect(_ui->persistence, QOverload<int>::of(&QSpinBox::valueChanged), this, &LaserPointerSettings::pushSettings);
	connect(_ui->color0, &QAbstractButton::toggled, this, &LaserPointerSettings::pushSettings);
	connect(_ui->color1, &QAbstractButton::toggled, this, &LaserPointerSettings::pushSettings);
	connect(_ui->color2, &QAbstractButton::toggled, this, &LaserPointerSettings::pushSettings);
	connect(_ui->color3, &QAbstractButton::toggled, this, &LaserPointerSettings::pushSettings);

	return widget;
}

ToolProperties LaserPointerSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::tracking, _ui->trackpointer->isChecked());
	cfg.setValue(props::persistence, _ui->persistence->value());

	int color=0;

	if(_ui->color1->isChecked())
		color=1;
	else if(_ui->color2->isChecked())
		color=2;
	else if(_ui->color3->isChecked())
		color=3;
	cfg.setValue(props::color, color);

	return cfg;
}

void LaserPointerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	_ui->trackpointer->setChecked(cfg.value(props::tracking));
	_ui->persistence->setValue(cfg.value(props::persistence));

	switch(cfg.value(props::color)) {
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
	pushSettings();
}

void LaserPointerSettings::quickAdjust1(qreal adjustment)
{
	m_quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(m_quickAdjust1, &i);
	if(int(i)) {
		m_quickAdjust1 = f;
		_ui->persistence->setValue(_ui->persistence->value() + int(i));
	}
}

void LaserPointerSettings::stepAdjust1(bool increase)
{
	QSpinBox *persistence = _ui->persistence;
	persistence->setValue(stepLogarithmic(
		persistence->minimum(), persistence->maximum(), persistence->value(),
		increase));
}

}


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

#include "colorpickersettings.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "tools/colorpicker.h"
#include "widgets/palettewidget.h"


#include <QBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>

namespace tools {

ColorPickerSettings::ColorPickerSettings(const QString &name, const QString &title, ToolController *ctrl)
	:  QObject(), ToolSettings(name, title, "color-picker", ctrl)
{
	m_palette.setColumns(8);
}

ColorPickerSettings::~ColorPickerSettings()
{
}

QWidget *ColorPickerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setMargin(3);
	widget->setLayout(layout);

	QHBoxLayout *sizelayout = new QHBoxLayout;
	layout->addLayout(sizelayout);

	QLabel *sizelbl = new QLabel(tr("Size:"), widget);
	sizelayout->addWidget(sizelbl);

	QSlider *slider = new QSlider(widget);
	slider->setOrientation(Qt::Horizontal);
	sizelayout->addWidget(slider);

	m_size = new QSpinBox(widget);
	sizelayout->addWidget(m_size);

	m_size->setMinimum(1);
	slider->setMinimum(1);

	m_size->setMaximum(128);
	slider->setMaximum(128);

	m_layerpick = new QCheckBox(tr("Pick from current layer only"), widget);
	layout->addWidget(m_layerpick);

	m_palettewidget = new widgets::PaletteWidget(widget);
	m_palettewidget->setPalette(&m_palette);
	layout->addWidget(m_palettewidget);

	connect(m_palettewidget, &widgets::PaletteWidget::colorSelected, this, &ColorPickerSettings::colorSelected);
	connect(m_size, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(slider, &QSlider::valueChanged, m_size, &QSpinBox::setValue);
	connect(m_size, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));
	connect(slider, &QSlider::valueChanged, this, &ColorPickerSettings::pushSettings);
	connect(m_layerpick, &QCheckBox::toggled, this, &ColorPickerSettings::pushSettings);

	return widget;
}

void ColorPickerSettings::pushSettings()
{
	auto *tool = static_cast<ColorPicker*>(controller()->getTool(Tool::PICKER));
	tool->setSize(m_size->value());
	tool->setPickFromCurrentLayer(m_layerpick->isChecked());
}

int ColorPickerSettings::getSize() const
{
	return m_size->value();
}

void ColorPickerSettings::quickAdjust1(float adjustment)
{
	int adj = qRound(adjustment);
	if(adj!=0)
		m_size->setValue(m_size->value() + adj);
}

ToolProperties ColorPickerSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue("layerpick", m_layerpick->isChecked());
	cfg.setValue("size", m_size->value());
	return cfg;
}

void ColorPickerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_layerpick->setChecked(cfg.boolValue("layerpick", false));
	m_size->setValue(cfg.intValue("size", 1));
}

void ColorPickerSettings::addColor(const QColor &color)
{
	if(m_palette.count() && m_palette.color(0).color == color)
		return;

	m_palette.insertColor(0, color);

	if(m_palette.count() > 80)
		m_palette.removeColor(m_palette.count()-1);

	m_palettewidget->update();
}

}


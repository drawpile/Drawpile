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

#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/dialogs/colordialog.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/colorpicker.h"
#include "desktop/widgets/kis_slider_spin_box.h"

#include <QtColorWidgets/swatch.hpp>

#include <QBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

namespace tools {

namespace props {
	static const ToolProperties::Value<bool>
		layerPick { QStringLiteral("layerpick"), false }
		;
	static const ToolProperties::RangedValue<int>
		size { QStringLiteral("size"), 1, 0, 255 }
		;
}

ColorPickerSettings::ColorPickerSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

ColorPickerSettings::~ColorPickerSettings()
{
}

QWidget *ColorPickerSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(3, 3, 3, 3);
	widget->setLayout(layout);

	m_size = new KisSliderSpinBox;
	m_size->setPrefix(tr("Size: "));
	m_size->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_size->setMinimum(1);
	m_size->setMaximum(128);
	layout->addWidget(m_size);

	m_layerpick = new QCheckBox(tr("Pick from current layer only"), widget);
	layout->addWidget(m_layerpick);

	m_palettewidget = new color_widgets::Swatch(widget);
	m_palettewidget->palette().setColumns(16);
	m_palettewidget->setReadOnly(true);
	layout->addWidget(m_palettewidget);

	QPushButton *addButton = new QPushButton{
		icon::fromTheme("list-add"), tr("Add Color..."), widget};
	layout->addWidget(addButton);

	connect(m_palettewidget, &color_widgets::Swatch::colorSelected, this, &ColorPickerSettings::colorSelected);
	connect(m_size, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(m_size, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerSettings::pushSettings);
	connect(m_layerpick, &QCheckBox::toggled, this, &ColorPickerSettings::pushSettings);
	connect(addButton, &QPushButton::pressed, this, &ColorPickerSettings::openColorDialog);

	return widget;
}

void ColorPickerSettings::pushSettings()
{
	auto *tool = static_cast<ColorPicker*>(controller()->getTool(Tool::PICKER));
	tool->setSize(m_size->value());
	tool->setPickFromCurrentLayer(m_layerpick->isChecked());
}

void ColorPickerSettings::openColorDialog()
{
	color_widgets::ColorPalette &palette = m_palettewidget->palette();
	color_widgets::ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(
		palette.count() == 0 ? Qt::black : palette.colorAt(0), m_palettewidget);
	connect(dlg, &color_widgets::ColorDialog::colorSelected, this, &ColorPickerSettings::selectColorFromDialog);
	dlg->show();
}

void ColorPickerSettings::selectColorFromDialog(const QColor &color)
{
	addColor(color);
	emit colorSelected(color);
}

int ColorPickerSettings::getSize() const
{
	return m_size->value();
}

void ColorPickerSettings::quickAdjust1(qreal adjustment)
{
	m_quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(m_quickAdjust1, &i);
	if(int(i)) {
		m_quickAdjust1 = f;
		m_size->setValue(m_size->value() + int(i));
	}
}

void ColorPickerSettings::stepAdjust1(bool increase)
{
	m_size->setValue(stepLogarithmic(
		m_size->minimum(), m_size->maximum(), m_size->value(), increase));
}

ToolProperties ColorPickerSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::layerPick, m_layerpick->isChecked());
	cfg.setValue(props::size, m_size->value());
	return cfg;
}

void ColorPickerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_layerpick->setChecked(cfg.value(props::layerPick));
	m_size->setValue(cfg.value(props::size));
}

void ColorPickerSettings::addColor(const QColor &color)
{
	auto &palette = m_palettewidget->palette();
	palette.insertColor(0, color);

	int i = 1;
	while(i < palette.count()) {
		if(palette.colorAt(i) == color) {
			palette.eraseColor(i);
		} else {
			++i;
		}
	}

	if(palette.count() > 80)
		palette.eraseColor(palette.count()-1);
}

}


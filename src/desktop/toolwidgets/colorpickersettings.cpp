// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/palettewidget.h"
#include "libclient/tools/colorpicker.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>
#include <QSpinBox>
#include <QtColorWidgets/color_utils.hpp>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> layerPick{
	QStringLiteral("layerpick"), false};
static const ToolProperties::RangedValue<int> size{
	QStringLiteral("size"), 1, 0, 255};
}

namespace colorpickersettings {

HeaderWidget::HeaderWidget(QWidget *parent)
	: QWidget{parent}
{
}

void HeaderWidget::pickFromScreen()
{
	if(!m_picking) {
		setFocusPolicy(Qt::StrongFocus);
		setFocus();
		grabMouse(Qt::CrossCursor);
		grabKeyboard();
		m_picking = true;
		emit pickingChanged(true);
	}
}

void HeaderWidget::stopPickingFromScreen()
{
	m_picking = false;
	emit pickingChanged(false);
	releaseKeyboard();
	releaseMouse();
	setFocusPolicy(Qt::NoFocus);
}

void HeaderWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_picking) {
		emit colorPicked(
			color_widgets::utils::get_screen_color(compat::globalPos(*event)));
		stopPickingFromScreen();
		event->accept();
	} else {
		QWidget::mouseReleaseEvent(event);
	}
}

}

ColorPickerSettings::ColorPickerSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

ColorPickerSettings::~ColorPickerSettings() {}

QWidget *ColorPickerSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new colorpickersettings::HeaderWidget{parent};
	QHBoxLayout *headerLayout = new QHBoxLayout;
	headerLayout->setSpacing(0);
	headerLayout->setContentsMargins(0, 0, 4, 0);
	m_headerWidget->setLayout(headerLayout);

#ifndef SINGLE_MAIN_WINDOW
	m_pickButton = new widgets::GroupedToolButton;
	m_pickButton->setIcon(QIcon::fromTheme("monitor"));
	m_pickButton->setToolTip(tr("Pick from screen"));
	m_pickButton->setCheckable(true);
	headerLayout->addWidget(m_pickButton);
#endif

	headerLayout->addSpacing(4);

	m_size = new KisSliderSpinBox;
	m_size->setPrefix(tr("Size: "));
	m_size->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_size->setMinimum(1);
	m_size->setMaximum(128);
	headerLayout->addWidget(m_size);

	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(3, 3, 3, 3);
	widget->setLayout(layout);

	m_layerpick = new QCheckBox(tr("Pick from current layer only"), widget);
	layout->addWidget(m_layerpick);

	m_palettewidget = new widgets::PaletteWidget;
	m_palettewidget->setColumns(9);
	layout->addWidget(m_palettewidget, 1);

	connect(
		m_palettewidget, &widgets::PaletteWidget::colorSelected, this,
		&ColorPickerSettings::colorSelected);
	connect(
		m_headerWidget, &colorpickersettings::HeaderWidget::colorPicked, this,
		&ColorPickerSettings::selectColor);
	connect(
		m_size, SIGNAL(valueChanged(int)), parent, SIGNAL(sizeChanged(int)));
	connect(
		m_size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorPickerSettings::pushSettings);
	connect(
		m_layerpick, &QCheckBox::toggled, this,
		&ColorPickerSettings::pushSettings);
#ifndef SINGLE_MAIN_WINDOW
	connect(
		m_headerWidget, &colorpickersettings::HeaderWidget::pickingChanged,
		m_pickButton, &QAbstractButton::setChecked);
	connect(
		m_pickButton, &QAbstractButton::clicked, this,
		&ColorPickerSettings::pickFromScreen);
#endif

	return widget;
}

void ColorPickerSettings::pushSettings()
{
	ColorPicker *tool =
		static_cast<ColorPicker *>(controller()->getTool(Tool::PICKER));
	tool->setSize(m_size->value());
	tool->setPickFromCurrentLayer(m_layerpick->isChecked());
}

void ColorPickerSettings::selectColor(const QColor &color)
{
	addColor(color);
	emit colorSelected(color);
}

void ColorPickerSettings::startPickFromScreen()
{
	if(m_pickButton && !m_pickingFromScreen) {
		m_pickButton->click();
	}
}

void ColorPickerSettings::cancelPickFromScreen()
{
	if(m_headerWidget) {
		m_headerWidget->stopPickingFromScreen();
	}
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
	color_widgets::ColorPalette &palette = m_palettewidget->colorPalette();
	palette.insertColor(0, color);
	m_palettewidget->setNextColor(color);

	int i = 1;
	while(i < palette.count()) {
		if(palette.colorAt(i) == color) {
			palette.eraseColor(i);
		} else {
			++i;
		}
	}

	if(palette.count() > 80)
		palette.eraseColor(palette.count() - 1);
}

void ColorPickerSettings::pickFromScreen()
{
	m_headerWidget->pickFromScreen();
}

}

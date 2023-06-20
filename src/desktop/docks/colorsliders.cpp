// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/colorsliders.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"

#include <QtColorWidgets/hue_slider.hpp>
#include <QtColorWidgets/swatch.hpp>

#include <QLabel>
#include <QGridLayout>
#include <QSpinBox>

namespace docks {

struct ColorSliderDock::Private {
	color_widgets::Swatch *lastUsedSwatch = nullptr;

	color_widgets::HueSlider *hue = nullptr;
	color_widgets::GradientSlider *saturation = nullptr;
	color_widgets::GradientSlider *value = nullptr;
	color_widgets::GradientSlider *red = nullptr;
	color_widgets::GradientSlider *green = nullptr;
	color_widgets::GradientSlider *blue = nullptr;

	QSpinBox *huebox = nullptr;
	QSpinBox *saturationbox = nullptr;
	QSpinBox *valuebox = nullptr;
	QSpinBox *redbox = nullptr;
	QSpinBox *greenbox = nullptr;
	QSpinBox *bluebox = nullptr;

    bool updating = false;
};

ColorSliderDock::ColorSliderDock(const QString& title, QWidget *parent)
	: DockBase(title, parent), d(new Private)
{
	// Create title bar widget
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
	d->lastUsedSwatch->setForcedColumns(docks::ToolSettings::LASTUSED_COLOR_COUNT);
	d->lastUsedSwatch->setReadOnly(true);
	d->lastUsedSwatch->setBorder(Qt::NoPen);
	d->lastUsedSwatch->setMinimumHeight(24);

	titlebar->addSpace(24);
	titlebar->addCustomWidget(d->lastUsedSwatch, true);
	titlebar->addSpace(24);

	connect(d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this, &ColorSliderDock::colorSelected);

	// Create main UI widget
	QWidget *w = new QWidget;
	auto *layout = new QGridLayout;
	w->setLayout(layout);

	int row = 0;

	d->hue = new color_widgets::HueSlider(w);
	d->hue->setMaximum(359);
	d->huebox = new QSpinBox(w);
	d->huebox->setMaximum(359);
	d->hue->setMaximumHeight(d->huebox->height());
	layout->addWidget(new QLabel{tr("H"), w}, row, 0);
	layout->addWidget(d->hue, row, 1);
	layout->addWidget(d->huebox, row, 2);

	++row;

	d->saturation = new color_widgets::GradientSlider(w);
	d->saturation->setMaximum(255);
	d->saturationbox = new QSpinBox(w);
	d->saturationbox->setMaximum(255);
	d->saturation->setMaximumHeight(d->saturationbox->height());
	layout->addWidget(new QLabel{tr("S"), w}, row, 0);
	layout->addWidget(d->saturation, row, 1);
	layout->addWidget(d->saturationbox, row, 2);

	++row;

	d->value = new color_widgets::GradientSlider(w);
	d->value->setMaximum(255);
	d->valuebox = new QSpinBox(w);
	d->valuebox->setMaximum(255);
	d->value->setMaximumHeight(d->valuebox->height());
	layout->addWidget(new QLabel{tr("V"), w}, row, 0);
	layout->addWidget(d->value, row, 1);
	layout->addWidget(d->valuebox, row, 2);

	++row;

	d->red = new color_widgets::GradientSlider(w);
	d->red->setMaximum(255);
	d->redbox = new QSpinBox(w);
	d->redbox->setMaximum(255);
	d->red->setMaximumHeight(d->redbox->height());
	layout->addWidget(new QLabel{tr("R"), w}, row, 0);
	layout->addWidget(d->red, row, 1);
	layout->addWidget(d->redbox, row, 2);

	++row;

	d->green = new color_widgets::GradientSlider(w);
	d->green->setMaximum(255);
	d->greenbox = new QSpinBox(w);
	d->greenbox->setMaximum(255);
	d->green->setMaximumHeight(d->greenbox->height());
	layout->addWidget(new QLabel{tr("G"), w}, row, 0);
	layout->addWidget(d->green, row, 1);
	layout->addWidget(d->greenbox, row, 2);

	++row;

	d->blue = new color_widgets::GradientSlider(w);
	d->blue->setMaximum(255);
	d->bluebox = new QSpinBox(w);
	d->bluebox->setMaximum(255);
	d->blue->setMaximumHeight(d->bluebox->height());
	layout->addWidget(new QLabel{tr("B"), w}, row, 0);
	layout->addWidget(d->blue, row, 1);
	layout->addWidget(d->bluebox, row, 2);

	++row;

	layout->setSpacing(3);
	layout->setRowStretch(row, 1);

	setWidget(w);

	connect(d->hue, QOverload<int>::of(&color_widgets::HueSlider::valueChanged), this, &ColorSliderDock::updateFromHsvSliders);
	connect(d->huebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromHsvSpinbox);

	connect(d->saturation, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorSliderDock::updateFromHsvSliders);
	connect(d->saturationbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromHsvSpinbox);

	connect(d->value, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorSliderDock::updateFromHsvSliders);
	connect(d->valuebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromHsvSpinbox);

	connect(d->red, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorSliderDock::updateFromRgbSliders);
	connect(d->redbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromRgbSpinbox);

	connect(d->green, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorSliderDock::updateFromRgbSliders);
	connect(d->greenbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromRgbSpinbox);

	connect(d->blue, QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this, &ColorSliderDock::updateFromRgbSliders);
	connect(d->bluebox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorSliderDock::updateFromRgbSpinbox);
}

ColorSliderDock::~ColorSliderDock()
{
	delete d;
}

void ColorSliderDock::setColor(const QColor& color)
{
	d->updating = true;

	d->lastUsedSwatch->setSelected(findPaletteColor(d->lastUsedSwatch->palette(), color));

	d->hue->setColorSaturation(color.saturationF());
	d->hue->setColorValue(color.valueF());
	d->hue->setValue(color.hue());
	d->huebox->setValue(color.hue());

	d->saturation->setFirstColor(QColor::fromHsv(color.hue(), 0, color.value()));
	d->saturation->setLastColor(QColor::fromHsv(color.hue(), 255, color.value()));
	d->saturation->setValue(color.saturation());
	d->saturationbox->setValue(color.saturation());

	d->value->setFirstColor(QColor::fromHsv(color.hue(), color.saturation(), 0));
	d->value->setLastColor(QColor::fromHsv(color.hue(), color.saturation(), 255));
	d->value->setValue(color.value());
	d->valuebox->setValue(color.value());

	d->red->setFirstColor(QColor(0, color.green(), color.blue()));
	d->red->setLastColor(QColor(255, color.green(), color.blue()));
	d->red->setValue(color.red());
	d->redbox->setValue(color.red());

	d->green->setFirstColor(QColor(color.red(), 0, color.blue()));
	d->green->setLastColor(QColor(color.red(), 255, color.blue()));
	d->green->setValue(color.green());
	d->greenbox->setValue(color.green());

	d->blue->setFirstColor(QColor(color.red(), color.green(), 0));
	d->blue->setLastColor(QColor(color.red(), color.green(), 255));
	d->blue->setValue(color.blue());
	d->bluebox->setValue(color.blue());

	d->updating = false;
}

void ColorSliderDock::updateFromRgbSliders()
{
	if(!d->updating) {
		const QColor color(d->red->value(), d->green->value(), d->blue->value());

		setColor(color);
		emit colorSelected(color);
	}
}

void ColorSliderDock::updateFromRgbSpinbox()
{
	if(!d->updating) {
		const QColor color(d->redbox->value(), d->greenbox->value(), d->bluebox->value());
		setColor(color);
		emit colorSelected(color);
	}
}

void ColorSliderDock::updateFromHsvSliders()
{
	if(!d->updating) {
		const QColor color = QColor::fromHsv(d->hue->value(), d->saturation->value(), d->value->value());

		setColor(color);
		emit colorSelected(color);
	}
}

void ColorSliderDock::updateFromHsvSpinbox()
{
	if(!d->updating) {
		const QColor color = QColor::fromHsv(d->huebox->value(), d->saturationbox->value(), d->valuebox->value());

		setColor(color);
		emit colorSelected(color);
	}
}

void ColorSliderDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	const QColor color(d->red->value(), d->green->value(), d->blue->value());
	d->lastUsedSwatch->setSelected(findPaletteColor(d->lastUsedSwatch->palette(), color));
}

}


// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/colorsliders.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include <QGridLayout>
#include <QLabel>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QtColorWidgets/color_utils.hpp>
#include <QtColorWidgets/hue_slider.hpp>
#include <QtColorWidgets/swatch.hpp>

namespace docks {

struct ColorSliderDock::Private {
	QColor lastColor = Qt::black;
	color_widgets::Swatch *lastUsedSwatch = nullptr;

	color_widgets::HueSlider *hue = nullptr;
	QLabel *saturationLabel = nullptr;
	color_widgets::GradientSlider *saturation = nullptr;
	QLabel *valueLabel = nullptr;
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

	color_widgets::ColorWheel::ColorSpaceEnum colorSpace =
		color_widgets::ColorWheel::ColorHSV;

	bool updating = false;
};

ColorSliderDock::ColorSliderDock(const QString &title, QWidget *parent)
	: DockBase(title, parent)
	, d(new Private)
{
	// Create title bar widget
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
	d->lastUsedSwatch->setForcedColumns(
		docks::ToolSettings::LASTUSED_COLOR_COUNT);
	d->lastUsedSwatch->setReadOnly(true);
	d->lastUsedSwatch->setBorder(Qt::NoPen);
	d->lastUsedSwatch->setMinimumHeight(24);

	titlebar->addSpace(24);
	titlebar->addCustomWidget(d->lastUsedSwatch, true);
	titlebar->addSpace(24);

	connect(
		d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this,
		&ColorSliderDock::colorSelected);

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

	d->saturationLabel = new QLabel{w};
	d->saturation = new color_widgets::GradientSlider(w);
	d->saturation->setMaximum(255);
	d->saturationbox = new QSpinBox(w);
	d->saturationbox->setMaximum(255);
	d->saturation->setMaximumHeight(d->saturationbox->height());
	layout->addWidget(d->saturationLabel, row, 0);
	layout->addWidget(d->saturation, row, 1);
	layout->addWidget(d->saturationbox, row, 2);

	++row;

	d->valueLabel = new QLabel{w};
	d->value = new color_widgets::GradientSlider(w);
	d->value->setMaximum(255);
	d->valuebox = new QSpinBox(w);
	d->valuebox->setMaximum(255);
	d->value->setMaximumHeight(d->valuebox->height());
	layout->addWidget(d->valueLabel, row, 0);
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

	connect(
		d->hue, QOverload<int>::of(&color_widgets::HueSlider::valueChanged),
		this, &ColorSliderDock::updateFromHsvSliders);
	connect(
		d->huebox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromHsvSpinbox);

	connect(
		d->saturation,
		QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this,
		&ColorSliderDock::updateFromHsvSliders);
	connect(
		d->saturationbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromHsvSpinbox);

	connect(
		d->value,
		QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this,
		&ColorSliderDock::updateFromHsvSliders);
	connect(
		d->valuebox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromHsvSpinbox);

	connect(
		d->red,
		QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this,
		&ColorSliderDock::updateFromRgbSliders);
	connect(
		d->redbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromRgbSpinbox);

	connect(
		d->green,
		QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this,
		&ColorSliderDock::updateFromRgbSliders);
	connect(
		d->greenbox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromRgbSpinbox);

	connect(
		d->blue,
		QOverload<int>::of(&color_widgets::GradientSlider::valueChanged), this,
		&ColorSliderDock::updateFromRgbSliders);
	connect(
		d->bluebox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&ColorSliderDock::updateFromRgbSpinbox);

	dpApp().settings().bindColorWheelSpace(
		this, &ColorSliderDock::setColorSpace);
}

ColorSliderDock::~ColorSliderDock()
{
	delete d;
}

void ColorSliderDock::setColor(const QColor &color)
{
	if(!d->updating) {
		updateColor(color, false, false);
	}
}

void ColorSliderDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	const QColor color(d->red->value(), d->green->value(), d->blue->value());
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), color));
}

void ColorSliderDock::updateFromRgbSliders()
{
	if(!d->updating) {
		const QColor color(
			d->red->value(), d->green->value(), d->blue->value());
		updateColor(color, false, true);
	}
}

void ColorSliderDock::updateFromRgbSpinbox()
{
	if(!d->updating) {
		const QColor color(
			d->redbox->value(), d->greenbox->value(), d->bluebox->value());
		updateColor(color, false, true);
	}
}

void ColorSliderDock::updateFromHsvSliders()
{
	if(!d->updating) {
		const QColor color = getColorSpaceColor(
			d->hue->value(), d->saturation->value(), d->value->value());
		d->huebox->setValue(d->hue->value());
		d->saturationbox->setValue(d->saturation->value());
		d->valuebox->setValue(d->value->value());
		updateColor(color, true, true);
	}
}

void ColorSliderDock::updateFromHsvSpinbox()
{
	if(!d->updating) {
		const QColor color = getColorSpaceColor(
			d->huebox->value(), d->saturationbox->value(),
			d->valuebox->value());
		d->hue->setValue(d->huebox->value());
		d->saturation->setValue(d->saturationbox->value());
		d->value->setValue(d->valuebox->value());
		updateColor(color, true, true);
	}
}

void ColorSliderDock::setColorSpace(
	color_widgets::ColorWheel::ColorSpaceEnum colorSpace)
{
	switch(colorSpace) {
	case color_widgets::ColorWheel::ColorHSL:
		d->colorSpace = color_widgets::ColorWheel::ColorHSL;
		d->saturationLabel->setText(tr("S"));
		d->valueLabel->setText(tr("L"));
		break;
	case color_widgets::ColorWheel::ColorLCH:
		d->colorSpace = color_widgets::ColorWheel::ColorLCH;
		d->saturationLabel->setText(tr("C"));
		d->valueLabel->setText(tr("L"));
		break;
	default:
		d->colorSpace = color_widgets::ColorWheel::ColorHSV;
		d->saturationLabel->setText(tr("S"));
		d->valueLabel->setText(tr("V"));
		break;
	}
	QScopedValueRollback<bool> guard{d->updating, true};
	updateSaturationSlider(d->lastColor, false);
	updateValueSlider(d->lastColor, false);
}

void ColorSliderDock::updateColor(
	const QColor &color, bool fromHsvSelection, bool selected)
{
	QScopedValueRollback<bool> guard{d->updating, true};

	d->lastColor = color;
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), color));

	updateHueSlider(color, fromHsvSelection);
	updateSaturationSlider(color, fromHsvSelection);
	updateValueSlider(color, fromHsvSelection);

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

	if(selected) {
		emit colorSelected(color);
	}
}

void ColorSliderDock::updateHueSlider(
	const QColor &color, bool fromHsvSelection)
{
	d->hue->setColorSaturation(color.saturationF());
	d->hue->setColorValue(color.valueF());
	if(!fromHsvSelection) {
		d->hue->setValue(color.hue());
		d->huebox->setValue(color.hue());
	}
}

void ColorSliderDock::updateSaturationSlider(
	const QColor &color, bool fromHsvSelection)
{
	switch(d->colorSpace) {
	case color_widgets::ColorWheel::ColorHSL: {
		qreal hf = color.hueF();
		qreal lf = color_widgets::utils::color_lightnessF(color);
		QVector<QColor> colors;
		colors.reserve(SLIDER_COLORS);
		for(int i = 0; i < SLIDER_COLORS; ++i) {
			qreal sf = i / qreal(SLIDER_COLORS - 1);
			colors.append(color_widgets::utils::color_from_hsl(hf, sf, lf));
		}
		d->saturation->setColors(colors);
		// Saturation is indeterminate at the lightness extremes.
		if(!fromHsvSelection && lf != 0.0 && lf != 1.0) {
			int s = qRound(
				color_widgets::utils::color_HSL_saturationF(color) * 255.0);
			d->saturation->setValue(s);
			d->saturationbox->setValue(s);
		}
		break;
	}
	case color_widgets::ColorWheel::ColorLCH: {
		qreal hf = color.hueF();
		qreal lf = color_widgets::utils::color_lumaF(color);
		QVector<QColor> colors;
		colors.reserve(SLIDER_COLORS);
		for(int i = 0; i < SLIDER_COLORS; ++i) {
			qreal cf = i / qreal(SLIDER_COLORS - 1);
			colors.append(color_widgets::utils::color_from_lch(hf, cf, lf));
		}
		d->saturation->setColors(colors);
		if(!fromHsvSelection) {
			int c = qRound(color_widgets::utils::color_chromaF(color) * 255.0);
			d->saturation->setValue(c);
			d->saturationbox->setValue(c);
		}
		break;
	}
	default: {
		int h = color.hue();
		int v = color.value();
		d->saturation->setColors(
			{QColor::fromHsv(h, 0, v), QColor::fromHsv(h, 255, v)});
		if(!fromHsvSelection) {
			int s = color.saturation();
			d->saturation->setValue(s);
			d->saturationbox->setValue(s);
		}
		break;
	}
	}
}

void ColorSliderDock::updateValueSlider(
	const QColor &color, bool fromHsvSelection)
{
	switch(d->colorSpace) {
	case color_widgets::ColorWheel::ColorHSL: {
		qreal hf = color.hueF();
		qreal sf = color_widgets::utils::color_HSL_saturationF(color);
		QVector<QColor> colors;
		colors.reserve(SLIDER_COLORS);
		for(int i = 0; i < SLIDER_COLORS; ++i) {
			qreal lf = i / qreal(SLIDER_COLORS - 1);
			colors.append(color_widgets::utils::color_from_hsl(hf, sf, lf));
		}
		d->value->setColors(colors);
		if(!fromHsvSelection) {
			int l =
				qRound(color_widgets::utils::color_lightnessF(color) * 255.0);
			d->value->setValue(l);
			d->valuebox->setValue(l);
		}
		break;
	}
	case color_widgets::ColorWheel::ColorLCH: {
		qreal hf = color.hueF();
		qreal cf = color_widgets::utils::color_chromaF(color);
		QVector<QColor> colors;
		colors.reserve(SLIDER_COLORS);
		for(int i = 0; i < SLIDER_COLORS; ++i) {
			qreal lf = i / qreal(SLIDER_COLORS - 1);
			colors.append(color_widgets::utils::color_from_lch(hf, cf, lf));
		}
		d->value->setColors(colors);
		if(!fromHsvSelection) {
			int l = qRound(color_widgets::utils::color_lumaF(color) * 255.0);
			d->value->setValue(l);
			d->valuebox->setValue(l);
		}
		break;
	}
	default: {
		int h = color.hue();
		int s = color.saturation();
		d->value->setColors(
			{QColor::fromHsv(h, s, 0), QColor::fromHsv(h, s, 255)});
		if(!fromHsvSelection) {
			int v = color.value();
			d->value->setValue(v);
			d->valuebox->setValue(v);
		}
		break;
	}
	}
}

QColor ColorSliderDock::getColorSpaceColor(int h, int s, int v) const
{
	switch(d->colorSpace) {
	case color_widgets::ColorWheel::ColorHSL:
		return fixHue(
			h, color_widgets::utils::color_from_hsl(
				   h / 359.0, s / 255.0, v / 255.0));
	case color_widgets::ColorWheel::ColorLCH:
		return fixHue(
			h, color_widgets::utils::color_from_lch(
				   h / 359.0, s / 255.0, v / 255.0));
	default:
		return QColor::fromHsv(h, s, v);
	}
}

// Hue may become indeterminate in HSL and LCH color spaces, but since we know
// the intended hue, we can fix it up.
QColor ColorSliderDock::fixHue(int h, const QColor &color)
{
	return QColor::fromHsv(h, color.saturation(), color.value());
}

}

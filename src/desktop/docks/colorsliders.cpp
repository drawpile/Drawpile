// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/colorsliders.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QtColorWidgets/color_line_edit.hpp>
#include <QtColorWidgets/color_names.hpp>
#include <QtColorWidgets/color_utils.hpp>
#include <QtColorWidgets/hue_slider.hpp>
#include <QtColorWidgets/swatch.hpp>

namespace docks {

struct ColorSliderDock::Private {
	QColor lastColor = Qt::black;
	color_widgets::Swatch *lastUsedSwatch = nullptr;

	QStackedWidget *stack = nullptr;

	QLabel *hueLabel = nullptr;
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

	widgets::GroupedToolButton *hsvButton = nullptr;
	QButtonGroup *group = nullptr;
	color_widgets::ColorLineEdit *lineEdit = nullptr;

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
	QVBoxLayout *layout = new QVBoxLayout(w);

	d->stack = new QStackedWidget;
	d->stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(d->stack);

	QWidget *hsvWidget = new QWidget;
	QGridLayout *hsvGrid = new QGridLayout(hsvWidget);
	hsvGrid->setContentsMargins(0, 0, 0, 0);
	hsvGrid->setSpacing(3);
	d->stack->addWidget(hsvWidget);

	d->hueLabel = new QLabel;
	d->hue = new color_widgets::HueSlider;
	d->hue->setMaximum(359);
	d->huebox = new QSpinBox;
	d->huebox->setMaximum(359);
	d->hue->setMaximumHeight(d->huebox->height());
	hsvGrid->addWidget(d->hueLabel, 0, 0);
	hsvGrid->addWidget(d->hue, 0, 1);
	hsvGrid->addWidget(d->huebox, 0, 2);

	d->saturationLabel = new QLabel;
	d->saturation = new color_widgets::GradientSlider;
	d->saturation->setMaximum(255);
	d->saturationbox = new QSpinBox;
	d->saturationbox->setMaximum(255);
	d->saturation->setMaximumHeight(d->saturationbox->height());
	hsvGrid->addWidget(d->saturationLabel, 1, 0);
	hsvGrid->addWidget(d->saturation, 1, 1);
	hsvGrid->addWidget(d->saturationbox, 1, 2);

	d->valueLabel = new QLabel;
	d->value = new color_widgets::GradientSlider;
	d->value->setMaximum(255);
	d->valuebox = new QSpinBox;
	d->valuebox->setMaximum(255);
	d->value->setMaximumHeight(d->valuebox->height());
	hsvGrid->addWidget(d->valueLabel, 2, 0);
	hsvGrid->addWidget(d->value, 2, 1);
	hsvGrid->addWidget(d->valuebox, 2, 2);

	QWidget *rgbWidget = new QWidget;
	QGridLayout *rgbGrid = new QGridLayout(rgbWidget);
	rgbGrid->setContentsMargins(0, 0, 0, 0);
	rgbGrid->setSpacing(3);
	d->stack->addWidget(rgbWidget);

	d->red = new color_widgets::GradientSlider;
	d->red->setMaximum(255);
	d->redbox = new QSpinBox;
	d->redbox->setMaximum(255);
	d->red->setMaximumHeight(d->redbox->height());
	//: The "Red" R of RGB.
	rgbGrid->addWidget(new QLabel(tr("R")), 0, 0);
	rgbGrid->addWidget(d->red, 0, 1);
	rgbGrid->addWidget(d->redbox, 0, 2);

	d->green = new color_widgets::GradientSlider;
	d->green->setMaximum(255);
	d->greenbox = new QSpinBox;
	d->greenbox->setMaximum(255);
	d->green->setMaximumHeight(d->greenbox->height());
	//: The "Green" G of RGB.
	rgbGrid->addWidget(new QLabel(tr("G")), 1, 0);
	rgbGrid->addWidget(d->green, 1, 1);
	rgbGrid->addWidget(d->greenbox, 1, 2);

	d->blue = new color_widgets::GradientSlider;
	d->blue->setMaximum(255);
	d->bluebox = new QSpinBox;
	d->bluebox->setMaximum(255);
	d->blue->setMaximumHeight(d->bluebox->height());
	//: The "Blue" B of RGB.
	rgbGrid->addWidget(new QLabel(tr("B")), 2, 0);
	rgbGrid->addWidget(d->blue, 2, 1);
	rgbGrid->addWidget(d->bluebox, 2, 2);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	buttonsLayout->setSpacing(0);
	layout->addLayout(buttonsLayout);

	d->hsvButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	d->hsvButton->setCheckable(true);
	d->hsvButton->setChecked(true);
	buttonsLayout->addWidget(d->hsvButton);

	widgets::GroupedToolButton *rgbButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	rgbButton->setCheckable(true);
	rgbButton->setText(tr("RGB"));
	buttonsLayout->addWidget(rgbButton);

	d->group = new QButtonGroup(w);
	d->group->addButton(d->hsvButton, 0);
	d->group->addButton(rgbButton, 1);

	buttonsLayout->addSpacing(3);

	d->lineEdit = new color_widgets::ColorLineEdit;
	buttonsLayout->addWidget(d->lineEdit);

	layout->addStretch(1);
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

	connect(
		d->group, &QButtonGroup::idClicked, this, &ColorSliderDock::setMode);

	connect(
		d->lineEdit, &color_widgets::ColorLineEdit::colorEdited, this,
		&ColorSliderDock::updateFromLineEdit);
	connect(
		d->lineEdit, &color_widgets::ColorLineEdit::colorEditingFinished, this,
		&ColorSliderDock::updateFromLineEditFinished);

	dpApp().settings().bindColorWheelSpace(
		this, &ColorSliderDock::setColorSpace);
	dpApp().settings().bindColorSlidersMode(this, &ColorSliderDock::setMode);
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

void ColorSliderDock::updateFromLineEdit(const QColor &color)
{
	if(!d->updating) {
		updateColor(color, false, true);
	}
}

void ColorSliderDock::updateFromLineEditFinished(const QColor &color)
{
	if(!d->updating) {
		updateColor(color, false, true);
		// Force the line edit to be in #RRGGBB format, since it may not be.
		d->lineEdit->setText(
			color_widgets::stringFromColor(color, d->lineEdit->showAlpha()));
	}
}

void ColorSliderDock::setColorSpace(
	color_widgets::ColorWheel::ColorSpaceEnum colorSpace)
{
	switch(colorSpace) {
	case color_widgets::ColorWheel::ColorHSL:
		d->colorSpace = color_widgets::ColorWheel::ColorHSL;
		//: The "Hue" H of HSL.
		d->hueLabel->setText(tr("H", "HSL"));
		//: The "Saturation" S of HSL.
		d->saturationLabel->setText(tr("S", "HSL"));
		//: The "Lightness" L HSL.
		d->valueLabel->setText(tr("L", "HSL"));
		//: Color space HSL (hue, saturation, lightness)
		d->hsvButton->setText(tr("HSL"));
		break;
	case color_widgets::ColorWheel::ColorLCH:
		d->colorSpace = color_widgets::ColorWheel::ColorLCH;
		//: The "Hue" H of HCL.
		d->hueLabel->setText(tr("H", "HCL"));
		//: The "Chroma" C of HCL.
		d->saturationLabel->setText(tr("C", "HCL"));
		//: The "Luma" L of HCL.
		d->valueLabel->setText(tr("L", "HCL"));
		//: Color space HCL (hue, chroma, lightness)
		d->hsvButton->setText(tr("HCL"));
		break;
	default:
		d->colorSpace = color_widgets::ColorWheel::ColorHSV;
		//: The "Hue" H of HSV.
		d->hueLabel->setText(tr("H", "HSV"));
		//: The "Saturation" S of HSV.
		d->saturationLabel->setText(tr("S", "HSV"));
		//: The "Value" V of HSV.
		d->valueLabel->setText(tr("V", "HSV"));
		//: Color space HSV (hue, chroma, value)
		d->hsvButton->setText(tr("HSV"));
		break;
	}
	QScopedValueRollback<bool> guard{d->updating, true};
	updateSaturationSlider(d->lastColor, false);
	updateValueSlider(d->lastColor, false);
}

void ColorSliderDock::setMode(int mode)
{
	if(mode == 0 || mode == 1) {
		QSignalBlocker blocker(d->group);
		d->group->button(mode)->setChecked(true);
		d->stack->setCurrentIndex(mode);
		dpApp().settings().setColorSlidersMode(mode);
	} else {
		qWarning("Unknown color slider mode %d", mode);
	}
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

	d->lineEdit->setColor(color);

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

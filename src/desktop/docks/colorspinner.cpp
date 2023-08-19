// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/docks/colorspinner.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"

#include <QtColorWidgets/swatch.hpp>
#include <QtColorWidgets/color_wheel.hpp>

namespace docks {

struct ColorSpinnerDock::Private {
	color_widgets::Swatch *lastUsedSwatch = nullptr;
	color_widgets::ColorWheel *colorwheel = nullptr;
};

ColorSpinnerDock::ColorSpinnerDock(const QString& title, QWidget *parent)
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

	connect(d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this, &ColorSpinnerDock::colorSelected);

	// Create main widget
	d->colorwheel = new color_widgets::ColorWheel(this);
	setWidget(d->colorwheel);

	connect(d->colorwheel, &color_widgets::ColorWheel::colorSelected, this, &ColorSpinnerDock::colorSelected);

	auto &settings = dpApp().settings();
	settings.bindColorWheelShape(d->colorwheel, &color_widgets::ColorWheel::setSelectorShape, &color_widgets::ColorWheel::selectorShapeChanged);
	settings.bindColorWheelAngleAs<bool>(d->colorwheel, &color_widgets::ColorWheel::setRotatingSelector, &color_widgets::ColorWheel::rotatingSelectorChanged);
	settings.bindColorWheelSpace(d->colorwheel, &color_widgets::ColorWheel::setColorSpace, &color_widgets::ColorWheel::colorSpaceChanged);
	settings.bindColorWheelMirror(d->colorwheel, &color_widgets::ColorWheel::setMirroredSelector, &color_widgets::ColorWheel::mirroredSelectorChanged);
}

ColorSpinnerDock::~ColorSpinnerDock()
{
	delete d;
}

void ColorSpinnerDock::setColor(const QColor& color)
{
	d->lastUsedSwatch->setSelected(findPaletteColor(d->lastUsedSwatch->palette(), color));

	if(d->colorwheel->color() != color)
		d->colorwheel->setColor(color);
}

void ColorSpinnerDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	d->lastUsedSwatch->setSelected(findPaletteColor(d->lastUsedSwatch->palette(), d->colorwheel->color()));
}

}


/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2022 Calle Laakkonen

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

#include "desktop/main.h"
#include "desktop/docks/colorspinner.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"

#include <QtColorWidgets/swatch.hpp>
#include <QtColorWidgets/color_wheel.hpp>
#include <QSettings>

namespace docks {

struct ColorSpinnerDock::Private {
	color_widgets::Swatch *lastUsedSwatch = nullptr;
	color_widgets::ColorWheel *colorwheel = nullptr;
};

ColorSpinnerDock::ColorSpinnerDock(const QString& title, QWidget *parent)
	: QDockWidget(title, parent), d(new Private)
{
	// Create title bar widget
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
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

	// Restore UI state
	connect(static_cast<DrawpileApp*>(qApp), &DrawpileApp::settingsChanged,
			this, &ColorSpinnerDock::updateSettings);
	updateSettings();
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

void ColorSpinnerDock::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/colorwheel");
	d->colorwheel->setSelectorShape(
			static_cast<color_widgets::ColorWheel::ShapeEnum>(cfg.value("shape").toInt()));
	d->colorwheel->setRotatingSelector(
			static_cast<color_widgets::ColorWheel::AngleEnum>(cfg.value("rotate").toInt()));
	d->colorwheel->setColorSpace(
			static_cast<color_widgets::ColorWheel::ColorSpaceEnum>(cfg.value("space").toInt()));
}

}


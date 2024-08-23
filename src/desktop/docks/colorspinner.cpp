// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/colorspinner.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QScopedValueRollback>
#include <QtColorWidgets/swatch.hpp>

namespace docks {

struct ColorSpinnerDock::Private {
	widgets::GroupedToolButton *menuButton = nullptr;
	QAction *shapeTriangleAction = nullptr;
	QAction *shapeSquareAction = nullptr;
	QAction *angleFixedAction = nullptr;
	QAction *angleRotatingAction = nullptr;
	QAction *colorSpaceHsvAction = nullptr;
	QAction *colorSpaceHslAction = nullptr;
	QAction *colorSpaceHclAction = nullptr;
	QAction *directionAscendingAction = nullptr;
	QAction *directionDescendingAction = nullptr;
	color_widgets::Swatch *lastUsedSwatch = nullptr;
	color_widgets::ColorWheel *colorwheel = nullptr;
	bool updating = false;
};

ColorSpinnerDock::ColorSpinnerDock(const QString &title, QWidget *parent)
	: DockBase(title, parent)
	, d(new Private)
{
	// Create title bar widget
	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	QMenu *menu = new QMenu(this);

	QMenu *shapeMenu = menu->addMenu(tr("Shape"));
	QActionGroup *shapeGroup = new QActionGroup(this);

	d->shapeTriangleAction = shapeMenu->addAction(tr("Triangle"));
	d->shapeTriangleAction->setCheckable(true);
	shapeGroup->addAction(d->shapeTriangleAction);
	connect(
		d->shapeTriangleAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelShape(
					color_widgets::ColorWheel::ShapeTriangle);
			}
		});

	d->shapeSquareAction = shapeMenu->addAction(tr("Square"));
	d->shapeSquareAction->setCheckable(true);
	shapeGroup->addAction(d->shapeSquareAction);
	connect(
		d->shapeSquareAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelShape(
					color_widgets::ColorWheel::ShapeSquare);
			}
		});

	QMenu *angleMenu = menu->addMenu(tr("Angle"));
	QActionGroup *angleGroup = new QActionGroup(this);

	d->angleFixedAction = angleMenu->addAction(tr("Fixed"));
	d->angleFixedAction->setCheckable(true);
	angleGroup->addAction(d->angleFixedAction);
	connect(d->angleFixedAction, &QAction::toggled, this, [this](bool toggled) {
		if(toggled && !d->updating) {
			dpApp().settings().setColorWheelAngle(
				color_widgets::ColorWheel::AngleFixed);
		}
	});

	d->angleRotatingAction = angleMenu->addAction(tr("Rotating"));
	d->angleRotatingAction->setCheckable(true);
	angleGroup->addAction(d->angleRotatingAction);
	connect(
		d->angleRotatingAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelAngle(
					color_widgets::ColorWheel::AngleRotating);
			}
		});

	QMenu *colorSpaceMenu = menu->addMenu(tr("Color space"));
	QActionGroup *colorSpaceGroup = new QActionGroup(this);

	d->colorSpaceHsvAction = colorSpaceMenu->addAction(tr("HSV"));
	d->colorSpaceHsvAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHsvAction);
	connect(
		d->colorSpaceHsvAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorHSV);
			}
		});

	d->colorSpaceHslAction = colorSpaceMenu->addAction(tr("HSL"));
	d->colorSpaceHslAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHslAction);
	connect(
		d->colorSpaceHslAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorHSL);
			}
		});

	d->colorSpaceHclAction = colorSpaceMenu->addAction(tr("HCL"));
	d->colorSpaceHclAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHclAction);
	connect(
		d->colorSpaceHclAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorLCH);
			}
		});

	QMenu *directionMenu = menu->addMenu(tr("Direction"));
	QActionGroup *directionGroup = new QActionGroup(this);

	d->directionAscendingAction = directionMenu->addAction(tr("Ascending"));
	d->directionAscendingAction->setCheckable(true);
	directionGroup->addAction(d->directionAscendingAction);
	connect(
		d->directionAscendingAction, &QAction::toggled, this,
		[this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelMirror(true);
			}
		});

	d->directionDescendingAction = directionMenu->addAction(tr("Descending"));
	d->directionDescendingAction->setCheckable(true);
	directionGroup->addAction(d->directionDescendingAction);
	connect(
		d->directionDescendingAction, &QAction::toggled, this,
		[this](bool toggled) {
			if(toggled && !d->updating) {
				dpApp().settings().setColorWheelMirror(false);
			}
		});

	d->menuButton = new widgets::GroupedToolButton(this);
	d->menuButton->setIcon(QIcon::fromTheme("application-menu"));
	d->menuButton->setPopupMode(QToolButton::InstantPopup);
	d->menuButton->setMenu(menu);

	d->lastUsedSwatch = new color_widgets::Swatch(titlebar);
	d->lastUsedSwatch->setForcedRows(1);
	d->lastUsedSwatch->setForcedColumns(
		docks::ToolSettings::LASTUSED_COLOR_COUNT);
	d->lastUsedSwatch->setReadOnly(true);
	d->lastUsedSwatch->setBorder(Qt::NoPen);
	d->lastUsedSwatch->setMinimumHeight(24);

	titlebar->addCustomWidget(d->menuButton);
	titlebar->addSpace(4);
	titlebar->addCustomWidget(d->lastUsedSwatch, true);
	titlebar->addSpace(4);

	connect(
		d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this,
		&ColorSpinnerDock::colorSelected);

	// Create main widget
	d->colorwheel = new color_widgets::ColorWheel(this);
	d->colorwheel->setMinimumSize(64, 64);
	setWidget(d->colorwheel);

	connect(
		d->colorwheel, &color_widgets::ColorWheel::colorSelected, this,
		&ColorSpinnerDock::colorSelected);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindColorWheelShape(this, &ColorSpinnerDock::setShape);
	settings.bindColorWheelAngle(this, &ColorSpinnerDock::setAngle);
	settings.bindColorWheelSpace(this, &ColorSpinnerDock::setColorSpace);
	settings.bindColorWheelMirror(this, &ColorSpinnerDock::setMirror);
}

ColorSpinnerDock::~ColorSpinnerDock()
{
	delete d;
}

void ColorSpinnerDock::setColor(const QColor &color)
{
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), color));

	if(d->colorwheel->color() != color)
		d->colorwheel->setColor(color);
}

void ColorSpinnerDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), d->colorwheel->color()));
}

void ColorSpinnerDock::setShape(color_widgets::ColorWheel::ShapeEnum shape)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	switch(shape) {
	case color_widgets::ColorWheel::ShapeSquare:
		d->shapeSquareAction->setChecked(true);
		d->colorwheel->setSelectorShape(color_widgets::ColorWheel::ShapeSquare);
		break;
	default:
		d->shapeTriangleAction->setChecked(true);
		d->colorwheel->setSelectorShape(
			color_widgets::ColorWheel::ShapeTriangle);
		break;
	}
}

void ColorSpinnerDock::setAngle(color_widgets::ColorWheel::AngleEnum angle)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	switch(angle) {
	case color_widgets::ColorWheel::AngleFixed:
		d->angleFixedAction->setChecked(true);
		d->colorwheel->setRotatingSelector(false);
		break;
	default:
		d->angleRotatingAction->setChecked(true);
		d->colorwheel->setRotatingSelector(true);
		break;
	}
}

void ColorSpinnerDock::setColorSpace(
	color_widgets::ColorWheel::ColorSpaceEnum colorSpace)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	switch(colorSpace) {
	case color_widgets::ColorWheel::ColorHSL:
		d->colorSpaceHslAction->setChecked(true);
		d->colorwheel->setColorSpace(color_widgets::ColorWheel::ColorHSL);
		break;
	case color_widgets::ColorWheel::ColorLCH:
		d->colorSpaceHclAction->setChecked(true);
		d->colorwheel->setColorSpace(color_widgets::ColorWheel::ColorLCH);
		break;
	default:
		d->colorSpaceHsvAction->setChecked(true);
		d->colorwheel->setColorSpace(color_widgets::ColorWheel::ColorHSV);
		break;
	}
}

void ColorSpinnerDock::setMirror(bool mirror)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	if(mirror) {
		d->directionAscendingAction->setChecked(true);
		d->colorwheel->setMirroredSelector(true);
	} else {
		d->directionDescendingAction->setChecked(true);
		d->colorwheel->setMirroredSelector(false);
	}
}

}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/colorspinner.h"
#include "desktop/dialogs/shadeselectordialog.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/enums.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/shadeselector.h"
#include "libclient/config/config.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QVBoxLayout>
#include <QtColorWidgets/color_wheel.hpp>
#include <QtColorWidgets/swatch.hpp>
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
#	include "desktop/widgets/colorpopup.h"
#endif

namespace docks {

#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
namespace {

class PopupColorWheel : public color_widgets::ColorWheel {
public:
	PopupColorWheel(ColorSpinnerDock *dock)
		: color_widgets::ColorWheel()
		, m_dock(dock)
	{
	}

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		if(event->buttons() & Qt::LeftButton) {
			m_dock->showPreviewPopup();
		}
		color_widgets::ColorWheel::mousePressEvent(event);
	}

private:
	ColorSpinnerDock *m_dock;
};

}
#else
using PopupColorWheel = color_widgets::ColorWheel;
#endif

struct ColorSpinnerDock::Private {
	widgets::GroupedToolButton *menuButton = nullptr;
	QAction *shapeRotatingTriangleAction = nullptr;
	QAction *shapeRotatingSquareAction = nullptr;
	QAction *shapeFixedTriangleAction = nullptr;
	QAction *shapeFixedSquareAction = nullptr;
	QAction *angleFixedAction = nullptr;
	QAction *angleRotatingAction = nullptr;
	QAction *colorSpaceHsvAction = nullptr;
	QAction *colorSpaceHslAction = nullptr;
	QAction *colorSpaceHclAction = nullptr;
	QAction *directionAscendingAction = nullptr;
	QAction *directionDescendingAction = nullptr;
	QAction *alignTopAction = nullptr;
	QAction *alignCenterAction = nullptr;
	QAction *previewAction = nullptr;
	QAction *showColorShadesAction = nullptr;
	QAction *configureColorShadesAction = nullptr;
	color_widgets::Swatch *lastUsedSwatch = nullptr;
	QColor lastUsedColor;
	PopupColorWheel *colorwheel = nullptr;
	widgets::ShadeSelector *shadeSelector = nullptr;
	QMenu *shadeSelectorMenu = nullptr;
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	widgets::ColorPopup *popup = nullptr;
	bool popupEnabled = false;
#endif
	bool updating = false;
	bool settingColorFromShades = false;
};

ColorSpinnerDock::ColorSpinnerDock(QWidget *parent)
	: DockBase(
		  tr("Color Wheel"), tr("Wheel"),
		  QIcon::fromTheme("drawpile_colorwheel"), parent)
	, d(new Private)
{
	// Create title bar widget
	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	config::Config *cfg = dpAppConfig();
	QMenu *menu = new QMenu(this);

	QMenu *shapeMenu = menu->addMenu(tr("Shape"));
	QActionGroup *shapeGroup = new QActionGroup(this);

	d->shapeRotatingTriangleAction =
		shapeMenu->addAction(tr("Rotating triangle"));
	d->shapeRotatingTriangleAction->setCheckable(true);
	shapeGroup->addAction(d->shapeRotatingTriangleAction);
	connect(
		d->shapeRotatingTriangleAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelShape(
					int(color_widgets::ColorWheel::ShapeTriangle));
				cfg->setColorWheelAngle(
					int(color_widgets::ColorWheel::AngleRotating));
			}
		});

	d->shapeRotatingSquareAction = shapeMenu->addAction(tr("Rotating square"));
	d->shapeRotatingSquareAction->setCheckable(true);
	shapeGroup->addAction(d->shapeRotatingSquareAction);
	connect(
		d->shapeRotatingSquareAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelShape(
					int(color_widgets::ColorWheel::ShapeSquare));
				cfg->setColorWheelAngle(
					int(color_widgets::ColorWheel::AngleRotating));
			}
		});

	d->shapeFixedTriangleAction = shapeMenu->addAction(tr("Fixed triangle"));
	d->shapeFixedTriangleAction->setCheckable(true);
	shapeGroup->addAction(d->shapeFixedTriangleAction);
	connect(
		d->shapeFixedTriangleAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelShape(
					int(color_widgets::ColorWheel::ShapeTriangle));
				cfg->setColorWheelAngle(
					int(color_widgets::ColorWheel::AngleFixed));
			}
		});

	d->shapeFixedSquareAction = shapeMenu->addAction(tr("Fixed square"));
	d->shapeFixedSquareAction->setCheckable(true);
	shapeGroup->addAction(d->shapeFixedSquareAction);
	connect(
		d->shapeFixedSquareAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelShape(
					int(color_widgets::ColorWheel::ShapeSquare));
				cfg->setColorWheelAngle(
					int(color_widgets::ColorWheel::AngleFixed));
			}
		});

	QMenu *colorSpaceMenu = menu->addMenu(tr("Color space"));
	QActionGroup *colorSpaceGroup = new QActionGroup(this);

	d->colorSpaceHsvAction = colorSpaceMenu->addAction(tr("HSV"));
	d->colorSpaceHsvAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHsvAction);
	connect(
		d->colorSpaceHsvAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelSpace(
					int(color_widgets::ColorWheel::ColorHSV));
			}
		});

	d->colorSpaceHslAction = colorSpaceMenu->addAction(tr("HSL"));
	d->colorSpaceHslAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHslAction);
	connect(
		d->colorSpaceHslAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelSpace(
					int(color_widgets::ColorWheel::ColorHSL));
			}
		});

	d->colorSpaceHclAction = colorSpaceMenu->addAction(tr("HCL"));
	d->colorSpaceHclAction->setCheckable(true);
	colorSpaceGroup->addAction(d->colorSpaceHclAction);
	connect(
		d->colorSpaceHclAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelSpace(
					int(color_widgets::ColorWheel::ColorLCH));
			}
		});

	QMenu *directionMenu = menu->addMenu(tr("Direction"));
	QActionGroup *directionGroup = new QActionGroup(this);

	d->directionAscendingAction = directionMenu->addAction(tr("Ascending"));
	d->directionAscendingAction->setCheckable(true);
	directionGroup->addAction(d->directionAscendingAction);
	connect(
		d->directionAscendingAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelMirror(true);
			}
		});

	d->directionDescendingAction = directionMenu->addAction(tr("Descending"));
	d->directionDescendingAction->setCheckable(true);
	directionGroup->addAction(d->directionDescendingAction);
	connect(
		d->directionDescendingAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelMirror(false);
			}
		});

	QMenu *alignMenu = menu->addMenu(tr("Alignment"));
	QActionGroup *alignGroup = new QActionGroup(this);

	d->alignTopAction = alignMenu->addAction(tr("Top"));
	d->alignTopAction->setCheckable(true);
	alignGroup->addAction(d->alignTopAction);
	connect(
		d->alignTopAction, &QAction::toggled, this, [this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelAlign(int(Qt::AlignTop));
			}
		});

	d->alignCenterAction = alignMenu->addAction(tr("Center"));
	d->alignCenterAction->setCheckable(true);
	alignGroup->addAction(d->alignCenterAction);
	connect(
		d->alignCenterAction, &QAction::toggled, this,
		[this, cfg](bool toggled) {
			if(toggled && !d->updating) {
				cfg->setColorWheelAlign(int(Qt::AlignCenter));
			}
		});

#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	d->previewAction = menu->addAction(tr("Preview selected color"));
	d->previewAction->setCheckable(true);
	connect(
		d->previewAction, &QAction::toggled, this, [this, cfg](bool toggled) {
			if(!d->updating) {
				cfg->setColorWheelPreview(toggled ? 1 : 0);
			}
		});
#endif

	menu->addSeparator();

	d->showColorShadesAction = menu->addAction(tr("Show color harmonies"));
	d->showColorShadesAction->setStatusTip(
		tr("Toggle the harmony swatches below the color wheel"));
	d->showColorShadesAction->setCheckable(true);

	d->configureColorShadesAction =
		menu->addAction(tr("Configure color harmonies…"));
	d->configureColorShadesAction->setStatusTip(
		tr("Change the color harmonies and how they are displayed"));
	connect(
		d->configureColorShadesAction, &QAction::triggered, this,
		&ColorSpinnerDock::showColorShadesDialog);

	menu->addSeparator();
	ColorPaletteDock::addSwatchOptionsToMenu(menu, COLOR_SWATCH_NO_SPINNER);

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
	utils::setWidgetRetainSizeWhenHidden(d->lastUsedSwatch, true);

	titlebar->addCustomWidget(d->menuButton);
	titlebar->addSpace(4);
	titlebar->addCustomWidget(d->lastUsedSwatch, 1);
	titlebar->addSpace(4);

	connect(
		d->lastUsedSwatch, &color_widgets::Swatch::colorSelected, this,
		&ColorSpinnerDock::colorSelected);

	QWidget *widget = new QWidget(this);
	widget->setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(0);

	d->colorwheel = new PopupColorWheel(this);
	d->colorwheel->setMinimumSize(64, 64);
	d->colorwheel->setContextMenuPolicy(Qt::CustomContextMenu);
	utils::setWidgetLongPressEnabled(d->colorwheel, false);
	layout->addWidget(d->colorwheel, 1);

	setWidget(widget);

	connect(
		d->colorwheel, &color_widgets::ColorWheel::colorSelected, this,
		&ColorSpinnerDock::colorSelected);
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	connect(
		d->colorwheel, &color_widgets::ColorWheel::editingFinished, this,
		&ColorSpinnerDock::hidePreviewPopup);
#endif
	connect(
		d->colorwheel, &color_widgets::ColorWheel::customContextMenuRequested,
		this, &ColorSpinnerDock::showContextMenu);

	CFG_BIND_SET(cfg, ColorWheelShape, this, ColorSpinnerDock::setShape);
	CFG_BIND_SET(cfg, ColorWheelAngle, this, ColorSpinnerDock::setAngle);
	CFG_BIND_SET(cfg, ColorWheelSpace, this, ColorSpinnerDock::setColorSpace);
	CFG_BIND_SET(cfg, ColorWheelMirror, this, ColorSpinnerDock::setMirror);
	CFG_BIND_SET(cfg, ColorWheelAlign, this, ColorSpinnerDock::setAlign);
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	CFG_BIND_SET(cfg, ColorWheelPreview, this, ColorSpinnerDock::setPreview);
#endif
	CFG_BIND_SET(cfg, ColorSwatchFlags, this, ColorSpinnerDock::setSwatchFlags);
	CFG_BIND_ACTION(cfg, ColorShadesEnabled, d->showColorShadesAction);
	CFG_BIND_SET(
		cfg, ColorShadesEnabled, this, ColorSpinnerDock::setShadesEnabled);
}

ColorSpinnerDock::~ColorSpinnerDock()
{
	delete d;
}

#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
void ColorSpinnerDock::showPreviewPopup()
{
	if(d->popupEnabled) {
		if(!d->popup) {
			d->popup = new widgets::ColorPopup(this);
			d->popup->setSelectedColor(d->colorwheel->color());
		}
		d->popup->setPreviousColor(d->colorwheel->color());
		d->popup->setLastUsedColor(d->lastUsedColor);
		d->popup->showPopup(
			this, isFloating() ? nullptr : parentWidget(),
			mapFromGlobal(d->colorwheel->mapToGlobal(QPoint(0, 0))).y(),
			!d->colorwheel->alignTop());
	}
}

void ColorSpinnerDock::hidePreviewPopup()
{
	if(d->popup) {
		d->popup->hide();
	}
}
#endif

void ColorSpinnerDock::setColor(const QColor &color)
{
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), color));

	if(d->shadeSelector && !d->settingColorFromShades &&
	   d->colorwheel->color().rgb() != color.rgb()) {
		d->shadeSelector->setColor(color);
	}

	if(d->colorwheel->color() != color) {
		d->colorwheel->setColor(color);
		d->lastUsedColor = color;
	} else if(!d->lastUsedColor.isValid()) {
		d->lastUsedColor = color;
	}
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	if(d->popup) {
		d->popup->setSelectedColor(color);
	}
#endif
}

void ColorSpinnerDock::setColorFromShades(const QColor &color)
{
	if(!d->settingColorFromShades) {
		QScopedValueRollback<bool> rollback(d->settingColorFromShades, true);
		setColor(color);
		emit colorSelected(color);
	}
}

void ColorSpinnerDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	d->lastUsedSwatch->setPalette(pal);
	d->lastUsedSwatch->setSelected(
		findPaletteColor(d->lastUsedSwatch->palette(), d->colorwheel->color()));
	d->lastUsedColor =
		pal.count() == 0 ? d->colorwheel->color() : pal.colorAt(0);
}

void ColorSpinnerDock::setShape(int shape)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	d->colorwheel->setSelectorShape(
		color_widgets::ColorWheel::ShapeEnum(shape));
	updateShapeAction();
}

void ColorSpinnerDock::setAngle(int angle)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	d->colorwheel->setRotatingSelector(
		angle != int(color_widgets::ColorWheel::AngleFixed));
	updateShapeAction();
}

void ColorSpinnerDock::setColorSpace(int colorSpace)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	switch(colorSpace) {
	case int(color_widgets::ColorWheel::ColorHSL):
		d->colorSpaceHslAction->setChecked(true);
		d->colorwheel->setColorSpace(color_widgets::ColorWheel::ColorHSL);
		break;
	case int(color_widgets::ColorWheel::ColorLCH):
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

void ColorSpinnerDock::setAlign(int align)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	if(align & int(Qt::AlignTop)) {
		d->alignTopAction->setChecked(true);
		d->colorwheel->setAlignTop(true);
	} else {
		d->alignCenterAction->setChecked(true);
		d->colorwheel->setAlignTop(false);
	}
}

#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
void ColorSpinnerDock::setPreview(int preview)
{
	QScopedValueRollback<bool> guard(d->updating, true);
	bool enabled = preview != 0;
	d->previewAction->setChecked(enabled);
	d->popupEnabled = enabled;
	if(!enabled) {
		hidePreviewPopup();
	}
}
#endif

void ColorSpinnerDock::setShadesEnabled(bool shadesEnabled)
{
	if(shadesEnabled && !d->shadeSelector) {
		d->shadeSelector = new widgets::ShadeSelector;
		d->shadeSelector->setColor(d->colorwheel->color());
		d->shadeSelector->setContextMenuPolicy(Qt::CustomContextMenu);

		config::Config *cfg = dpAppConfig();
		CFG_BIND_SET(
			cfg, ColorShadesConfig, d->shadeSelector,
			widgets::ShadeSelector::setConfig);
		CFG_BIND_SET(
			cfg, ColorShadesRowHeight, d->shadeSelector,
			widgets::ShadeSelector::setRowHeight);
		CFG_BIND_SET(
			cfg, ColorShadesColumnCount, d->shadeSelector,
			widgets::ShadeSelector::setColumnCount);
		CFG_BIND_SET(
			cfg, ColorShadesBorderThickness, d->shadeSelector,
			widgets::ShadeSelector::setBorderThickness);

		widget()->layout()->addWidget(d->shadeSelector);

		connect(
			d->colorwheel, &color_widgets::ColorWheel::colorSelected,
			d->shadeSelector, &widgets::ShadeSelector::setColor);
		connect(
			d->shadeSelector, &widgets::ShadeSelector::colorSelected, this,
			&ColorSpinnerDock::setColorFromShades);
		connect(
			d->shadeSelector, &widgets::ShadeSelector::colorDoubleClicked,
			d->shadeSelector, &widgets::ShadeSelector::setColor);
		connect(
			d->shadeSelector, &widgets::ShadeSelector::colorDoubleClicked, this,
			&ColorSpinnerDock::setColorFromShades);
		connect(
			d->shadeSelector,
			&widgets::ShadeSelector::customContextMenuRequested, this,
			&ColorSpinnerDock::showShadesContextMenu);
	} else if(!shadesEnabled && d->shadeSelector) {
		d->shadeSelector->deleteLater();
		d->shadeSelector = nullptr;
	}
}

void ColorSpinnerDock::updateShapeAction()
{
	QAction *action;
	bool rotating = d->colorwheel->rotatingSelector();
	switch(d->colorwheel->selectorShape()) {
	case color_widgets::ColorWheel::ShapeSquare:
		action =
			rotating ? d->shapeRotatingSquareAction : d->shapeFixedSquareAction;
		break;
	default:
		action = rotating ? d->shapeRotatingTriangleAction
						  : d->shapeFixedTriangleAction;
		break;
	}
	action->setChecked(true);
}

void ColorSpinnerDock::setSwatchFlags(int flags)
{
	bool hideSwatch = flags & COLOR_SWATCH_NO_SPINNER;
	d->lastUsedSwatch->setVisible(!hideSwatch);
}

void ColorSpinnerDock::showContextMenu(const QPoint &pos)
{
#ifdef DP_COLOR_SPINNER_ENABLE_PREVIEW
	hidePreviewPopup();
#endif
	d->menuButton->menu()->popup(d->colorwheel->mapToGlobal(pos));
}

void ColorSpinnerDock::showShadesContextMenu(const QPoint &pos)
{
	if(d->shadeSelector) {
		if(!d->shadeSelectorMenu) {
			d->shadeSelectorMenu = new QMenu(this);
			d->shadeSelectorMenu->addAction(d->showColorShadesAction);
			d->shadeSelectorMenu->addAction(d->configureColorShadesAction);
		}
		d->shadeSelectorMenu->popup(d->shadeSelector->mapToGlobal(pos));
	}
}

void ColorSpinnerDock::showColorShadesDialog()
{
	QString name = QStringLiteral("colorshadesdialog");
	dialogs::ShadeSelectorDialog *dlg =
		findChild<dialogs::ShadeSelectorDialog *>(
			name, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->activateWindow();
		dlg->raise();
	} else {
		dlg = new dialogs::ShadeSelectorDialog(d->colorwheel->color(), this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setObjectName(name);
		utils::showWindow(dlg);
	}
}

}

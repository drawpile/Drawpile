// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/colorcircle.h"
#include "desktop/dialogs/artisticcolorwheeldialog.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/toolsettingsdock.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/artisticcolorwheel.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QScopedValueRollback>
#include <QVBoxLayout>
#include <QtColorWidgets/swatch.hpp>
#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
#	include "desktop/widgets/colorpopup.h"
#endif

namespace docks {

ColorCircleDock::ColorCircleDock(QWidget *parent)
	: DockBase(
		  tr("Color Circle"), tr("Circle"),
		  QIcon::fromTheme("drawpile_colorcircle"), parent)
{
	TitleWidget *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	QMenu *menu = new QMenu(this);

	QAction *configureAction =
		menu->addAction(QIcon::fromTheme("configure"), tr("Configure…"));
	connect(
		configureAction, &QAction::triggered, this,
		&ColorCircleDock::showSettingsDialog);

	QMenu *colorSpaceMenu = menu->addMenu(
		QCoreApplication::translate("docks::ColorSpinnerDock", "Color space"));
	QActionGroup *colorSpaceGroup = new QActionGroup(this);

	m_colorSpaceHsvAction = colorSpaceMenu->addAction(
		QCoreApplication::translate("docks::ColorSpinnerDock", "HSV"));
	m_colorSpaceHsvAction->setCheckable(true);
	colorSpaceGroup->addAction(m_colorSpaceHsvAction);
	connect(
		m_colorSpaceHsvAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !m_updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorHSV);
			}
		});

	m_colorSpaceHslAction = colorSpaceMenu->addAction(
		QCoreApplication::translate("docks::ColorSpinnerDock", "HSL"));
	m_colorSpaceHslAction->setCheckable(true);
	colorSpaceGroup->addAction(m_colorSpaceHslAction);
	connect(
		m_colorSpaceHslAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !m_updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorHSL);
			}
		});

	m_colorSpaceHclAction = colorSpaceMenu->addAction(
		QCoreApplication::translate("docks::ColorSpinnerDock", "HCL"));
	m_colorSpaceHclAction->setCheckable(true);
	colorSpaceGroup->addAction(m_colorSpaceHclAction);
	connect(
		m_colorSpaceHclAction, &QAction::toggled, this, [this](bool toggled) {
			if(toggled && !m_updating) {
				dpApp().settings().setColorWheelSpace(
					color_widgets::ColorWheel::ColorLCH);
			}
		});

#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	m_previewAction = menu->addAction(QCoreApplication::translate(
		"docks::ColorSpinnerDock", "Preview selected color"));
	m_previewAction->setCheckable(true);
	connect(m_previewAction, &QAction::toggled, this, [this](bool toggled) {
		if(!m_updating) {
			dpApp().settings().setColorWheelPreview(toggled ? 1 : 0);
		}
	});
#endif

	menu->addSeparator();
	ColorPaletteDock::addSwatchOptionsToMenu(menu, COLOR_SWATCH_NO_CIRCLE);

	widgets::GroupedToolButton *menuButton =
		new widgets::GroupedToolButton(this);
	menuButton->setIcon(QIcon::fromTheme("application-menu"));
	menuButton->setPopupMode(QToolButton::InstantPopup);
	menuButton->setMenu(menu);

	m_lastUsedSwatch = new color_widgets::Swatch(titlebar);
	m_lastUsedSwatch->setForcedRows(1);
	m_lastUsedSwatch->setForcedColumns(
		docks::ToolSettings::LASTUSED_COLOR_COUNT);
	m_lastUsedSwatch->setReadOnly(true);
	m_lastUsedSwatch->setBorder(Qt::NoPen);
	m_lastUsedSwatch->setMinimumHeight(24);
	utils::setWidgetRetainSizeWhenHidden(m_lastUsedSwatch, true);

	titlebar->addCustomWidget(menuButton);
	titlebar->addSpace(4);
	titlebar->addCustomWidget(m_lastUsedSwatch, 1);
	titlebar->addSpace(4);

	connect(
		m_lastUsedSwatch, &color_widgets::Swatch::colorSelected, this,
		&ColorCircleDock::colorSelected);

	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout(widget);

	m_wheel = new widgets::ArtisticColorWheel;
	m_wheel->setMinimumSize(64, 64);
	layout->addWidget(m_wheel);
	connect(
		m_wheel, &widgets::ArtisticColorWheel::colorSelected, this,
		&ColorCircleDock::colorSelected);
#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	connect(
		m_wheel, &widgets::ArtisticColorWheel::editingStarted, this,
		&ColorCircleDock::showPreviewPopup);
	connect(
		m_wheel, &widgets::ArtisticColorWheel::editingFinished, this,
		&ColorCircleDock::hidePreviewPopup);
#endif
	setWidget(widget);

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindColorCircleHueLimit(
		m_wheel, &widgets::ArtisticColorWheel::setHueLimit);
	settings.bindColorCircleHueCount(
		m_wheel, &widgets::ArtisticColorWheel::setHueCount);
	settings.bindColorCircleHueAngle(
		m_wheel, &widgets::ArtisticColorWheel::setHueAngle);
	settings.bindColorCircleSaturationLimit(
		m_wheel, &widgets::ArtisticColorWheel::setSaturationLimit);
	settings.bindColorCircleSaturationCount(
		m_wheel, &widgets::ArtisticColorWheel::setSaturationCount);
	settings.bindColorCircleValueLimit(
		m_wheel, &widgets::ArtisticColorWheel::setValueLimit);
	settings.bindColorCircleValueCount(
		m_wheel, &widgets::ArtisticColorWheel::setValueCount);
	settings.bindColorCircleGamutMaskPath(
		m_wheel, &widgets::ArtisticColorWheel::setGamutMaskPath);
	settings.bindColorCircleGamutMaskAngle(
		m_wheel, &widgets::ArtisticColorWheel::setGamutMaskAngle);
	settings.bindColorCircleGamutMaskOpacity(
		m_wheel, &widgets::ArtisticColorWheel::setGamutMaskOpacity);
	settings.bindColorWheelSpace(this, &ColorCircleDock::setColorSpace);
#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	settings.bindColorWheelPreview(this, &ColorCircleDock::setPreview);
#endif
	settings.bindColorSwatchFlags(this, &ColorCircleDock::setSwatchFlags);
}

void ColorCircleDock::setColor(const QColor &color)
{
	m_lastUsedSwatch->setSelected(
		findPaletteColor(m_lastUsedSwatch->palette(), color));

	if(m_wheel->color() != color) {
		m_wheel->setColor(color);
		m_lastUsedColor = color;
	} else if(!m_lastUsedColor.isValid()) {
		m_lastUsedColor = color;
	}
#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
	if(m_popup) {
		m_popup->setSelectedColor(color);
	}
#endif
}

void ColorCircleDock::setLastUsedColors(const color_widgets::ColorPalette &pal)
{
	m_lastUsedSwatch->setPalette(pal);
	m_lastUsedSwatch->setSelected(
		findPaletteColor(m_lastUsedSwatch->palette(), m_wheel->color()));
	m_lastUsedColor = pal.count() == 0 ? m_wheel->color() : pal.colorAt(0);
}

void ColorCircleDock::setColorSpace(
	color_widgets::ColorWheel::ColorSpaceEnum colorSpace)
{
	QScopedValueRollback<bool> guard(m_updating, true);
	switch(colorSpace) {
	case color_widgets::ColorWheel::ColorHSL:
		m_colorSpaceHslAction->setChecked(true);
		m_wheel->setColorSpace(color_widgets::ColorWheel::ColorHSL);
		break;
	case color_widgets::ColorWheel::ColorLCH:
		m_colorSpaceHclAction->setChecked(true);
		m_wheel->setColorSpace(color_widgets::ColorWheel::ColorLCH);
		break;
	default:
		m_colorSpaceHsvAction->setChecked(true);
		m_wheel->setColorSpace(color_widgets::ColorWheel::ColorHSV);
		break;
	}
}

#ifdef DP_COLOR_CIRCLE_ENABLE_PREVIEW
void ColorCircleDock::setPreview(int preview)
{
	QScopedValueRollback<bool> guard(m_updating, true);
	bool enabled = preview != 0;
	m_previewAction->setChecked(enabled);
	m_popupEnabled = enabled;
	if(!enabled) {
		hidePreviewPopup();
	}
}

void ColorCircleDock::showPreviewPopup()
{
	if(m_popupEnabled) {
		if(!m_popup) {
			m_popup = new widgets::ColorPopup(this);
			m_popup->setSelectedColor(m_wheel->color());
		}
		m_popup->setPreviousColor(m_wheel->color());
		m_popup->setLastUsedColor(m_lastUsedColor);
		m_popup->showPopup(
			this, isFloating() ? nullptr : parentWidget(),
			mapFromGlobal(m_wheel->mapToGlobal(QPoint(0, 0))).y(), true);
	}
}

void ColorCircleDock::hidePreviewPopup()
{
	if(m_popup) {
		m_popup->hide();
	}
}
#endif

void ColorCircleDock::showSettingsDialog()
{
	dialogs::ArtisticColorWheelDialog *dlg =
		new dialogs::ArtisticColorWheelDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
}

void ColorCircleDock::setSwatchFlags(int flags)
{
	bool hideSwatch = flags & COLOR_SWATCH_NO_CIRCLE;
	m_lastUsedSwatch->setVisible(!hideSwatch);
}

}

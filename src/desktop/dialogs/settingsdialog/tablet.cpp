// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/tablet.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolcontroller.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#ifdef Q_OS_ANDROID
#	include "desktop/dialogs/settingsdialog/touch.h"
#endif

namespace dialogs {
namespace settingsdialog {

Tablet::Tablet(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Tablet::createButtons(QDialogButtonBox *buttons)
{
	m_tabletTesterButton =
		new QPushButton(QIcon::fromTheme("input-tablet"), tr("Tablet Tester"));
	m_tabletTesterButton->setAutoDefault(false);
	buttons->addButton(m_tabletTesterButton, QDialogButtonBox::ActionRole);
	m_tabletTesterButton->setEnabled(false);
	connect(
		m_tabletTesterButton, &QPushButton::clicked, this,
		&Tablet::tabletTesterRequested);
}

void Tablet::showButtons()
{
	m_tabletTesterButton->setEnabled(true);
	m_tabletTesterButton->setVisible(true);
}

void Tablet::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initTablet(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initPressureCurve(cfg, utils::addFormSection(layout));
}

void Tablet::initPressureCurve(config::Config *cfg, QFormLayout *form)
{
	widgets::CurveWidget *curve = new widgets::CurveWidget(this);
	curve->setAxisTitleLabels(tr("Stylus"), tr("Output"));
	curve->setCurveSize(200, 200);
	CFG_BIND_SET(
		cfg, GlobalPressureCurve, curve,
		widgets::CurveWidget::setCurveFromString);
	connect(
		curve, &widgets::CurveWidget::curveChanged, cfg,
		[cfg](const KisCubicCurve &newCurve) {
			cfg->setGlobalPressureCurve(newCurve.toString());
		});
	form->addRow(tr("Global pressure curve:"), curve);
	disableKineticScrollingOnWidget(curve->curveWidget());

	utils::addFormSpacer(form);

	QCheckBox *pressureCurveMode =
		new QCheckBox(tr("Use separate curve for eraser tip"));
	CFG_BIND_CHECKBOX(cfg, GlobalPressureCurveMode, pressureCurveMode);
	form->addRow(tr("Eraser pressure curve:"), pressureCurveMode);

	widgets::CurveWidget *eraserCurve = new widgets::CurveWidget(this);
	eraserCurve->setAxisTitleLabels(tr("Eraser"), tr("Output"));
	eraserCurve->setCurveSize(200, 200);
	CFG_BIND_SET(
		cfg, GlobalPressureCurveEraser, eraserCurve,
		widgets::CurveWidget::setCurveFromString);
	CFG_BIND_SET(
		cfg, GlobalPressureCurveMode, eraserCurve,
		widgets::CurveWidget::setVisible);
	connect(
		eraserCurve, &widgets::CurveWidget::curveChanged, cfg,
		[cfg](const KisCubicCurve &newCurve) {
			cfg->setGlobalPressureCurveEraser(newCurve.toString());
		});
	form->addRow(nullptr, eraserCurve);
	disableKineticScrollingOnWidget(eraserCurve->curveWidget());
}

void Tablet::initTablet(config::Config *cfg, QFormLayout *form)
{
#if defined(Q_OS_WIN)
	QComboBox *driver = new QComboBox;
	driver->addItem(
		tr("Windows Ink"), int(tools::TabletInputMode::KisTabletWinink));
	driver->addItem(
		tr("Windows Ink Non-Native"),
		int(tools::TabletInputMode::KisTabletWininkNonNative));
	driver->addItem(tr("Wintab"), int(tools::TabletInputMode::KisTabletWintab));
	driver->addItem(
		tr("Wintab Relative"),
		int(tools::TabletInputMode::KisTabletWintabRelativePenHack));
#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	driver->addItem(tr("Qt5"), int(tools::TabletInputMode::Qt5));
#	else
	driver->addItem(
		tr("Qt6 Windows Ink"), int(tools::TabletInputMode::Qt6Winink));
	driver->addItem(tr("Qt6 Wintab"), int(tools::TabletInputMode::Qt6Wintab));
#	endif

	CFG_BIND_COMBOBOX_USER_INT(cfg, TabletDriver, driver);
#else
	QLabel *driver =
		new QLabel(QStringLiteral("Qt %1").arg(QString::fromUtf8(qVersion())));
#endif
	form->addRow(tr("Tablet driver:"), driver);

	QCheckBox *pressure = new QCheckBox(tr("Enable pressure sensitivity"));
	CFG_BIND_CHECKBOX(cfg, TabletEvents, pressure);
	form->addRow(tr("Pen pressure:"), pressure);

#ifdef Q_OS_ANDROID
	Touch::addTouchPressureSettingTo(cfg, form);
#endif

	KisSliderSpinBox *smoothing = new KisSliderSpinBox;
	smoothing->setMaximum(tools::ToolController::MAX_SMOOTHING);
	smoothing->setPrefix(tr("Global smoothing: "));
	CFG_BIND_SLIDERSPINBOX(cfg, Smoothing, smoothing);
	form->addRow(tr("Smoothing:"), smoothing);
	disableKineticScrollingOnWidget(smoothing);

	QCheckBox *mouseSmoothing =
		new QCheckBox(tr("Apply global smoothing to mouse"));
	CFG_BIND_CHECKBOX(cfg, MouseSmoothing, mouseSmoothing);
	form->addRow(nullptr, mouseSmoothing);

	QCheckBox *interpolate = new QCheckBox(tr("Compensate jagged curves"));
	CFG_BIND_CHECKBOX(cfg, InterpolateInputs, interpolate);
	form->addRow(tr("Workarounds:"), interpolate);

#ifdef KRITA_QATTRIBUTE_ANDROID_EMULATE_MOUSE_BUTTONS_FOR_PAGE_UP_DOWN
	QCheckBox *pageUpDownWorkaround =
		//: Xiaomi is a brand that makes Android tablets. This is a setting for
		//: a workaround that matters for those tablets.
		new QCheckBox(tr("Translate stylus buttons to clicks (Xiaomi)"));
	CFG_BIND_CHECKBOX(
		cfg, AndroidWorkaroundEmulateMouseButtonsForPageUpDown,
		pageUpDownWorkaround);
	form->addRow(nullptr, pageUpDownWorkaround);
#endif

#ifdef KRITA_QATTRIBUTE_ANDROID_IGNORE_HISTORIC_TABLET_EVENTS
	QCheckBox *ignoreHistoricTabletEventsWorkaround =
		//: Xiaomi is a brand that makes Android tablets. This is a setting for
		//: a workaround that matters for those tablets.
		new QCheckBox(tr("Disregard position history (Xiaomi)"));
	CFG_BIND_CHECKBOX(
		cfg, AndroidWorkaroundIgnoreHistoricTabletEvents,
		ignoreHistoricTabletEventsWorkaround);
	form->addRow(nullptr, ignoreHistoricTabletEventsWorkaround);
#endif

	QComboBox *eraserAction = new QComboBox;
	eraserAction->addItem(
		tr("Treat as regular pen tip"), int(tools::EraserAction::Ignore));
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	eraserAction->addItem(
		tr("Switch to eraser slot"), int(tools::EraserAction::Switch));
#endif
	eraserAction->addItem(
		tr("Erase with current brush"), int(tools::EraserAction::Override));
	CFG_BIND_COMBOBOX_USER_INT(cfg, TabletEraserAction, eraserAction);
	//: This refers to the eraser end tablet pen, not a tooltip or something.
	form->addRow(tr("Eraser tip behavior:"), eraserAction);
}

}
}

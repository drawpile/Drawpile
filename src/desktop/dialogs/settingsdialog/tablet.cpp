// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/tablet.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
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

Tablet::Tablet(desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
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

void Tablet::setUp(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initTablet(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initPressureCurve(settings, utils::addFormSection(layout));
}

void Tablet::initPressureCurve(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	widgets::CurveWidget *curve = new widgets::CurveWidget(this);
	curve->setAxisTitleLabels(tr("Stylus"), tr("Output"));
	curve->setCurveSize(200, 200);
	settings.bindGlobalPressureCurve(
		curve, &widgets::CurveWidget::setCurveFromString);
	connect(
		curve, &widgets::CurveWidget::curveChanged, &settings,
		[&settings](const KisCubicCurve &newCurve) {
			settings.setGlobalPressureCurve(newCurve.toString());
		});
	form->addRow(tr("Global pressure curve:"), curve);
	disableKineticScrollingOnWidget(curve->curveWidget());

	utils::addFormSpacer(form);

	QCheckBox *pressureCurveMode =
		new QCheckBox(tr("Use separate curve for eraser tip"));
	settings.bindGlobalPressureCurveMode(pressureCurveMode);
	form->addRow(tr("Eraser pressure curve:"), pressureCurveMode);

	widgets::CurveWidget *eraserCurve = new widgets::CurveWidget(this);
	eraserCurve->setAxisTitleLabels(tr("Eraser"), tr("Output"));
	eraserCurve->setCurveSize(200, 200);
	settings.bindGlobalPressureCurveEraser(
		eraserCurve, &widgets::CurveWidget::setCurveFromString);
	settings.bindGlobalPressureCurveMode(
		eraserCurve, &widgets::CurveWidget::setVisible);
	connect(
		eraserCurve, &widgets::CurveWidget::curveChanged, &settings,
		[&settings](const KisCubicCurve &newCurve) {
			settings.setGlobalPressureCurveEraser(newCurve.toString());
		});
	form->addRow(nullptr, eraserCurve);
	disableKineticScrollingOnWidget(eraserCurve->curveWidget());
}

void Tablet::initTablet(
	desktop::settings::Settings &settings, QFormLayout *form)
{
#if defined(Q_OS_WIN)
	QComboBox *driver = new QComboBox;
	driver->addItem(
		tr("Windows Ink"),
		QVariant::fromValue(tabletinput::Mode::KisTabletWinink));
	driver->addItem(
		tr("Windows Ink Non-Native"),
		QVariant::fromValue(tabletinput::Mode::KisTabletWininkNonNative));
	driver->addItem(
		tr("Wintab"), QVariant::fromValue(tabletinput::Mode::KisTabletWintab));
	driver->addItem(
		tr("Wintab Relative"),
		QVariant::fromValue(tabletinput::Mode::KisTabletWintabRelativePenHack));
#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	driver->addItem(tr("Qt5"), QVariant::fromValue(tabletinput::Mode::Qt5));
#	else
	driver->addItem(
		tr("Qt6 Windows Ink"),
		QVariant::fromValue(tabletinput::Mode::Qt6Winink));
	driver->addItem(
		tr("Qt6 Wintab"), QVariant::fromValue(tabletinput::Mode::Qt6Wintab));
#	endif

	settings.bindTabletDriver(driver, Qt::UserRole);
#else
	QLabel *driver =
		new QLabel(QStringLiteral("Qt %1").arg(QString::fromUtf8(qVersion())));
#endif
	form->addRow(tr("Tablet driver:"), driver);

	auto *pressure = new QCheckBox(tr("Enable pressure sensitivity"));
	settings.bindTabletEvents(pressure);
	form->addRow(tr("Pen pressure:"), pressure);

#ifdef Q_OS_ANDROID
	Touch::addTouchPressureSettingTo(settings, form);
#endif

	auto *smoothing = new KisSliderSpinBox;
	smoothing->setMaximum(libclient::settings::maxSmoothing);
	smoothing->setPrefix(tr("Global smoothing: "));
	settings.bindSmoothing(smoothing);
	form->addRow(tr("Smoothing:"), smoothing);
	disableKineticScrollingOnWidget(smoothing);

	auto *mouseSmoothing = new QCheckBox(tr("Apply global smoothing to mouse"));
	settings.bindMouseSmoothing(mouseSmoothing);
	form->addRow(nullptr, mouseSmoothing);

	auto *interpolate = new QCheckBox(tr("Compensate jagged curves"));
	settings.bindInterpolateInputs(interpolate);
	form->addRow(nullptr, interpolate);

	auto *eraserAction = new QComboBox;
	eraserAction->addItem(
		tr("Treat as regular pen tip"), int(tabletinput::EraserAction::Ignore));
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	eraserAction->addItem(
		tr("Switch to eraser slot"), int(tabletinput::EraserAction::Switch));
#endif
	eraserAction->addItem(
		tr("Erase with current brush"),
		int(tabletinput::EraserAction::Override));
	settings.bindTabletEraserAction(eraserAction, Qt::UserRole);
	//: This refers to the eraser end tablet pen, not a tooltip or something.
	form->addRow(tr("Eraser tip behavior:"), eraserAction);
}

} // namespace settingsdialog
} // namespace dialogs

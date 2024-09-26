// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/input.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/tablettest.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Input::Input(desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
}

void Input::setUp(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initTablet(settings, layout);
	utils::addFormSeparator(layout);
	initPressureCurve(settings, utils::addFormSection(layout));
#ifdef Q_OS_ANDROID
	utils::addFormSeparator(layout);
	initAndroid(settings, utils::addFormSection(layout));
#endif
}

#ifdef Q_OS_ANDROID
void Input::initAndroid(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *captureVolumeRocker = new QCheckBox(tr("Capture volume rocker"));
	settings.bindCaptureVolumeRocker(captureVolumeRocker);
	form->addRow(tr("Android:"), captureVolumeRocker);
}
#endif

void Input::initPressureCurve(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *curve = new widgets::CurveWidget(this);
	curve->setAxisTitleLabels(tr("Input"), tr("Output"));
	curve->setCurveSize(200, 200);
	settings.bindGlobalPressureCurve(
		curve, &widgets::CurveWidget::setCurveFromString);
	connect(
		curve, &widgets::CurveWidget::curveChanged, &settings,
		[&settings](const KisCubicCurve &newCurve) {
			settings.setGlobalPressureCurve(newCurve.toString());
		});
	form->addRow(tr("Global pressure curve:"), curve);
}

void Input::initTablet(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	QHBoxLayout *section = new QHBoxLayout;
	layout->addLayout(section);

	QFormLayout *form = utils::addFormSection(section);

	auto *pressure = new QCheckBox(tr("Enable pressure sensitivity"));
	settings.bindTabletEvents(pressure);
	form->addRow(tr("Tablet:"), pressure);

	auto *interpolate = new QCheckBox(tr("Compensate jagged curves"));
	settings.bindInterpolateInputs(interpolate);
	form->addRow(nullptr, interpolate);

	auto *smoothing = new KisSliderSpinBox;
	smoothing->setMaximum(libclient::settings::maxSmoothing);
	smoothing->setPrefix(tr("Global smoothing: "));
	settings.bindSmoothing(smoothing);
	form->addRow(nullptr, smoothing);

	auto *mouseSmoothing = new QCheckBox(tr("Apply global smoothing to mouse"));
	settings.bindMouseSmoothing(mouseSmoothing);
	form->addRow(nullptr, mouseSmoothing);

	auto *eraserAction = new QComboBox;
	eraserAction->addItem(
		tr("Do nothing"), int(tabletinput::EraserAction::Ignore));
#ifndef __EMSCRIPTEN__
	eraserAction->addItem(
		tr("Switch to eraser slot"), int(tabletinput::EraserAction::Switch));
#endif
	eraserAction->addItem(
		tr("Erase with current brush"),
		int(tabletinput::EraserAction::Override));
	settings.bindTabletEraserAction(eraserAction, Qt::UserRole);
	//: Eraser refers to the eraser tip of a tablet pen.
	form->addRow(tr("Eraser action:"), eraserAction);

#ifdef Q_OS_WIN
	utils::addFormSpacer(form);

	auto *driver = new QComboBox;
	driver->addItem(
		tr("KisTablet Windows Ink"),
		QVariant::fromValue(tabletinput::Mode::KisTabletWinink));
	driver->addItem(
		tr("KisTablet Wintab"),
		QVariant::fromValue(tabletinput::Mode::KisTabletWintab));
	driver->addItem(
		tr("KisTablet Wintab Relative"),
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
	form->addRow(tr("Driver:"), driver);
#endif

	utils::addFormSpacer(form);

	auto *touchMode = utils::addRadioGroup(
		form, tr("Touch mode:"), true,
		{
			{tr("Touchscreen"), false},
			{tr("Gestures"), true},
		});
	settings.bindTouchGestures(touchMode);

	auto *oneTouch = utils::addRadioGroup(
		form, tr("One-finger input:"), true,
		{
			{tr("None"), int(desktop::settings::OneFingerTouchAction::Nothing)},
			{tr("Draw"), int(desktop::settings::OneFingerTouchAction::Draw)},
			{tr("Pan"), int(desktop::settings::OneFingerTouchAction::Pan)},
			{tr("Guess"), int(desktop::settings::OneFingerTouchAction::Guess)},
		});
	settings.bindOneFingerTouch(oneTouch);
	settings.bindTouchGestures(
		oneTouch->button(int(desktop::settings::OneFingerTouchAction::Draw)),
		&QWidget::setDisabled);

	auto *twoTouch = new utils::EncapsulatedLayout;
	twoTouch->setContentsMargins(0, 0, 0, 0);
	auto *zoom = new QCheckBox(tr("Pinch to zoom"));
	settings.bindTwoFingerZoom(zoom);
	twoTouch->addWidget(zoom);
	auto *rotate = new QCheckBox(tr("Twist to rotate"));
	settings.bindTwoFingerRotate(rotate);
	twoTouch->addWidget(rotate);
	twoTouch->addStretch();
	form->addRow(tr("Touch gestures:"), twoTouch);

	auto *testerLayout = new QVBoxLayout;
	auto *testerLabel = new QLabel(tr("Test your tablet here:"));
	testerLabel->setAlignment(Qt::AlignHCenter);
	testerLayout->addWidget(testerLabel);
	auto *testerFrame = new QFrame;
	testerFrame->setFrameShape(QFrame::StyledPanel);
	testerFrame->setFrameShadow(QFrame::Sunken);
	testerFrame->setFixedSize(194, 194);
	auto *testerFrameLayout = new QHBoxLayout(testerFrame);
	testerFrameLayout->setContentsMargins(0, 0, 0, 0);
	testerFrameLayout->addWidget(new widgets::TabletTester);
	testerLayout->addWidget(testerFrame);
	section->addLayout(testerLayout);
}

} // namespace settingsdialog
} // namespace dialogs

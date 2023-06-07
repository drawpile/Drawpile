// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/input.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/tablettest.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Input::Input(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *form = new utils::SanerFormLayout(this);

	initTablet(settings, form);
	form->addSeparator();
	initPressureCurve(settings, form);
	form->addSeparator();
	initTouch(settings, form);
}

void Input::initPressureCurve(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *curve = new widgets::CurveWidget(this);
	curve->setMinimumHeight(200);
	curve->setAxisTitleLabels(tr("Input"), tr("Output"));
	settings.bindGlobalPressureCurve(curve, &widgets::CurveWidget::setCurveFromString);
	connect(curve, &widgets::CurveWidget::curveChanged, &settings, [&settings](const KisCubicCurve &newCurve) {
		settings.setGlobalPressureCurve(newCurve.toString());
	});
	curve->setFixedButtonWidth(194);
	auto *label = new QLabel(tr("Global pressure curve:"));
	label->setWordWrap(true);
	label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	label->setAlignment(form->labelAlignment() | Qt::AlignTop);
	form->addRow(label, curve, 1, 2);
}

void Input::initTablet(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *pressure = new QCheckBox(tr("Enable pressure sensitivity"));
	settings.bindTabletEvents(pressure);
	form->addRow(tr("Tablet:"), pressure);

	auto *eraser = new QCheckBox(tr("Detect eraser tip"));
	settings.bindTabletEraser(eraser);
	form->addRow(nullptr, eraser);

	form->addSpacer();

#ifdef Q_OS_WIN
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	driver->addItem(
		tr("Qt5"),
		QVariant::fromValue(tabletinput::Mode::Qt5));
#else
	driver->addItem(
		tr("Qt6 Windows Ink"),
		QVariant::fromValue(tabletinput::Mode::Qt6Winink));
	driver->addItem(
		tr("Qt6 Wintab"),
		QVariant::fromValue(tabletinput::Mode::Qt6Wintab));
#endif

	settings.bindTabletDriver(driver, Qt::UserRole);
	form->addRow("Driver:", driver);

	form->addSpacer();
#endif

	auto *smoothing = new QSlider(Qt::Horizontal);
	smoothing->setMaximum(10);
	smoothing->setTickPosition(QSlider::TicksBelow);
	smoothing->setTickInterval(1);
	settings.bindSmoothing(smoothing);
	form->addRow(tr("Smoothing:"), utils::labelEdges(smoothing, tr("Less"), tr("More")));

	form->addSpacer(QSizePolicy::MinimumExpanding);

	auto *testerFrame = new QFrame;
	testerFrame->setFrameShape(QFrame::StyledPanel);
	testerFrame->setFrameShadow(QFrame::Sunken);
	testerFrame->setMinimumSize(194, 194);
	auto *testerLayout = new QHBoxLayout(testerFrame);
	testerLayout->setContentsMargins(0, 0, 0, 0);
	testerLayout->addWidget(new widgets::TabletTester);
	form->addAside(testerFrame, 0);
}

void Input::initTouch(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *oneTouch = form->addRadioGroup(tr("One-finger input:"), true, {
		{ tr("Do nothing"), 0 },
		{ tr("Draw"), 1 },
		{ tr("Scroll"), 2 },
	}, 2);
	settings.bindOneFingerDraw(oneTouch->button(1));
	settings.bindOneFingerScroll(oneTouch->button(2));

	auto *twoTouch = new utils::EncapsulatedLayout;
	twoTouch->setContentsMargins(0, 0, 0, 0);
	auto *zoom = new QCheckBox(tr("Pinch to zoom"));
	settings.bindTwoFingerZoom(zoom);
	twoTouch->addWidget(zoom);
	auto *rotate = new QCheckBox(tr("Twist to rotate"));
	settings.bindTwoFingerRotate(rotate);
	twoTouch->addWidget(rotate);
	twoTouch->addStretch();
	form->addRow(tr("Touch gestures:"), twoTouch, 1, 2);
}

} // namespace settingsdialog
} // namespace dialogs

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
	auto *kisTablet = new QCheckBox(tr("Use Krita tablet support"));
	utils::setSpacingControlType(kisTablet, QSizePolicy::RadioButton);
	form->addRow(tr("Driver:"), kisTablet);

	auto *drivers = form->addRadioGroup(nullptr, false, {
		{ tr("Windows Ink"), 0 },
		{ tr("Wintab"), 1 }
	});

	auto *relativePenMode = new QCheckBox(tr("Relative pen mode"));
	utils::setSpacingControlType(relativePenMode, QSizePolicy::RadioButton);
	form->addRow(nullptr, utils::indent(relativePenMode));

	auto updateMode = [=, &settings] {
		using tabletinput::Mode;

		auto mode = Mode::Uninitialized;
		auto kisMode = kisTablet->isChecked();
		if (drivers->checkedId() == 1) {
			if (relativePenMode->isChecked()) {
				mode = Mode::KisTabletWintabRelativePenHack;
			} else {
				mode = kisMode
					? Mode::KisTabletWintab
					: Mode::Qt6Wintab;
			}
		} else {
			mode = kisMode ? Mode::KisTabletWinink : Mode::Qt6Winink;
		}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		if (mode == Mode::Qt6Winink || mode == Mode::Qt6Wintab) {
			mode = Mode::Qt5;
		}
#endif

		settings.setTabletDriver(mode);
	};
	connect(drivers, QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled), this, updateMode);
	connect(relativePenMode, &QCheckBox::toggled, this, updateMode);
	connect(kisTablet, &QCheckBox::toggled, this, updateMode);

	settings.bindTabletDriver(this, [=](tabletinput::Mode mode) {
		using tabletinput::Mode;

		// Qt6 cannot use relative pen mode, but it is confusing to keep the
		// box disabled since it is not obvious why it is disabled. Instead,
		// if someone tries to enable it, just switch to KisTablet mode.
		auto canRelativeMode = mode == Mode::KisTabletWintab
			|| mode == Mode::KisTabletWintabRelativePenHack
			|| mode == Mode::Qt6Wintab;
		auto kisMode = mode == Mode::KisTabletWintab
			|| mode == Mode::KisTabletWintabRelativePenHack
			|| mode == Mode::KisTabletWinink;
		auto winInk = mode == Mode::KisTabletWinink
			|| mode == Mode::Qt6Winink
			|| mode == Mode::Qt5;

		kisTablet->setChecked(kisMode);
		relativePenMode->setEnabled(canRelativeMode);
		relativePenMode->setChecked(mode == Mode::KisTabletWintabRelativePenHack);
		drivers->button(winInk ? 0 : 1)->setChecked(true);
		drivers->button(1)->setEnabled(mode != Mode::Qt5);
	});

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

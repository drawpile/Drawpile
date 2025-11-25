// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/touch.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/view/enums.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Touch::Touch(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Touch::createButtons(QDialogButtonBox *buttons)
{
	m_touchTesterButton = new QPushButton(
		QIcon::fromTheme("input-touchscreen"), tr("Touch Tester"));
	m_touchTesterButton->setAutoDefault(false);
	buttons->addButton(m_touchTesterButton, QDialogButtonBox::ActionRole);
	m_touchTesterButton->setEnabled(false);
	connect(
		m_touchTesterButton, &QPushButton::clicked, this,
		&Touch::touchTesterRequested);
}

void Touch::showButtons()
{
	m_touchTesterButton->setEnabled(true);
	m_touchTesterButton->setVisible(true);
}

void Touch::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initMode(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTouchActions(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTapActions(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTapAndHoldActions(cfg, utils::addFormSection(layout));
}

void Touch::addTouchPressureSettingTo(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *touchDrawPressure =
		new QCheckBox(tr("Enable pressure for touch drawing"));
	CFG_BIND_CHECKBOX(cfg, TouchDrawPressure, touchDrawPressure);
	form->addRow(nullptr, touchDrawPressure);
	form->addRow(
		nullptr, utils::formNote(
					 tr("Required for certain screen tablets, probably doesn't "
						"work for finger drawing!")));
}

void Touch::initMode(config::Config *cfg, QFormLayout *form)
{
	QButtonGroup *touchMode = utils::addRadioGroup(
		form, tr("Touch mode:"), true,
		{
			{tr("Touchscreen"), false},
			{tr("Gestures"), true},
		});
	CFG_BIND_BUTTONGROUP_TYPE(cfg, TouchGestures, touchMode, bool);
	addTouchPressureSettingTo(cfg, form);
}

void Touch::initTapActions(config::Config *cfg, QFormLayout *form)
{
	QComboBox *oneFingerTap = new QComboBox;
	QComboBox *twoFingerTap = new QComboBox;
	QComboBox *threeFingerTap = new QComboBox;
	QComboBox *fourFingerTap = new QComboBox;
	for(QComboBox *tap :
		{oneFingerTap, twoFingerTap, threeFingerTap, fourFingerTap}) {
		tap->addItem(tr("No action"), int(view::TouchTapAction::Nothing));
		tap->addItem(tr("Undo"), int(view::TouchTapAction::Undo));
		tap->addItem(tr("Redo"), int(view::TouchTapAction::Redo));
		tap->addItem(tr("Hide docks"), int(view::TouchTapAction::HideDocks));
		tap->addItem(
			tr("Toggle color picker"), int(view::TouchTapAction::ColorPicker));
		tap->addItem(tr("Toggle eraser"), int(view::TouchTapAction::Eraser));
		tap->addItem(
			tr("Toggle erase mode"), int(view::TouchTapAction::EraseMode));
		tap->addItem(
			tr("Toggle recolor mode"), int(view::TouchTapAction::RecolorMode));
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTap, oneFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, TwoFingerTap, twoFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, ThreeFingerTap, threeFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, FourFingerTap, fourFingerTap);

	CFG_BIND_SET(cfg, TouchGestures, twoFingerTap, QComboBox::setDisabled);
	CFG_BIND_SET(cfg, TouchGestures, threeFingerTap, QComboBox::setDisabled);
	CFG_BIND_SET(cfg, TouchGestures, fourFingerTap, QComboBox::setDisabled);

	form->addRow(tr("One-finger tap:"), oneFingerTap);
	form->addRow(tr("Two-finger tap:"), twoFingerTap);
	form->addRow(tr("Three-finger tap:"), threeFingerTap);
	form->addRow(tr("Four-finger tap:"), fourFingerTap);
}

void Touch::initTapAndHoldActions(config::Config *cfg, QFormLayout *form)
{
	QComboBox *oneFingerTapAndHold = new QComboBox;
	for(QComboBox *tapAndHold : {oneFingerTapAndHold}) {
		tapAndHold->addItem(
			tr("No action"), int(view::TouchTapAndHoldAction::Nothing));
		tapAndHold->addItem(
			tr("Pick color"), int(view::TouchTapAndHoldAction::ColorPickMode));
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTapAndHold, oneFingerTapAndHold);

	CFG_BIND_SET(
		cfg, TouchGestures, oneFingerTapAndHold, QComboBox::setDisabled);

	form->addRow(tr("One-finger tap and hold:"), oneFingerTapAndHold);
}

void Touch::initTouchActions(config::Config *cfg, QFormLayout *form)
{
	QComboBox *oneFingerTouch = new QComboBox;
	oneFingerTouch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	oneFingerTouch->addItem(
		tr("No action"), int(view::OneFingerTouchAction::Nothing));
	oneFingerTouch->addItem(tr("Draw"), int(view::OneFingerTouchAction::Draw));
	oneFingerTouch->addItem(
		tr("Pan canvas"), int(view::OneFingerTouchAction::Pan));
	oneFingerTouch->addItem(
		tr("Guess"), int(view::OneFingerTouchAction::Guess));
	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTouch, oneFingerTouch);
	form->addRow(tr("One-finger touch:"), oneFingerTouch);

	QComboBox *twoFingerPinch = new QComboBox;
	twoFingerPinch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerPinch->addItem(
		tr("No action"), int(view::TwoFingerPinchAction::Nothing));
	twoFingerPinch->addItem(tr("Zoom"), int(view::TwoFingerPinchAction::Zoom));
	CFG_BIND_COMBOBOX_USER_INT(cfg, TwoFingerPinch, twoFingerPinch);
	form->addRow(tr("Two-finger pinch:"), twoFingerPinch);

	QComboBox *twoFingerTwist = new QComboBox;
	twoFingerTwist->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerTwist->addItem(
		tr("No action"), int(view::TwoFingerPinchAction::Nothing));
	twoFingerTwist->addItem(
		tr("Rotate canvas"), int(view::TwoFingerTwistAction::Rotate));
	twoFingerTwist->addItem(
		tr("Free rotate canvas"),
		int(view::TwoFingerTwistAction::RotateNoSnap));
	twoFingerTwist->addItem(
		tr("Ratchet rotate canvas"),
		int(view::TwoFingerTwistAction::RotateDiscrete));
	CFG_BIND_COMBOBOX_USER_INT(cfg, TwoFingerTwist, twoFingerTwist);
	form->addRow(tr("Two-finger twist:"), twoFingerTwist);

	KisSliderSpinBox *touchSmoothing = new KisSliderSpinBox;
	touchSmoothing->setRange(0, 100);
	touchSmoothing->setPrefix(tr("Smoothing:"));
	touchSmoothing->setSuffix(tr("%"));
	touchSmoothing->setBlockUpdateSignalOnDrag(true);
	disableKineticScrollingOnWidget(touchSmoothing);
	CFG_BIND_SLIDERSPINBOX(cfg, TouchSmoothing, touchSmoothing);
	form->addRow(nullptr, touchSmoothing);
}

}
}

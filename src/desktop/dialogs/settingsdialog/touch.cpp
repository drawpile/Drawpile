// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/touch.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Touch::Touch(desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
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

void Touch::setUp(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initMode(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTouchActions(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTapActions(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initTapAndHoldActions(settings, utils::addFormSection(layout));
}

void Touch::initMode(desktop::settings::Settings &settings, QFormLayout *form)
{
	QButtonGroup *touchMode = utils::addRadioGroup(
		form, tr("Touch mode:"), true,
		{
			{tr("Touchscreen"), false},
			{tr("Gestures"), true},
		});
	settings.bindTouchGestures(touchMode);
}

void Touch::initTapActions(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QComboBox *oneFingerTap = new QComboBox;
	QComboBox *twoFingerTap = new QComboBox;
	QComboBox *threeFingerTap = new QComboBox;
	QComboBox *fourFingerTap = new QComboBox;
	for(QComboBox *tap :
		{oneFingerTap, twoFingerTap, threeFingerTap, fourFingerTap}) {
		tap->addItem(
			tr("No action"), int(desktop::settings::TouchTapAction::Nothing));
		tap->addItem(tr("Undo"), int(desktop::settings::TouchTapAction::Undo));
		tap->addItem(tr("Redo"), int(desktop::settings::TouchTapAction::Redo));
		tap->addItem(
			tr("Hide docks"),
			int(desktop::settings::TouchTapAction::HideDocks));
		tap->addItem(
			tr("Toggle color picker"),
			int(desktop::settings::TouchTapAction::ColorPicker));
		tap->addItem(
			tr("Toggle eraser"),
			int(desktop::settings::TouchTapAction::Eraser));
		tap->addItem(
			tr("Toggle erase mode"),
			int(desktop::settings::TouchTapAction::EraseMode));
		tap->addItem(
			tr("Toggle recolor mode"),
			int(desktop::settings::TouchTapAction::RecolorMode));
	}

	settings.bindOneFingerTap(oneFingerTap, Qt::UserRole);
	settings.bindTwoFingerTap(twoFingerTap, Qt::UserRole);
	settings.bindThreeFingerTap(threeFingerTap, Qt::UserRole);
	settings.bindFourFingerTap(fourFingerTap, Qt::UserRole);

	settings.bindTouchGestures(twoFingerTap, &QComboBox::setDisabled);
	settings.bindTouchGestures(threeFingerTap, &QComboBox::setDisabled);
	settings.bindTouchGestures(fourFingerTap, &QComboBox::setDisabled);

	form->addRow(tr("One-finger tap:"), oneFingerTap);
	form->addRow(tr("Two-finger tap:"), twoFingerTap);
	form->addRow(tr("Three-finger tap:"), threeFingerTap);
	form->addRow(tr("Four-finger tap:"), fourFingerTap);
}

void Touch::initTapAndHoldActions(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QComboBox *oneFingerTapAndHold = new QComboBox;
	for(QComboBox *tapAndHold : {oneFingerTapAndHold}) {
		tapAndHold->addItem(
			tr("No action"),
			int(desktop::settings::TouchTapAndHoldAction::Nothing));
		tapAndHold->addItem(
			tr("Pick color"),
			int(desktop::settings::TouchTapAndHoldAction::ColorPickMode));
	}

	settings.bindOneFingerTapAndHold(oneFingerTapAndHold, Qt::UserRole);

	settings.bindTouchGestures(oneFingerTapAndHold, &QComboBox::setDisabled);

	form->addRow(tr("One-finger tap and hold:"), oneFingerTapAndHold);
}

void Touch::initTouchActions(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QComboBox *oneFingerTouch = new QComboBox;
	oneFingerTouch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	oneFingerTouch->addItem(
		tr("No action"), int(desktop::settings::OneFingerTouchAction::Nothing));
	oneFingerTouch->addItem(
		tr("Draw"), int(desktop::settings::OneFingerTouchAction::Draw));
	oneFingerTouch->addItem(
		tr("Pan canvas"), int(desktop::settings::OneFingerTouchAction::Pan));
	oneFingerTouch->addItem(
		tr("Guess"), int(desktop::settings::OneFingerTouchAction::Guess));
	settings.bindOneFingerTouch(oneFingerTouch, Qt::UserRole);
	form->addRow(tr("One-finger touch:"), oneFingerTouch);

	QComboBox *twoFingerPinch = new QComboBox;
	twoFingerPinch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerPinch->addItem(
		tr("No action"), int(desktop::settings::TwoFingerPinchAction::Nothing));
	twoFingerPinch->addItem(
		tr("Zoom"), int(desktop::settings::TwoFingerPinchAction::Zoom));
	settings.bindTwoFingerPinch(twoFingerPinch, Qt::UserRole);
	form->addRow(tr("Two-finger pinch:"), twoFingerPinch);

	QComboBox *twoFingerTwist = new QComboBox;
	twoFingerTwist->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	twoFingerTwist->addItem(
		tr("No action"), int(desktop::settings::TwoFingerPinchAction::Nothing));
	twoFingerTwist->addItem(
		tr("Rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::Rotate));
	twoFingerTwist->addItem(
		tr("Free rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::RotateNoSnap));
	twoFingerTwist->addItem(
		tr("Ratchet rotate canvas"),
		int(desktop::settings::TwoFingerTwistAction::RotateDiscrete));
	settings.bindTwoFingerTwist(twoFingerTwist, Qt::UserRole);
	form->addRow(tr("Two-finger twist:"), twoFingerTwist);
}

} // namespace settingsdialog
} // namespace dialogs

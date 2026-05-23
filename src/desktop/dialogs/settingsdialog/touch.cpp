// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/touch.h"
#include "desktop/dialogs/actionpickerdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/utils/clickeventfilter.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libclient/view/enums.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
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
	initTapExtraActions(cfg, utils::addFormSection(layout));
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

Touch::TapActionWidget::TapActionWidget(
	config::Config *cfg, const GetTriggerFn &getTrigger,
	const SetTriggerFn &setTrigger, QWidget *parent)
	: QWidget(parent)
	, m_cfg(cfg)
	, m_getTrigger(getTrigger)
	, m_setTrigger(setTrigger)
{
	setContentsMargins(0, 0, 0, 0);

	QHBoxLayout *widgetLayout = new QHBoxLayout(this);
	widgetLayout->setContentsMargins(0, 0, 0, 0);
	widgetLayout->setSpacing(0);

	m_lineEdit = new QLineEdit;
	m_lineEdit->setReadOnly(true);
	m_lineEdit->setPlaceholderText(
		QCoreApplication::translate(
			"CanvasShortcutDialog", "Choose an action"));
	widgetLayout->addWidget(m_lineEdit, 1);

	m_changeButton = new QPushButton;
	m_changeButton->setText(
		QCoreApplication::translate(
			"CanvasShortcutDialog", "Change\342\200\246"));
	widgetLayout->addWidget(m_changeButton);

	ClickEventFilter *triggerEditClickEventFilter = new ClickEventFilter(this);
	m_lineEdit->installEventFilter(triggerEditClickEventFilter);
	connect(
		triggerEditClickEventFilter, &ClickEventFilter::clicked, this,
		&TapActionWidget::changeTrigger);
	connect(
		m_changeButton, &QPushButton::clicked, this,
		&TapActionWidget::changeTrigger);
}

void Touch::TapActionWidget::updateTapAction(int tapAction)
{
	setVisible(tapAction == int(view::TouchTapAction::TriggerAction));
}

void Touch::TapActionWidget::updateTriggerLabel(const QString &trigger)
{
	if(trigger.isEmpty()) {
		m_lineEdit->clear();
	} else {
		m_lineEdit->setText(
			CustomShortcutModel::getCustomizableActionTitle(trigger));
	}
}

void Touch::TapActionWidget::changeTrigger()
{
	if(isEnabled()) {
		QString objectName = QStringLiteral("actionpickerdialog");
		ActionPickerDialog *dlg = findChild<ActionPickerDialog *>(
			objectName, Qt::FindDirectChildrenOnly);
		if(dlg) {
			dlg->activateWindow();
			dlg->raise();
		} else {
			dlg = new ActionPickerDialog(this);
			dlg->setAttribute(Qt::WA_DeleteOnClose);
			dlg->setObjectName(objectName);
			dlg->setSelectedAction(m_getTrigger(m_cfg));
			connect(
				dlg, &ActionPickerDialog::actionSelected, this,
				&TapActionWidget::setTriggerInConfig);
			utils::showWindow(dlg);
		}
	}
}

void Touch::TapActionWidget::setTriggerInConfig(const QString &action)
{
	m_setTrigger(m_cfg, action);
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
	QComboBox *oneFingerTap = makeTapCombo();
	QComboBox *twoFingerTap = makeTapCombo();
	QComboBox *threeFingerTap = makeTapCombo();
	QComboBox *fourFingerTap = makeTapCombo();

	TapActionWidget *oneFingerTapWidget = new TapActionWidget(
		cfg, &config::Config::getOneFingerTapTrigger,
		&config::Config::setOneFingerTapTrigger);
	TapActionWidget *twoFingerTapWidget = new TapActionWidget(
		cfg, &config::Config::getTwoFingerTapTrigger,
		&config::Config::setTwoFingerTapTrigger);
	TapActionWidget *threeFingerTapWidget = new TapActionWidget(
		cfg, &config::Config::getThreeFingerTapTrigger,
		&config::Config::setThreeFingerTapTrigger);
	TapActionWidget *fourFingerTapWidget = new TapActionWidget(
		cfg, &config::Config::getFourFingerTapTrigger,
		&config::Config::setFourFingerTapTrigger);

	form->addRow(tr("One-finger tap:"), oneFingerTap);
	form->addRow(nullptr, oneFingerTapWidget);
	form->addRow(tr("Two-finger tap:"), twoFingerTap);
	form->addRow(nullptr, twoFingerTapWidget);
	form->addRow(tr("Three-finger tap:"), threeFingerTap);
	form->addRow(nullptr, threeFingerTapWidget);
	form->addRow(tr("Four-finger tap:"), fourFingerTap);
	form->addRow(nullptr, fourFingerTapWidget);

	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTap, oneFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, TwoFingerTap, twoFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, ThreeFingerTap, threeFingerTap);
	CFG_BIND_COMBOBOX_USER_INT(cfg, FourFingerTap, fourFingerTap);

	CFG_BIND_SET(
		cfg, OneFingerTap, oneFingerTapWidget,
		TapActionWidget::updateTapAction);
	CFG_BIND_SET(
		cfg, OneFingerTapTrigger, oneFingerTapWidget,
		TapActionWidget::updateTriggerLabel);
	CFG_BIND_SET(
		cfg, TwoFingerTapTrigger, twoFingerTapWidget,
		TapActionWidget::updateTriggerLabel);
	CFG_BIND_SET(
		cfg, TwoFingerTap, twoFingerTapWidget,
		TapActionWidget::updateTapAction);
	CFG_BIND_SET(
		cfg, ThreeFingerTapTrigger, threeFingerTapWidget,
		TapActionWidget::updateTriggerLabel);
	CFG_BIND_SET(
		cfg, ThreeFingerTap, threeFingerTapWidget,
		TapActionWidget::updateTapAction);
	CFG_BIND_SET(
		cfg, FourFingerTapTrigger, fourFingerTapWidget,
		TapActionWidget::updateTriggerLabel);
	CFG_BIND_SET(
		cfg, FourFingerTap, fourFingerTapWidget,
		TapActionWidget::updateTapAction);

	CFG_BIND_SET(cfg, TouchGestures, twoFingerTap, QComboBox::setDisabled);
	CFG_BIND_SET(
		cfg, TouchGestures, twoFingerTapWidget, TapActionWidget::setDisabled);
	CFG_BIND_SET(cfg, TouchGestures, threeFingerTap, QComboBox::setDisabled);
	CFG_BIND_SET(
		cfg, TouchGestures, threeFingerTapWidget, TapActionWidget::setDisabled);
	CFG_BIND_SET(cfg, TouchGestures, fourFingerTap, QComboBox::setDisabled);
	CFG_BIND_SET(
		cfg, TouchGestures, fourFingerTapWidget, TapActionWidget::setDisabled);
}

void Touch::initTapExtraActions(config::Config *cfg, QFormLayout *form)
{
	QComboBox *oneFingerTapAndHold = new QComboBox;
	for(QComboBox *tapAndHold : {oneFingerTapAndHold}) {
		tapAndHold->addItem(
			tr("No action"), int(view::TouchTapAndHoldAction::Nothing));
		tapAndHold->addItem(
			tr("Pick color"), int(view::TouchTapAndHoldAction::ColorPickMode));
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTapAndHold, oneFingerTapAndHold);

	form->addRow(tr("One-finger tap and hold:"), oneFingerTapAndHold);

	QComboBox *oneFingerDoubleTap = makeTapCombo();
	TapActionWidget *oneFingerDoubleTapWidget = new TapActionWidget(
		cfg, &config::Config::getOneFingerDoubleTapTrigger,
		&config::Config::setOneFingerDoubleTapTrigger);

	form->addRow(tr("One-finger double-tap:"), oneFingerDoubleTap);
	form->addRow(nullptr, oneFingerDoubleTapWidget);

	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerDoubleTap, oneFingerDoubleTap);
	CFG_BIND_SET(
		cfg, OneFingerDoubleTap, oneFingerDoubleTapWidget,
		TapActionWidget::updateTapAction);
	CFG_BIND_SET(
		cfg, OneFingerDoubleTapTrigger, oneFingerDoubleTapWidget,
		TapActionWidget::updateTriggerLabel);

	CFG_BIND_SET(
		cfg, TouchGestures, oneFingerTapAndHold, QComboBox::setDisabled);
}

void Touch::initTouchActions(config::Config *cfg, QFormLayout *form)
{
	QString drawText = tr("Draw");
	QString panText = tr("Pan canvas");
	QString guessTemplate = tr("Automatic (%1)");
	QComboBox *oneFingerTouch = new QComboBox;
	oneFingerTouch->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	oneFingerTouch->addItem(
		tr("No action"), int(view::OneFingerTouchAction::Nothing));
	oneFingerTouch->addItem(drawText, int(view::OneFingerTouchAction::Draw));
	oneFingerTouch->addItem(panText, int(view::OneFingerTouchAction::Pan));
	oneFingerTouch->addItem(
		//: Option for the one-finger touch gesture. %1 is either "Draw" or "Pan
		//: canvas", depending on whether Drawpile has detected a stylus.
		guessTemplate.arg(drawText), int(view::OneFingerTouchAction::Guess));
	CFG_BIND_COMBOBOX_USER_INT(cfg, OneFingerTouch, oneFingerTouch);
	form->addRow(tr("One-finger touch:"), oneFingerTouch);

	auto updateOneFingerTouchGuessActionText =
		[oneFingerTouch, guessPanText = guessTemplate.arg(panText)] {
			int i = oneFingerTouch->findData(
				int(view::OneFingerTouchAction::Guess));
			if(i != -1) {
				oneFingerTouch->setItemText(i, guessPanText);
			}
		};
	DrawpileApp &app = dpApp();
	connect(
		&app, &DrawpileApp::tabletEventReceived, this,
		updateOneFingerTouchGuessActionText);
	if(app.anyTabletEventsReceived()) {
		updateOneFingerTouchGuessActionText();
	}

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

QComboBox *Touch::makeTapCombo()
{
	QComboBox *tap = new QComboBox;
	tap->addItem(tr("Do nothing"), int(view::TouchTapAction::Nothing));
	tap->addItem(tr("Undo"), int(view::TouchTapAction::Undo));
	tap->addItem(tr("Redo"), int(view::TouchTapAction::Redo));
	tap->addItem(tr("Hide docks"), int(view::TouchTapAction::HideDocks));
	tap->addItem(
		tr("Toggle color picker"), int(view::TouchTapAction::ColorPicker));
	tap->addItem(tr("Toggle eraser"), int(view::TouchTapAction::Eraser));
	tap->addItem(tr("Toggle erase mode"), int(view::TouchTapAction::EraseMode));
	tap->addItem(
		tr("Toggle recolor mode"), int(view::TouchTapAction::RecolorMode));
	tap->addItem(
		tr("Trigger action"), int(view::TouchTapAction::TriggerAction));
	return tap;
}

}
}

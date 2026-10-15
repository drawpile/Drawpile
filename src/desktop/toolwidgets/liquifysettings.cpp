// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/liquifysettings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/noscroll.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/tools/liquify.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/strings.h"
#include <QActionGroup>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> size{
	QStringLiteral("size"), 60, 5, 2000},
	sizeDynamic{QStringLiteral("sizeDynamic"), 0, 0, 1},
	amount{QStringLiteral("amount"), 30, 1, 100},
	amountDynamic{QStringLiteral("amountDynamic"), 0, 0, 1},
	hardness{QStringLiteral("hardness"), 50, 1, 100},
	hardnessDynamic{QStringLiteral("hardnessDynamic"), 0, 0, 1},
	spacing{QStringLiteral("spacing"), 5, 1, 999};
}

LiquifySettings::LiquifySettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
	connect(
		ctrl, &ToolController::modelChanged, this, &LiquifySettings::setModel);
}

void LiquifySettings::setActions(QActionGroup *liquifyOperations)
{
	Q_ASSERT(liquifyOperations);
	Q_ASSERT(m_headerWidget);
	Q_ASSERT(!m_liquifyOperations);
	m_liquifyOperations = liquifyOperations;

	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);

	widgets::GroupedToolButton *headerMenuButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	headerLayout->addWidget(headerMenuButton);
	headerMenuButton->setIcon(
		QIcon::fromTheme(QStringLiteral("application-menu")));
	headerMenuButton->setPopupMode(QToolButton::InstantPopup);

	m_headerMenu = new QMenu(headerMenuButton);
	headerMenuButton->setMenu(m_headerMenu);

	QAction *resetAction = m_headerMenu->addAction(
		QIcon::fromTheme(QStringLiteral("view-refresh")),
		tr("Reset to default settings"));
	connect(
		resetAction, &QAction::triggered, this,
		&LiquifySettings::loadDefaultSettings);

	headerLayout->addStretch(1);

	QList<QAction *> operationActions = liquifyOperations->actions();
	int operationCount = operationActions.size();
	Q_ASSERT(operationCount == int(Operation::Last) + 1);
	for(int i = 0; i < operationCount; ++i) {
		widgets::GroupedToolButton *button = new widgets::GroupedToolButton(
			i == 0 ? widgets::GroupedToolButton::GroupLeft
			: i == int(Operation::Last)
				? widgets::GroupedToolButton::GroupRight
				: widgets::GroupedToolButton::GroupCenter);
		button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		button->setDefaultAction(operationActions[i]);
		headerLayout->addWidget(button, 1);
	}

	headerLayout->addStretch(1);

	pushSettings();
	connect(
		liquifyOperations, &QActionGroup::triggered, this,
		&LiquifySettings::pushSettings);
}

void LiquifySettings::toggleEraserMode()
{
	if(selectedOperation() == int(Operation::Erase)) {
		setOperation(m_lastNonEraserOperation);
	} else {
		setOperation(int(Operation::Erase));
	}
}

void LiquifySettings::quickAdjust1(qreal adjustment, bool wheel)
{
	quickAdjustBrushSizeOn(m_sizeSlider, adjustment, wheel, m_quickAdjust1);
}

void LiquifySettings::quickAdjust2(qreal adjustment, bool wheel)
{
	quickAdjustOn(m_amountSlider, adjustment, wheel, m_quickAdjust2);
}

void LiquifySettings::quickAdjust3(qreal adjustment, bool wheel)
{
	quickAdjustOn(m_hardnessSlider, adjustment, wheel, m_quickAdjust3);
}

void LiquifySettings::stepAdjust1(bool increase)
{
	m_sizeSlider->setValue(m_sizeSlider->value() + (increase ? 1 : -1));
}

void LiquifySettings::stepAdjust2(bool increase)
{
	m_amountSlider->setValue(m_amountSlider->value() + (increase ? 1 : -1));
}

void LiquifySettings::stepAdjust3(bool increase)
{
	m_hardnessSlider->setValue(m_hardnessSlider->value() + (increase ? 1 : -1));
}

int LiquifySettings::getSize() const
{
	return m_lastEmittedSize;
}

void LiquifySettings::setFeatureAccess(bool featureAccess)
{
	m_featureAccess = featureAccess;
}

ToolProperties LiquifySettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::size, m_sizeSlider->value());
	cfg.setValue(props::sizeDynamic, m_sizePressureButton->isChecked() ? 1 : 0);
	cfg.setValue(props::amount, m_amountSlider->value());
	cfg.setValue(
		props::amountDynamic, m_amountPressureButton->isChecked() ? 1 : 0);
	cfg.setValue(props::hardness, m_hardnessSlider->value());
	cfg.setValue(
		props::hardnessDynamic, m_hardnessPressureButton->isChecked() ? 1 : 0);
	cfg.setValue(props::spacing, m_spacingSlider->value());
	return cfg;
}

void LiquifySettings::restoreToolSettings(const ToolProperties &cfg)
{
	utils::setSpinnerValueSignalsBlocked(m_sizeSlider, cfg.value(props::size));
	utils::setButtonCheckedSignalsBlocked(
		m_sizePressureButton, cfg.value(props::sizeDynamic) != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_amountSlider, cfg.value(props::amount));
	utils::setButtonCheckedSignalsBlocked(
		m_amountPressureButton, cfg.value(props::amountDynamic) != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_hardnessSlider, cfg.value(props::hardness));
	utils::setButtonCheckedSignalsBlocked(
		m_hardnessPressureButton, cfg.value(props::hardnessDynamic) != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_spacingSlider, cfg.value(props::spacing));
	utils::clickButtonGroupSignalsBlocked(
		m_interpolationGroup, DP_MSG_TRANSFORM_REGION_MODE_BILINEAR);
	pushSettings();
}

void LiquifySettings::pushSettings()
{
	controller()->setLiquifyParams(m_interpolationGroup->checkedId());

	LiquifyTool::Properties props;
	props.size = qreal(m_sizeSlider->value());
	props.amount = qreal(m_amountSlider->value()) / 100.0;
	props.hardness =
		qreal(m_hardnessSlider->value() - 1) / 100.0 * HARDNESS_MULTIPLIER;
	props.spacing = qreal(m_spacingSlider->value()) / 100.0;
	props.sizePressure = m_sizePressureButton->isChecked();
	props.amountPressure = m_amountPressureButton->isChecked();
	props.hardnessPressure = m_hardnessPressureButton->isChecked();

	int operation = selectedOperation();
	if(operation != int(Operation::Erase)) {
		m_lastNonEraserOperation = operation;
	}

	switch(operation) {
	case int(Operation::Move):
		props.operation = LiquifyTool::Operation::Move;
		break;
	case int(Operation::RotateLeft):
		props.operation = LiquifyTool::Operation::Rotate;
		break;
	case int(Operation::RotateRight):
		props.operation = LiquifyTool::Operation::Rotate;
		props.reverse = true;
		break;
	case int(Operation::Expand):
		props.operation = LiquifyTool::Operation::Scale;
		break;
	case int(Operation::Shrink):
		props.operation = LiquifyTool::Operation::Scale;
		props.reverse = true;
		break;
	case int(Operation::Smoothe):
		props.operation = LiquifyTool::Operation::Smoothe;
		break;
	case int(Operation::Erase):
		props.operation = LiquifyTool::Operation::Erase;
		break;
	default:
		qWarning("Unhandled liquify operation %d", operation);
		break;
	}

	controller()->liquifyTool()->setProperties(props);

	if(props.size != m_lastEmittedSize) {
		m_lastEmittedSize = props.size;
		Q_EMIT sizeChanged(getSize());
	}
}

QWidget *LiquifySettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	m_stack = new QStackedWidget(parent);

	QWidget *widget = new QWidget;
	QGridLayout *grid = new QGridLayout(widget);
	grid->setContentsMargins(3, 3, 3, 3);
	grid->setSpacing(3);
	m_stack->addWidget(widget);

	int row = 0;

	m_sizeSlider = new widgets::NoScrollKisSliderSpinBox;
	m_sizeSlider->setRange(props::size.min, props::size.max);
	m_sizeSlider->setExponentRatio(3.0);
	m_sizeSlider->setPrefix(tr("Size: "));
	m_sizeSlider->setSuffix(strings::px());
	m_sizeSlider->setBlockUpdateSignalOnDrag(true);
	grid->addWidget(m_sizeSlider, row, 0, 1, 2);
	connect(
		m_sizeSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&LiquifySettings::pushSettings);

	m_sizePressureButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_sizePressureButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_sizePressureButton->setCheckable(true);
	m_sizePressureButton->setIcon(
		QIcon::fromTheme(QStringLiteral("pathshape")));
	m_sizePressureButton->setToolTip(tr("Pressure sensitivity"));
	m_sizePressureButton->setStatusTip(m_sizePressureButton->toolTip());
	grid->addWidget(m_sizePressureButton, row, 2);
	connect(
		m_sizePressureButton, &widgets::GroupedToolButton::clicked, this,
		&LiquifySettings::pushSettings);

	++row;

	m_amountSlider = new widgets::NoScrollKisSliderSpinBox;
	m_amountSlider->setRange(props::amount.min, props::amount.max);
	m_amountSlider->setPrefix(tr("Strength: "));
	m_amountSlider->setSuffix(strings::percent());
	m_amountSlider->setBlockUpdateSignalOnDrag(true);
	grid->addWidget(m_amountSlider, row, 0, 1, 2);
	connect(
		m_amountSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LiquifySettings::pushSettings);

	m_amountPressureButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_amountPressureButton->setIcon(
		QIcon::fromTheme(QStringLiteral("pathshape")));
	m_amountPressureButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_amountPressureButton->setCheckable(true);
	m_amountPressureButton->setToolTip(tr("Pressure sensitivity"));
	m_amountPressureButton->setStatusTip(m_amountPressureButton->toolTip());
	grid->addWidget(m_amountPressureButton, row, 2);
	connect(
		m_amountPressureButton, &widgets::GroupedToolButton::clicked, this,
		&LiquifySettings::pushSettings);

	++row;

	m_hardnessSlider = new widgets::NoScrollKisSliderSpinBox;
	m_hardnessSlider->setRange(props::hardness.min, props::hardness.max);
	m_hardnessSlider->setPrefix(tr("Hardness: "));
	m_hardnessSlider->setSuffix(strings::percent());
	m_hardnessSlider->setBlockUpdateSignalOnDrag(true);
	grid->addWidget(m_hardnessSlider, row, 0, 1, 2);
	connect(
		m_hardnessSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LiquifySettings::pushSettings);

	m_hardnessPressureButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_hardnessPressureButton->setIcon(
		QIcon::fromTheme(QStringLiteral("pathshape")));
	m_hardnessPressureButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_hardnessPressureButton->setCheckable(true);
	m_hardnessPressureButton->setToolTip(tr("Pressure sensitivity"));
	m_hardnessPressureButton->setStatusTip(m_hardnessPressureButton->toolTip());
	grid->addWidget(m_hardnessPressureButton, row, 2);
	connect(
		m_hardnessPressureButton, &widgets::GroupedToolButton::clicked, this,
		&LiquifySettings::pushSettings);

	++row;

	m_spacingSlider = new widgets::NoScrollKisSliderSpinBox;
	m_spacingSlider->setRange(props::spacing.min, props::spacing.max);
	m_spacingSlider->setPrefix(tr("Spacing: "));
	m_spacingSlider->setSuffix(strings::percent());
	m_spacingSlider->setBlockUpdateSignalOnDrag(true);
	grid->addWidget(m_spacingSlider, row, 0, 1, 2);
	connect(
		m_spacingSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LiquifySettings::pushSettings);

	++row;

	widgets::GroupedToolButton *nearestButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	nearestButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	nearestButton->setText(
		QCoreApplication::translate("tools::TransformSettings", "Nearest"));
	nearestButton->setStatusTip(
		QCoreApplication::translate(
			"tools::TransformSettings",
			"Nearest-neighbor interpolation, no smoothing"));
	nearestButton->setToolTip(nearestButton->statusTip());
	nearestButton->setCheckable(true);

	widgets::GroupedToolButton *bilinearButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	bilinearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	bilinearButton->setText(
		QCoreApplication::translate("tools::TransformSettings", "Bilinear"));
	bilinearButton->setStatusTip(
		QCoreApplication::translate(
			"tools::TransformSettings", "Smoothed interpolation"));
	bilinearButton->setToolTip(bilinearButton->statusTip());
	bilinearButton->setCheckable(true);
	bilinearButton->setChecked(true);

	QLabel *interpolationLabel = new QLabel(
		QCoreApplication::translate("tools::TransformSettings", "Sampling:"));

	QHBoxLayout *interpolationLayout = new QHBoxLayout;
	interpolationLayout->setContentsMargins(0, 0, 0, 0);
	interpolationLayout->setSpacing(0);
	interpolationLayout->addWidget(nearestButton);
	interpolationLayout->addWidget(bilinearButton);

	grid->addWidget(interpolationLabel, row, 0);
	grid->addLayout(interpolationLayout, row, 1, 1, 2);

	m_interpolationGroup = new QButtonGroup(this);
	m_interpolationGroup->addButton(
		nearestButton, DP_MSG_TRANSFORM_REGION_MODE_NEAREST);
	m_interpolationGroup->addButton(
		bilinearButton, DP_MSG_TRANSFORM_REGION_MODE_BILINEAR);
	connect(
		m_interpolationGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&LiquifySettings::pushSettings);
	++row;

	m_applyButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogApplyButton),
		QCoreApplication::translate("QPlatformTheme", "Apply"));
	m_applyButton->setStatusTip(tr("Apply the liquified transform"));
	m_applyButton->setToolTip(m_applyButton->statusTip());
	m_applyButton->setEnabled(false);
	connect(
		m_applyButton, &QPushButton::clicked, controller(),
		&ToolController::finishMultipartDrawing);

	m_cancelButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogCancelButton),
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	m_cancelButton->setStatusTip(tr("Discard the liquified transform"));
	m_cancelButton->setToolTip(m_cancelButton->statusTip());
	m_cancelButton->setEnabled(false);
	connect(
		m_cancelButton, &QPushButton::clicked, controller(),
		&ToolController::cancelMultipartDrawing);

	QHBoxLayout *applyCancelLayout = new QHBoxLayout;
	applyCancelLayout->setContentsMargins(0, 0, 0, 0);
	applyCancelLayout->addWidget(m_applyButton);
	applyCancelLayout->addWidget(m_cancelButton);
	grid->addLayout(applyCancelLayout, row, 0, 1, 3, Qt::AlignTop);

	grid->setColumnStretch(1, 1);
	grid->setRowStretch(row, 1);

	loadDefaultSettings();
	return m_stack;
}

void LiquifySettings::loadDefaultSettings()
{
	utils::setSpinnerValueSignalsBlocked(
		m_sizeSlider, props::size.defaultValue);
	utils::setButtonCheckedSignalsBlocked(
		m_sizePressureButton, props::sizeDynamic.defaultValue != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_amountSlider, props::amount.defaultValue);
	utils::setButtonCheckedSignalsBlocked(
		m_amountPressureButton, props::amountDynamic.defaultValue != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_hardnessSlider, props::hardness.defaultValue);
	utils::setButtonCheckedSignalsBlocked(
		m_hardnessPressureButton, props::hardnessDynamic.defaultValue != 0);
	utils::setSpinnerValueSignalsBlocked(
		m_spacingSlider, props::spacing.defaultValue);
	utils::clickButtonGroupSignalsBlocked(
		m_interpolationGroup, DP_MSG_TRANSFORM_REGION_MODE_BILINEAR);
	pushSettings();
}

void LiquifySettings::setModel(canvas::CanvasModel *canvas)
{
	if(canvas) {
		canvas::TransformModel *transform = canvas->transform();
		connect(
			transform, &canvas::TransformModel::transformChanged, this,
			&LiquifySettings::updateEnabled, Qt::QueuedConnection);
	}
	updateEnabledFrom(canvas);
}

void LiquifySettings::updateEnabled()
{
	updateEnabledFrom(controller()->model());
}

void LiquifySettings::updateEnabledFrom(canvas::CanvasModel *canvas)
{
	if(isUiInitialized()) {
		canvas::TransformModel *transform =
			canvas ? canvas->transform() : nullptr;

		bool haveLiquify = transform && transform->isLiquifyActive();
		m_applyButton->setEnabled(haveLiquify);
		m_cancelButton->setEnabled(haveLiquify);

		if(m_hadLiquify && !haveLiquify) {
			setOperation(int(Operation::Move));
		}
		m_hadLiquify = haveLiquify;
	}
}

int LiquifySettings::selectedOperation() const
{
	if(m_liquifyOperations) {
		return m_liquifyOperations->actions().indexOf(
			m_liquifyOperations->checkedAction());
	} else {
		return int(Operation::Move);
	}
}

void LiquifySettings::setOperation(int operation)
{
	QList<QAction *> operationActions = m_liquifyOperations->actions();
	if(operation >= 0 && operation <= operationActions.size()) {
		operationActions[operation]->trigger();
	}
}

}

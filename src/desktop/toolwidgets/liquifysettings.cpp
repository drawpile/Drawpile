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

LiquifySettings::LiquifySettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
	connect(
		ctrl, &ToolController::modelChanged, this, &LiquifySettings::setModel);
}

int LiquifySettings::getSize() const
{
	return m_lastEmittedSize;
}

void LiquifySettings::setFeatureAccess(bool featureAccess) {}

ToolProperties LiquifySettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	return cfg;
}

void LiquifySettings::restoreToolSettings(const ToolProperties &cfg) {}

void LiquifySettings::pushSettings()
{
	controller()->setLiquifyParams(m_interpolationGroup->checkedId());

	LiquifyTool::Properties props;
	// TODO are these ratios right? Compare with Krita.
	props.size = qreal(m_sizeSlider->value());
	props.amount = qreal(m_amountSlider->value()) / 100.0;
	props.spacing = qreal(m_spacingSlider->value()) / 100.0;
	props.sizePressure = m_sizePressureButton->isChecked();
	props.amountPressure = m_amountPressureButton->isChecked();

	int operation = m_operationGroup->checkedId();
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
	case int(Operation::Erase):
		props.operation = LiquifyTool::Operation::Undo;
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

	widgets::GroupedToolButton *moveButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	moveButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	moveButton->setCheckable(true);
	moveButton->setChecked(true);
	moveButton->setStatusTip(tr("Move"));
	moveButton->setToolTip(moveButton->statusTip());
	moveButton->setIcon(QIcon::fromTheme("drawpile_liquify_move"));
	headerLayout->addWidget(moveButton, 1);

	widgets::GroupedToolButton *rotateLeftButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	rotateLeftButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	rotateLeftButton->setCheckable(true);
	rotateLeftButton->setChecked(false);
	rotateLeftButton->setStatusTip(tr("Rotate counter-clockwise"));
	rotateLeftButton->setToolTip(rotateLeftButton->statusTip());
	rotateLeftButton->setIcon(QIcon::fromTheme("drawpile_liquify_rotate_left"));
	headerLayout->addWidget(rotateLeftButton, 1);

	widgets::GroupedToolButton *rotateRightButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	rotateRightButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	rotateRightButton->setCheckable(true);
	rotateRightButton->setChecked(false);
	rotateRightButton->setStatusTip(tr("Rotate clockwise"));
	rotateRightButton->setToolTip(rotateRightButton->statusTip());
	rotateRightButton->setIcon(
		QIcon::fromTheme("drawpile_liquify_rotate_right"));
	headerLayout->addWidget(rotateRightButton, 1);

	widgets::GroupedToolButton *expandButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	expandButton->setCheckable(true);
	expandButton->setChecked(false);
	expandButton->setStatusTip(tr("Expand"));
	expandButton->setToolTip(expandButton->statusTip());
	expandButton->setIcon(QIcon::fromTheme("drawpile_liquify_expand"));
	headerLayout->addWidget(expandButton, 1);

	widgets::GroupedToolButton *shrinkButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	shrinkButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	shrinkButton->setCheckable(true);
	shrinkButton->setChecked(false);
	shrinkButton->setStatusTip(tr("Shrink"));
	shrinkButton->setToolTip(shrinkButton->statusTip());
	shrinkButton->setIcon(QIcon::fromTheme("drawpile_liquify_shrink"));
	headerLayout->addWidget(shrinkButton, 1);

	widgets::GroupedToolButton *eraseButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	eraseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	eraseButton->setCheckable(true);
	eraseButton->setChecked(false);
	eraseButton->setStatusTip(tr("Erase"));
	eraseButton->setToolTip(eraseButton->statusTip());
	eraseButton->setIcon(QIcon::fromTheme("drawpile_liquify_erase"));
	headerLayout->addWidget(eraseButton, 1);

	headerLayout->addStretch(1);

	m_operationGroup = new QButtonGroup(this);
	m_operationGroup->addButton(moveButton, int(Operation::Move));
	m_operationGroup->addButton(rotateLeftButton, int(Operation::RotateLeft));
	m_operationGroup->addButton(rotateRightButton, int(Operation::RotateRight));
	m_operationGroup->addButton(expandButton, int(Operation::Expand));
	m_operationGroup->addButton(shrinkButton, int(Operation::Shrink));
	m_operationGroup->addButton(eraseButton, int(Operation::Erase));
	connect(
		m_operationGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&LiquifySettings::pushSettings);

	m_stack = new QStackedWidget(parent);

	QWidget *widget = new QWidget;
	QGridLayout *grid = new QGridLayout(widget);
	grid->setContentsMargins(3, 3, 3, 3);
	grid->setSpacing(3);
	m_stack->addWidget(widget);

	int row = 0;

	m_sizeSlider = new widgets::NoScrollKisSliderSpinBox;
	m_sizeSlider->setRange(5, 1000);
	m_sizeSlider->setExponentRatio(3.0);
	m_sizeSlider->setPrefix(tr("Size: "));
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
	m_amountSlider->setRange(1, 100);
	m_amountSlider->setPrefix(tr("Strength: "));
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

	m_spacingSlider = new widgets::NoScrollKisSliderSpinBox;
	m_spacingSlider->setRange(1, 300);
	m_spacingSlider->setPrefix(tr("Spacing: "));
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
	utils::setSpinnerValueSignalsBlocked(m_sizeSlider, 60);
	utils::setButtonCheckedSignalsBlocked(m_sizePressureButton, false);
	utils::setSpinnerValueSignalsBlocked(m_amountSlider, 5);
	utils::setButtonCheckedSignalsBlocked(m_amountPressureButton, false);
	utils::setSpinnerValueSignalsBlocked(m_spacingSlider, 20);
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
	if(m_applyButton) {
		canvas::TransformModel *transform =
			canvas ? canvas->transform() : nullptr;

		bool haveLiquify = transform && transform->isLiquifyActive();
		m_applyButton->setEnabled(haveLiquify);
		m_cancelButton->setEnabled(haveLiquify);
	}
}

}

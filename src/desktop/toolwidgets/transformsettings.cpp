// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/transformsettings.h"
#include "desktop/view/canvaswrapper.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/transform.h"
#include <QAction>
#include <QButtonGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> interpolation{
	QStringLiteral("interpolation"), 1, 0, 1};
}

TransformSettings::TransformSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
	connect(
		ctrl, &ToolController::modelChanged, this,
		&TransformSettings::setModel);
	connect(
		ctrl, &ToolController::transformToolStateChanged, this,
		&TransformSettings::updateHandles);
}

void TransformSettings::setCompatibilityMode(bool compatibilityMode)
{
	for(QAbstractButton *button : m_interpolationGroup->buttons()) {
		button->setDisabled(compatibilityMode);
	}
}

void TransformSettings::setActions(
	QAction *mirror, QAction *flip, QAction *rotatecw, QAction *rotateccw,
	QAction *shrinktoview, QAction *stamp)
{
	m_mirrorButton->setDefaultAction(mirror);
	m_flipButton->setDefaultAction(flip);
	m_rotateCwButton->setDefaultAction(rotatecw);
	m_rotateCcwButton->setDefaultAction(rotateccw);
	m_shrinkToViewButton->setDefaultAction(shrinktoview);
	connect(mirror, &QAction::triggered, this, &TransformSettings::mirror);
	connect(flip, &QAction::triggered, this, &TransformSettings::flip);
	connect(rotatecw, &QAction::triggered, this, &TransformSettings::rotateCw);
	connect(
		rotateccw, &QAction::triggered, this, &TransformSettings::rotateCcw);
	connect(
		shrinktoview, &QAction::triggered, this,
		&TransformSettings::shrinkToView);
	connect(stamp, &QAction::triggered, this, &TransformSettings::stamp);
}

ToolProperties TransformSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::interpolation, m_interpolationGroup->checkedId());
	return cfg;
}

void TransformSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QAbstractButton *button =
		m_interpolationGroup->button(cfg.value(props::interpolation));
	if(button) {
		button->setChecked(true);
	}
}

void TransformSettings::pushSettings()
{
	controller()->setTransformInterpolation(m_interpolationGroup->checkedId());
	tool()->setMode(tools::TransformTool::Mode(m_handlesGroup->checkedId()));
}

QWidget *TransformSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch();

	m_mirrorButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	headerLayout->addWidget(m_mirrorButton);

	m_flipButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_flipButton);

	m_rotateCwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_rotateCwButton);

	m_rotateCcwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_rotateCcwButton);

	m_shrinkToViewButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	headerLayout->addWidget(m_shrinkToViewButton);

	headerLayout->addStretch();

	QWidget *widget = new QWidget(parent);
	QFormLayout *layout = new QFormLayout(widget);

	widgets::GroupedToolButton *nearestButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	nearestButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	nearestButton->setText(tr("Nearest"));
	nearestButton->setStatusTip(
		tr("Nearest-neighbor interpolation, no smoothing"));
	nearestButton->setToolTip(nearestButton->statusTip());
	nearestButton->setCheckable(true);

	widgets::GroupedToolButton *bilinearButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	bilinearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	bilinearButton->setText(tr("Bilinear"));
	bilinearButton->setStatusTip(tr("Smoothed interpolation"));
	bilinearButton->setToolTip(bilinearButton->statusTip());
	bilinearButton->setCheckable(true);
	bilinearButton->setChecked(true);

	QHBoxLayout *interpolationLayout = new QHBoxLayout;
	interpolationLayout->setContentsMargins(0, 0, 0, 0);
	interpolationLayout->setSpacing(0);
	interpolationLayout->addWidget(nearestButton);
	interpolationLayout->addWidget(bilinearButton);
	//: Refers to the transform interpolation, but "interpolation" is
	//: uncomfortably long in English, it causes a lot of blank space and the
	//: tool settings to get wider. Try to take that into account if possible.
	QLabel *interpolationLabel = new QLabel(tr("Sampling:"));
	layout->addRow(interpolationLabel, interpolationLayout);

	m_interpolationGroup = new QButtonGroup(this);
	m_interpolationGroup->addButton(
		nearestButton, DP_MSG_TRANSFORM_REGION_MODE_NEAREST);
	m_interpolationGroup->addButton(
		bilinearButton, DP_MSG_TRANSFORM_REGION_MODE_BILINEAR);
	connect(
		m_interpolationGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&TransformSettings::pushSettings);

	m_scaleButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_scaleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_scaleButton->setText(tr("Scale"));
	m_scaleButton->setStatusTip(
		tr("Shows scaling handles on corners and edges"));
	m_scaleButton->setToolTip(m_scaleButton->statusTip());
	m_scaleButton->setCheckable(true);
	m_scaleButton->setChecked(true);

	m_distortButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_distortButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_distortButton->setText(tr("Distort"));
	m_distortButton->setStatusTip(
		tr("Shows distortion handles on corners and edges"));
	m_distortButton->setToolTip(m_distortButton->statusTip());
	m_distortButton->setCheckable(true);

	QHBoxLayout *handlesLayout = new QHBoxLayout;
	handlesLayout->setContentsMargins(0, 0, 0, 0);
	handlesLayout->setSpacing(0);
	handlesLayout->addWidget(m_scaleButton);
	handlesLayout->addWidget(m_distortButton);
	//: Refers to the handles on the corners and edges of a transform. Which are
	//: either arrows to scale the transformation or squares to distort it.
	layout->addRow(tr("Handles:"), handlesLayout);

	m_handlesGroup = new QButtonGroup(this);
	m_handlesGroup->addButton(
		m_scaleButton, int(tools::TransformTool::Mode::Scale));
	m_handlesGroup->addButton(
		m_distortButton, int(tools::TransformTool::Mode::Distort));
	connect(
		m_handlesGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&TransformSettings::pushSettings);

	m_applyButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogApplyButton),
		tr("Apply"));
	m_applyButton->setStatusTip(tr("Apply the transformation"));
	m_applyButton->setToolTip(m_applyButton->statusTip());
	m_applyButton->setEnabled(false);
	connect(
		m_applyButton, &QPushButton::clicked, controller(),
		&ToolController::finishMultipartDrawing);

	m_cancelButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogCancelButton),
		tr("Cancel"));
	m_cancelButton->setStatusTip(tr("Cancel the transformation"));
	m_cancelButton->setToolTip(m_cancelButton->statusTip());
	m_cancelButton->setEnabled(false);
	connect(
		m_cancelButton, &QPushButton::clicked, controller(),
		&ToolController::cancelMultipartDrawing);

	QHBoxLayout *applyCancelLayout = new QHBoxLayout;
	applyCancelLayout->setContentsMargins(0, 0, 0, 0);
	applyCancelLayout->addWidget(m_applyButton);
	applyCancelLayout->addWidget(m_cancelButton);
	layout->addRow(applyCancelLayout);

	return widget;
}

void TransformSettings::mirror()
{
	tool()->mirror();
}

void TransformSettings::flip()
{
	tool()->flip();
}

void TransformSettings::rotateCw()
{
	tool()->rotateCw();
}

void TransformSettings::rotateCcw()
{
	tool()->rotateCcw();
}

void TransformSettings::shrinkToView()
{
	Q_ASSERT(m_canvasView);
	if(m_canvasView) {
		tool()->shrinkToView(m_canvasView->screenRect());
	}
}

void TransformSettings::stamp()
{
	tool()->stamp();
}

void TransformSettings::setModel(canvas::CanvasModel *canvas)
{
	if(canvas) {
		connect(
			canvas->transform(), &canvas::TransformModel::transformChanged,
			this, &TransformSettings::updateEnabled);
	}
	updateEnabledFrom(canvas);
}

void TransformSettings::updateEnabled()
{
	updateEnabledFrom(controller()->model());
}

void TransformSettings::updateEnabledFrom(canvas::CanvasModel *canvas)
{
	if(m_applyButton) {
		canvas::TransformModel *transform =
			canvas ? canvas->transform() : nullptr;
		bool haveTransform = transform && transform->isActive();
		bool canApplyTransform = haveTransform && transform->isDstQuadValid();
		m_scaleButton->setEnabled(haveTransform);
		m_distortButton->setEnabled(haveTransform);
		m_applyButton->setEnabled(canApplyTransform);
		m_cancelButton->setEnabled(haveTransform);
	}
}

void TransformSettings::updateHandles(int mode)
{
	QAbstractButton *button = m_handlesGroup->button(mode);
	if(button) {
		QSignalBlocker blocker(m_handlesGroup);
		button->setChecked(true);
	}
}

TransformTool *TransformSettings::tool()
{
	return static_cast<tools::TransformTool *>(
		controller()->getTool(Tool::TRANSFORM));
}

}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/transformsettings.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/view/canvaswrapper.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/transform.h"
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStandardItemModel>
#include <QStyle>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> accuratepreview{
	QStringLiteral("accuratepreview"), true};
static const ToolProperties::RangedValue<int> interpolation{
	QStringLiteral("interpolation"), 1, 0, 1};
}

TransformSettings::TransformSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, m_previousMode(DP_BLEND_MODE_NORMAL)
	, m_previousEraseMode(DP_BLEND_MODE_ERASE)
{
	connect(
		ctrl, &ToolController::modelChanged, this,
		&TransformSettings::setModel);
	connect(
		ctrl, &ToolController::transformToolStateChanged, this,
		&TransformSettings::updateHandles);
	connect(
		this, &TransformSettings::blendModeChanged, this,
		&TransformSettings::pushSettings);
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

void TransformSettings::setCompatibilityMode(bool compatibilityMode)
{
	initBlendModeOptions(compatibilityMode);
	m_alphaPreserveButton->setEnabled(!compatibilityMode);
}

ToolProperties TransformSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::accuratepreview, m_previewGroup->checkedId() != 0);
	cfg.setValue(props::interpolation, m_interpolationGroup->checkedId());
	return cfg;
}

void TransformSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QAbstractButton *previewButton =
		m_previewGroup->button(cfg.value(props::accuratepreview) ? 1 : 0);
	if(previewButton) {
		previewButton->setChecked(true);
	}

	QAbstractButton *interpolationButton =
		m_interpolationGroup->button(cfg.value(props::interpolation));
	if(interpolationButton) {
		interpolationButton->setChecked(true);
	}
}

void TransformSettings::pushSettings()
{
	controller()->setTransformParams(
		m_previewGroup->checkedId() != 0, m_interpolationGroup->checkedId());
	TransformTool *tt = tool();
	if(tt->mode() != tools::TransformTool::Mode::Move) {
		tt->setMode(tools::TransformTool::Mode(m_handlesGroup->checkedId()));
	}

	int blendMode = getCurrentBlendMode();
	if(canvas::blendmode::presentsAsEraser(blendMode)) {
		m_previousEraseMode = blendMode;
	} else {
		m_previousMode = blendMode;
	}
}

void TransformSettings::toggleEraserMode()
{
	selectBlendMode(
		canvas::blendmode::presentsAsEraser(getCurrentBlendMode())
			? m_previousMode
			: m_previousEraseMode);
}

void TransformSettings::toggleAlphaPreserve()
{
	m_alphaPreserveButton->click();
}

QWidget *TransformSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch(1);

	m_mirrorButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	headerLayout->addWidget(m_mirrorButton, 3);

	m_flipButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_flipButton, 3);

	m_rotateCwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_rotateCwButton, 3);

	m_rotateCcwButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	headerLayout->addWidget(m_rotateCcwButton, 3);

	m_shrinkToViewButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	headerLayout->addWidget(m_shrinkToViewButton, 3);

	headerLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	QFormLayout *layout = new QFormLayout(widget);

	m_fastButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_fastButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_fastButton->setText(tr("Fast"));
	m_fastButton->setStatusTip(tr("Quick preview not taking into account "
								  "layering, opacity or anything else"));
	m_fastButton->setToolTip(m_fastButton->statusTip());
	m_fastButton->setCheckable(true);

	m_accurateButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_accurateButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_accurateButton->setText(tr("Accurate"));
	m_accurateButton->setStatusTip(
		tr("Preview the transform exactly how will look when applied"));
	m_accurateButton->setToolTip(m_accurateButton->statusTip());
	m_accurateButton->setCheckable(true);
	m_accurateButton->setChecked(true);

	m_pseudoFastButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_pseudoFastButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_pseudoFastButton->setText(m_fastButton->text());
	m_pseudoFastButton->setStatusTip(
		tr("Too many layers selected for accurate preview, only fast preview "
		   "is available."));
	m_pseudoFastButton->setToolTip(m_pseudoFastButton->statusTip());
	m_pseudoFastButton->setCheckable(true);
	m_pseudoFastButton->setChecked(true);
	m_pseudoFastButton->setEnabled(false);
	m_pseudoFastButton->hide();

	QHBoxLayout *previewLayout = new QHBoxLayout;
	previewLayout->setContentsMargins(0, 0, 0, 0);
	previewLayout->setSpacing(0);
	previewLayout->addWidget(m_fastButton);
	previewLayout->addWidget(m_accurateButton);
	previewLayout->addWidget(m_pseudoFastButton);
	QLabel *previewLabel = new QLabel(tr("Preview:"));
	layout->addRow(previewLabel, previewLayout);

	m_previewGroup = new QButtonGroup(this);
	m_previewGroup->addButton(m_fastButton, 0);
	m_previewGroup->addButton(m_accurateButton, 1);
	connect(
		m_previewGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&TransformSettings::pushSettings);

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

	m_showHandlesButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_showHandlesButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_showHandlesButton->setText(tr("Show"));
	m_showHandlesButton->setStatusTip(
		tr("Show transform handles instead of just moving"));
	m_showHandlesButton->setToolTip(m_showHandlesButton->statusTip());
	m_showHandlesButton->hide();
	connect(
		m_showHandlesButton, &widgets::GroupedToolButton::clicked, this,
		&TransformSettings::showHandles);

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
	handlesLayout->addWidget(m_showHandlesButton);
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

	QHBoxLayout *modeLayout = new QHBoxLayout;
	modeLayout->setContentsMargins(0, 0, 0, 0);
	modeLayout->setSpacing(0);
	layout->addRow(tr("Mode:"), modeLayout);
	m_alphaPreserveButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	QIcon alphaLockedUnlockedIcon;
	alphaLockedUnlockedIcon.addFile(
		QStringLiteral("theme:drawpile_alpha_unlocked.svg"), QSize(),
		QIcon::Normal, QIcon::Off);
	alphaLockedUnlockedIcon.addFile(
		QStringLiteral("theme:drawpile_alpha_locked.svg"), QSize(),
		QIcon::Normal, QIcon::On);
	m_alphaPreserveButton->setIcon(alphaLockedUnlockedIcon);
	m_alphaPreserveButton->setToolTip(
		QCoreApplication::translate("BrushDock", "Preserve alpha"));
	m_alphaPreserveButton->setStatusTip(m_alphaPreserveButton->toolTip());
	m_alphaPreserveButton->setCheckable(true);
	modeLayout->addWidget(m_alphaPreserveButton);
	connect(
		m_alphaPreserveButton, &widgets::GroupedToolButton::clicked, this,
		&TransformSettings::updateAlphaPreserve);

	m_blendModeCombo = new QComboBox;
	QSizePolicy blendModeSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	blendModeSizePolicy.setHorizontalStretch(1);
	blendModeSizePolicy.setVerticalStretch(0);
	blendModeSizePolicy.setHeightForWidth(
		m_blendModeCombo->sizePolicy().hasHeightForWidth());
	m_blendModeCombo->setSizePolicy(blendModeSizePolicy);
	m_blendModeCombo->setMinimumSize(QSize(24, 0));
	initBlendModeOptions(false);
	modeLayout->addWidget(m_blendModeCombo);
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &TransformSettings::updateBlendMode);

	QSpacerItem *modeSpacer = new QSpacerItem(
		6, 0, QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Minimum);
	modeLayout->addSpacerItem(modeSpacer);

	m_constrainButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_constrainButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_constrainButton->setIcon(QIcon::fromTheme("zoom-original"));
	m_constrainButton->setToolTip(tr("Constrain"));
	m_constrainButton->setStatusTip(m_constrainButton->toolTip());
	m_constrainButton->setCheckable(true);
	modeLayout->addWidget(m_constrainButton);
	connect(
		m_constrainButton, &widgets::GroupedToolButton::clicked, this,
		&TransformSettings::updateConstrain);

	m_centerButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_centerButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_centerButton->setIcon(QIcon::fromTheme("drawpile_center"));
	m_centerButton->setToolTip(tr("Center"));
	m_centerButton->setStatusTip(m_centerButton->toolTip());
	m_centerButton->setCheckable(true);
	modeLayout->addWidget(m_centerButton);
	connect(
		m_centerButton, &widgets::GroupedToolButton::clicked, this,
		&TransformSettings::updateCenter);

	m_opacitySlider = new KisSliderSpinBox;
	m_opacitySlider->setRange(1, 100);
	m_opacitySlider->setPrefix(tr("Opacity: "));
	m_opacitySlider->setSuffix(tr("%"));
	m_opacitySlider->setValue(100);
	layout->addRow(m_opacitySlider);
	connect(
		m_opacitySlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &TransformSettings::updateOpacity);

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

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindAutomaticAlphaPreserve(
		this, &TransformSettings::setAutomaticAlphaPerserve);

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
		canvas::TransformModel *transform = canvas->transform();
		connect(
			transform, &canvas::TransformModel::transformChanged, this,
			&TransformSettings::updateEnabled, Qt::QueuedConnection);
		connect(
			this, &TransformSettings::blendModeChanged, transform,
			&canvas::TransformModel::setBlendMode);
		connect(
			this, &TransformSettings::opacityChanged, transform,
			&canvas::TransformModel::setOpacity);
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
		m_blendModeCombo->setEnabled(haveTransform);
		m_alphaPreserveButton->setEnabled(haveTransform);
		m_constrainButton->setEnabled(haveTransform);
		m_centerButton->setEnabled(haveTransform);
		m_opacitySlider->setEnabled(haveTransform);
		m_applyButton->setEnabled(canApplyTransform);
		m_cancelButton->setEnabled(haveTransform);

		bool canChangePreview =
			!haveTransform || transform->canPreviewAccurate();
		if(canChangePreview != m_accurateButton->isEnabled()) {
			// Hide the controls first to prevent the dock being forced wider.
			m_accurateButton->hide();
			m_fastButton->hide();
			m_pseudoFastButton->hide();
			m_accurateButton->setEnabled(canChangePreview);
			m_accurateButton->setVisible(canChangePreview);
			m_fastButton->setEnabled(canChangePreview);
			m_fastButton->setVisible(canChangePreview);
			m_pseudoFastButton->setVisible(!canChangePreview);
		}

		if(transform) {
			selectBlendMode(transform->blendMode());
			setOpacity(transform->opacity());
		}

		TransformTool *tt = tool();
		m_constrainButton->setChecked(haveTransform && tt->constrain());
		m_centerButton->setChecked(haveTransform && tt->center());
	}
}

void TransformSettings::updateHandles(int mode)
{
	QAbstractButton *button = m_handlesGroup->button(mode);
	if(button) {
		QSignalBlocker blocker(m_handlesGroup);
		button->setChecked(true);
		if(!m_handleButtonsVisible) {
			m_handleButtonsVisible = true;
			m_showHandlesButton->hide();
			m_scaleButton->show();
			m_distortButton->show();
		}
	} else if(m_handleButtonsVisible) {
		m_handleButtonsVisible = false;
		m_scaleButton->hide();
		m_distortButton->hide();
		m_showHandlesButton->show();
	}
}

void TransformSettings::showHandles()
{
	tool()->setMode(TransformTool::Mode::Scale);
}

void TransformSettings::initBlendModeOptions(bool compatibilityMode)
{
	int blendMode = getCurrentBlendMode();
	{
		QSignalBlocker blocker(m_blendModeCombo);
		m_blendModeCombo->setModel(
			getFillBlendModesFor(m_automaticAlphaPreserve, compatibilityMode));
	}
	selectBlendMode(blendMode);
}

void TransformSettings::updateAlphaPreserve(bool alphaPreserve)
{
	int blendMode = m_blendModeCombo->currentData().toInt();
	canvas::blendmode::adjustAlphaBehavior(blendMode, alphaPreserve);

	int index = searchBlendModeComboIndex(m_blendModeCombo, blendMode);
	if(index != -1) {
		QSignalBlocker blocker(m_blendModeCombo);
		m_blendModeCombo->setCurrentIndex(index);
	}
	emit blendModeChanged(blendMode);
}

void TransformSettings::updateBlendMode(int index)
{
	int blendMode = m_blendModeCombo->itemData(index).toInt();
	if(m_automaticAlphaPreserve) {
		QSignalBlocker blocker(m_alphaPreserveButton);
		m_alphaPreserveButton->setChecked(
			canvas::blendmode::presentsAsAlphaPreserving(blendMode));
	} else {
		canvas::blendmode::adjustAlphaBehavior(
			blendMode, m_alphaPreserveButton->isChecked());
	}
	emit blendModeChanged(blendMode);
}

void TransformSettings::selectBlendMode(int blendMode)
{
	int index = searchBlendModeComboIndex(m_blendModeCombo, blendMode);
	if(index != -1) {
		{
			QSignalBlocker blocker(m_blendModeCombo);
			m_blendModeCombo->setCurrentIndex(index);
		}
		if(m_automaticAlphaPreserve) {
			QSignalBlocker blocker(m_alphaPreserveButton);
			m_alphaPreserveButton->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(blendMode));
		}
		emit blendModeChanged(blendMode);
	}
}

int TransformSettings::getCurrentBlendMode() const
{
	int blendMode = m_blendModeCombo->count() == 0
						? DP_BLEND_MODE_NORMAL
						: m_blendModeCombo->currentData().toInt();
	canvas::blendmode::adjustAlphaBehavior(
		blendMode, m_alphaPreserveButton->isChecked());
	return blendMode;
}

void TransformSettings::setAutomaticAlphaPerserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != m_automaticAlphaPreserve) {
		m_automaticAlphaPreserve = automaticAlphaPreserve;
		canvas::CanvasModel *canvas = controller()->model();
		initBlendModeOptions(canvas && canvas->isCompatibilityMode());
	}
}

void TransformSettings::updateConstrain(bool constrain)
{
	tool()->setConstrain(constrain);
}

void TransformSettings::updateCenter(bool center)
{
	tool()->setCenter(center);
}

void TransformSettings::updateOpacity(int value)
{
	emit opacityChanged(value / 100.0);
}

void TransformSettings::setOpacity(qreal opacity)
{
	m_opacitySlider->setValue(qRound(opacity * 100.0));
}

TransformTool *TransformSettings::tool()
{
	return static_cast<tools::TransformTool *>(
		controller()->getTool(Tool::TRANSFORM));
}

}

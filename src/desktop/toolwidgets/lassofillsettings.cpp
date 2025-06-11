// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/lassofillsettings.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/tools/lassofill.h"
#include "libclient/tools/toolcontroller.h"
#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStyle>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> antialias{
	QStringLiteral("antialias"), false};
static const ToolProperties::RangedValue<int> opacity{
	QStringLiteral("opacity"), 100, 1, 100},
	stabilizer{QStringLiteral("stabilizer"), 0, 0, 1000},
	smoothing{QStringLiteral("smoothing"), 0, 0, 20},
	stabilizationMode{
		QStringLiteral("stabilizationMode"), 0, 0,
		int(brushes::LastStabilizationMode)},
	blendMode{
		QStringLiteral("blendMode"), DP_BLEND_MODE_NORMAL, 0,
		DP_BLEND_MODE_MAX};
}

LassoFillSettings::LassoFillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

void LassoFillSettings::setActions(
	QAction *automaticAlphaPreserve, QAction *maskSelection)
{
	m_headerMenu->addAction(automaticAlphaPreserve);
	m_headerMenu->addAction(maskSelection);
}

void LassoFillSettings::setFeatureAccess(bool featureAccess)
{
	if(!featureAccess && m_featureAccess) {
		controller()->getTool(Tool::LASSOFILL)->cancelMultipart();
	}
	m_featureAccess = featureAccess;
}

void LassoFillSettings::setCompatibilityMode(bool compatibilityMode)
{
	m_blendModeManager->setCompatibilityMode(compatibilityMode);
}

ToolProperties LassoFillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::antialias, m_antiAliasCheckBox->isChecked());
	cfg.setValue(props::opacity, m_opacitySpinner->value());
	cfg.setValue(props::stabilizer, m_stabilizerSpinner->value());
	cfg.setValue(props::smoothing, m_smoothingSpinner->value());
	cfg.setValue(props::stabilizationMode, getCurrentStabilizationMode());
	cfg.setValue(props::blendMode, m_blendModeManager->getCurrentBlendMode());
	return cfg;
}

void LassoFillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_antiAliasCheckBox->setChecked(cfg.value(props::antialias));
	m_opacitySpinner->setValue(cfg.value(props::opacity));
	m_stabilizerSpinner->setValue(cfg.value(props::stabilizer));
	m_smoothingSpinner->setValue(cfg.value(props::smoothing));
	if(cfg.value(props::stabilizationMode) == int(brushes::Smoothing)) {
		m_smoothingAction->setChecked(true);
		m_stabilizerSpinner->hide();
		m_smoothingSpinner->show();
	} else {
		m_stabilizerAction->setChecked(true);
		m_smoothingSpinner->hide();
		m_stabilizerSpinner->show();
	}
	m_blendModeManager->selectBlendMode(cfg.value(props::blendMode));
}

void LassoFillSettings::toggleEraserMode()
{
	m_blendModeManager->toggleEraserMode();
}

void LassoFillSettings::toggleAlphaPreserve()
{
	m_blendModeManager->toggleAlphaPreserve();
}

void LassoFillSettings::toggleBlendMode(int blendMode)
{
	m_blendModeManager->toggleBlendMode(blendMode);
}

void LassoFillSettings::pushSettings()
{
	ToolController *ctrl = controller();
	LassoFillTool *tool =
		static_cast<LassoFillTool *>(ctrl->getTool(Tool::LASSOFILL));
	tool->setParams(
		m_opacitySpinner->value() / 100.0f, getCurrentStabilizationMode(),
		m_stabilizerSpinner->value(), m_smoothingSpinner->value(),
		m_blendModeManager->getCurrentBlendMode(),
		m_antiAliasCheckBox->isChecked());
}

QWidget *LassoFillSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);

	widgets::GroupedToolButton *headerMenuButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	headerLayout->addWidget(headerMenuButton);
	headerMenuButton->setIcon(QIcon::fromTheme("application-menu"));
	headerMenuButton->setPopupMode(QToolButton::InstantPopup);

	m_headerMenu = new QMenu(headerMenuButton);
	headerMenuButton->setMenu(m_headerMenu);

	headerLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	QFormLayout *layout = new QFormLayout(widget);
	layout->setContentsMargins(3, 3, 3, 3);
	layout->setSpacing(3);

	m_opacitySpinner = new KisSliderSpinBox;
	m_opacitySpinner->setRange(1, 100);
	m_opacitySpinner->setBlockUpdateSignalOnDrag(true);
	m_opacitySpinner->setPrefix(
		QCoreApplication::translate("BrushDock", "Opacity: ", nullptr));
	m_opacitySpinner->setSuffix(
		QCoreApplication::translate("BrushDock", "%", nullptr));
	layout->addRow(m_opacitySpinner);
	connect(
		m_opacitySpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LassoFillSettings::pushSettings);

	m_stabilizerSpinner = new KisSliderSpinBox;
	m_stabilizerSpinner->setRange(0, 1000);
	m_stabilizerSpinner->setExponentRatio(3.0);
	m_stabilizerSpinner->setBlockUpdateSignalOnDrag(true);
	m_stabilizerSpinner->setPrefix(
		QCoreApplication::translate("BrushDock", "Stabilizer: ", nullptr));
	m_stabilizerSpinner->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	connect(
		m_stabilizerSpinner,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&LassoFillSettings::pushSettings);

	m_smoothingSpinner = new KisSliderSpinBox;
	m_smoothingSpinner->setRange(0, 20);
	m_smoothingSpinner->setBlockUpdateSignalOnDrag(true);
	m_smoothingSpinner->setPrefix(
		QCoreApplication::translate("BrushDock", "Smoothing: ", nullptr));
	m_smoothingSpinner->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_smoothingSpinner->hide();
	connect(
		m_smoothingSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LassoFillSettings::pushSettings);

	m_stabilizerButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_stabilizerButton->setIcon(QIcon::fromTheme("application-menu"));
	m_stabilizerButton->setPopupMode(QToolButton::InstantPopup);
	m_stabilizerButton->setStatusTip(tr("Stabilization mode"));
	m_stabilizerButton->setToolTip(m_stabilizerButton->statusTip());

	QMenu *stabilizerMenu = new QMenu(m_stabilizerButton);
	m_stabilizerButton->setMenu(stabilizerMenu);

	m_stabilizationModeGroup = new QActionGroup(stabilizerMenu);
	m_stabilizerAction = stabilizerMenu->addAction(QCoreApplication::translate(
		"tools::BrushSettings", "Time-Based Stabilizer", nullptr));
	m_smoothingAction = stabilizerMenu->addAction(QCoreApplication::translate(
		"tools::BrushSettings", "Average Smoothing", nullptr));
	m_stabilizerAction->setStatusTip(QCoreApplication::translate(
		"tools::BrushSettings",
		"Slows down the stroke and stabilizes it over time. Can produce very "
		"smooth results, but may feel sluggish.",
		nullptr));
	m_smoothingAction->setStatusTip(QCoreApplication::translate(
		"tools::BrushSettings",
		"Simply averages inputs to get a smoother result. Faster than the "
		"time-based stabilizer, but not as smooth.",
		nullptr));
	m_stabilizerAction->setCheckable(true);
	m_smoothingAction->setCheckable(true);
	m_stabilizationModeGroup->addAction(m_stabilizerAction);
	m_stabilizationModeGroup->addAction(m_smoothingAction);
	m_stabilizerAction->setChecked(true);
	connect(
		m_stabilizationModeGroup, &QActionGroup::triggered, this,
		&LassoFillSettings::updateStabilizationMode);

	QHBoxLayout *stabilizerLayout = new QHBoxLayout;
	stabilizerLayout->addWidget(m_stabilizerSpinner, 1);
	stabilizerLayout->addWidget(m_smoothingSpinner, 1);
	stabilizerLayout->addWidget(m_stabilizerButton);
	layout->addRow(stabilizerLayout);

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

	m_blendModeCombo = new QComboBox;
	QSizePolicy blendModeSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	blendModeSizePolicy.setHorizontalStretch(1);
	blendModeSizePolicy.setVerticalStretch(0);
	blendModeSizePolicy.setHeightForWidth(
		m_blendModeCombo->sizePolicy().hasHeightForWidth());
	m_blendModeCombo->setSizePolicy(blendModeSizePolicy);
	m_blendModeCombo->setMinimumSize(QSize(24, 0));

	QHBoxLayout *modeLayout = new QHBoxLayout;
	modeLayout->setContentsMargins(0, 0, 0, 0);
	modeLayout->setSpacing(0);
	modeLayout->addWidget(m_alphaPreserveButton);
	modeLayout->addWidget(m_blendModeCombo);
	layout->addRow(tr("Mode:"), modeLayout);

	m_antiAliasCheckBox = new QCheckBox(tr("Anti-aliasing"));
	m_antiAliasCheckBox->setStatusTip(tr("Smoothe out fill edges"));
	m_antiAliasCheckBox->setToolTip(m_antiAliasCheckBox->statusTip());
	connect(
		m_antiAliasCheckBox, &QCheckBox::clicked, this,
		&LassoFillSettings::pushSettings);
	layout->addRow(m_antiAliasCheckBox);

	m_applyButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogApplyButton),
		tr("Apply"));
	m_applyButton->setStatusTip(tr("Apply the current fill"));
	m_applyButton->setToolTip(m_applyButton->statusTip());
	m_applyButton->setEnabled(false);
	connect(
		m_applyButton, &QPushButton::clicked, controller(),
		&ToolController::finishMultipartDrawing);

	m_cancelButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogCancelButton),
		tr("Cancel"));
	m_cancelButton->setStatusTip(tr("Discard the current fill"));
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

	connect(
		controller(), &ToolController::lassoFillStateChanged, this,
		&LassoFillSettings::setButtonState);
	setButtonState(false);

	m_blendModeManager = BlendModeManager::initFill(
		m_blendModeCombo, m_alphaPreserveButton, this);
	dpApp().settings().bindAutomaticAlphaPreserve(
		m_blendModeManager, &BlendModeManager::setAutomaticAlphaPerserve);
	connect(
		m_blendModeManager, &BlendModeManager::blendModeChanged, this,
		&LassoFillSettings::pushSettings);

	return widget;
}

void LassoFillSettings::updateStabilizationMode(QAction *action)
{
	if(action == m_smoothingAction) {
		m_stabilizerSpinner->hide();
		m_smoothingSpinner->show();
	} else {
		m_smoothingSpinner->hide();
		m_stabilizerSpinner->show();
	}
	pushSettings();
}

int LassoFillSettings::getCurrentStabilizationMode() const
{
	if(m_smoothingAction->isChecked()) {
		return int(brushes::Smoothing);
	} else {
		return int(brushes::Stabilizer);
	}
}

void LassoFillSettings::setButtonState(bool pending)
{
	m_applyButton->setEnabled(pending);
	m_cancelButton->setEnabled(pending);
}

}

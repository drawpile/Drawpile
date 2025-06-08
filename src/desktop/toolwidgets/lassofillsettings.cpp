// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/lassofillsettings.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/tools/lassofill.h"
#include "libclient/tools/toolcontroller.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
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
	blendMode{
		QStringLiteral("blendMode"), DP_BLEND_MODE_NORMAL, 0,
		DP_BLEND_MODE_MAX};
}

LassoFillSettings::LassoFillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, m_previousMode(DP_BLEND_MODE_NORMAL)
	, m_previousEraseMode(DP_BLEND_MODE_ERASE)
{
}

void LassoFillSettings::setFeatureAccess(bool featureAccess)
{
	if(!featureAccess && m_featureAccess) {
		controller()->getTool(Tool::LASSOFILL)->cancelMultipart();
	}
	m_featureAccess = featureAccess;
}

ToolProperties LassoFillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::antialias, m_antiAliasCheckBox->isChecked());
	cfg.setValue(props::opacity, m_opacitySpinner->value());
	cfg.setValue(props::stabilizer, m_stabilizerSpinner->value());
	cfg.setValue(props::blendMode, m_blendModeCombo->currentData().toInt());
	return cfg;
}

void LassoFillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_antiAliasCheckBox->setChecked(cfg.value(props::antialias));
	m_opacitySpinner->setValue(cfg.value(props::opacity));
	m_stabilizerSpinner->setValue(cfg.value(props::stabilizer));
	selectBlendMode(cfg.value(props::blendMode));
}

void LassoFillSettings::toggleEraserMode()
{
	selectBlendMode(
		canvas::blendmode::presentsAsEraser(getCurrentBlendMode())
			? m_previousMode
			: m_previousEraseMode);
}

void LassoFillSettings::toggleAlphaPreserve()
{
	m_alphaPreserveButton->click();
}

void LassoFillSettings::pushSettings()
{
	ToolController *ctrl = controller();
	LassoFillTool *tool =
		static_cast<LassoFillTool *>(ctrl->getTool(Tool::LASSOFILL));
	tool->setParams(
		m_opacitySpinner->value() / 100.0f, m_stabilizerSpinner->value(),
		getCurrentBlendMode(), m_antiAliasCheckBox->isChecked());
}

QWidget *LassoFillSettings::createUiWidget(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	QFormLayout *layout = new QFormLayout(widget);
	layout->setContentsMargins(3, 3, 3, 3);
	layout->setSpacing(3);

	m_opacitySpinner = new KisSliderSpinBox;
	m_opacitySpinner->setRange(1, 100);
	m_opacitySpinner->setBlockUpdateSignalOnDrag(true);
	m_opacitySpinner->setPrefix(tr("Opacity: "));
	m_opacitySpinner->setSuffix(tr("%"));
	layout->addRow(m_opacitySpinner);
	connect(
		m_opacitySpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &LassoFillSettings::pushSettings);

	m_stabilizerSpinner = new KisSliderSpinBox;
	m_stabilizerSpinner->setRange(0, 1000);
	m_stabilizerSpinner->setExponentRatio(3.0);
	m_stabilizerSpinner->setBlockUpdateSignalOnDrag(true);
	m_stabilizerSpinner->setPrefix(tr("Stabilizer: "));
	layout->addRow(m_stabilizerSpinner);
	connect(
		m_stabilizerSpinner,
		QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&LassoFillSettings::pushSettings);

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
	connect(
		m_alphaPreserveButton, &widgets::GroupedToolButton::clicked, this,
		&LassoFillSettings::updateAlphaPreserve);

	m_blendModeCombo = new QComboBox;
	QSizePolicy blendModeSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	blendModeSizePolicy.setHorizontalStretch(1);
	blendModeSizePolicy.setVerticalStretch(0);
	blendModeSizePolicy.setHeightForWidth(
		m_blendModeCombo->sizePolicy().hasHeightForWidth());
	m_blendModeCombo->setSizePolicy(blendModeSizePolicy);
	m_blendModeCombo->setMinimumSize(QSize(24, 0));
	initBlendModeOptions(false);
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &LassoFillSettings::updateBlendMode);

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

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindAutomaticAlphaPreserve(
		this, &LassoFillSettings::setAutomaticAlphaPerserve);

	return widget;
}

void LassoFillSettings::initBlendModeOptions(bool compatibilityMode)
{
	int blendMode = getCurrentBlendMode();
	{
		QSignalBlocker blocker(m_blendModeCombo);
		m_blendModeCombo->setModel(
			getFillBlendModesFor(m_automaticAlphaPreserve, compatibilityMode));
	}
	selectBlendMode(blendMode);
}

void LassoFillSettings::updateAlphaPreserve(bool alphaPreserve)
{
	int blendMode = m_blendModeCombo->currentData().toInt();
	canvas::blendmode::adjustAlphaBehavior(blendMode, alphaPreserve);

	int index = searchBlendModeComboIndex(m_blendModeCombo, blendMode);
	if(index != -1) {
		QSignalBlocker blocker(m_blendModeCombo);
		m_blendModeCombo->setCurrentIndex(index);
	}

	pushSettings();
}

void LassoFillSettings::updateBlendMode(int index)
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
	pushSettings();
}

void LassoFillSettings::selectBlendMode(int blendMode)
{
	int count = m_blendModeCombo->count();
	for(int i = 0; i < count; ++i) {
		if(m_blendModeCombo->itemData(i).toInt() == blendMode) {
			m_blendModeCombo->setCurrentIndex(i);
			break;
		}
	}
}

int LassoFillSettings::getCurrentBlendMode() const
{
	int blendMode = m_blendModeCombo->count() == 0
						? DP_BLEND_MODE_NORMAL
						: m_blendModeCombo->currentData().toInt();
	canvas::blendmode::adjustAlphaBehavior(
		blendMode, m_alphaPreserveButton->isChecked());
	return blendMode;
}

void LassoFillSettings::setAutomaticAlphaPerserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != m_automaticAlphaPreserve) {
		m_automaticAlphaPreserve = automaticAlphaPreserve;
		canvas::CanvasModel *canvas = controller()->model();
		initBlendModeOptions(canvas && canvas->isCompatibilityMode());
	}
}

void LassoFillSettings::setButtonState(bool pending)
{
	m_applyButton->setEnabled(pending);
	m_cancelButton->setEnabled(pending);
}

}

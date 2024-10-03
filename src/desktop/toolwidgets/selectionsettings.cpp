// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/expandshrinkspinner.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/tools/toolcontroller.h"
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <libclient/tools/magicwand.h>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> antialias{
	QStringLiteral("antialias"), false},
	shrink{QStringLiteral("shrink"), false};
static const ToolProperties::RangedValue<int> expand{
	QStringLiteral("expand"), 0, 0, 100},
	featherRadius{QStringLiteral("featherRadius"), 0, 0, 40},
	size{QStringLiteral("size"), 5000, 10, 5000},
	opacity{QStringLiteral("opacity"), 100, 1, 100},
	gap{QStringLiteral("gap"), 0, 0, 32},
	source{QStringLiteral("source"), 2, 0, 2},
	area{QStringLiteral("area"), 0, 0, 1},
	kernel{QStringLiteral("kernel"), 0, 0, 1};
static const ToolProperties::RangedValue<double> tolerance{
	QStringLiteral("tolerance"), 0.0, 0.0, 1.0};
}

SelectionSettings::SelectionSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

void SelectionSettings::setActiveTool(tools::Tool::Type tool)
{
	m_isMagicWand = tool == Tool::MAGICWAND;
	// Hide first, then show. Otherwise the UI temporarily gets larger and
	// expands the surrounding dock unnecessarily.
	if(m_isMagicWand) {
		m_selectionContainer->hide();
		m_magicWandContainer->show();
	} else {
		m_magicWandContainer->hide();
		m_selectionContainer->show();
	}
}

ToolProperties SelectionSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::antialias, m_antiAliasCheckBox->isChecked());
	cfg.setValue(props::size, m_sizeSlider->value());
	cfg.setValue(props::opacity, m_opacitySlider->value());
	cfg.setValue(
		props::tolerance,
		m_toleranceSlider->value() / qreal(m_toleranceSlider->maximum()));
	cfg.setValue(props::shrink, m_expandShrink->isShrink());
	cfg.setValue(props::expand, m_expandShrink->spinnerValue());
	cfg.setValue(props::kernel, m_expandShrink->kernel());
	cfg.setValue(props::featherRadius, m_featherSlider->value());
	cfg.setValue(props::gap, m_closeGapsSlider->value());
	cfg.setValue(props::source, m_sourceGroup->checkedId());
	cfg.setValue(props::area, m_areaGroup->checkedId());
	return cfg;
}

void SelectionSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_antiAliasCheckBox->setChecked(cfg.value(props::antialias));
	m_sizeSlider->setValue(cfg.value(props::size));
	m_opacitySlider->setValue(cfg.value(props::opacity));
	m_toleranceSlider->setValue(
		cfg.value(props::tolerance) * m_toleranceSlider->maximum());
	m_expandShrink->setSpinnerValue(cfg.value(props::expand));
	m_expandShrink->setShrink(cfg.value(props::shrink));
	m_expandShrink->setKernel(cfg.value(props::kernel));
	m_featherSlider->setValue(cfg.value(props::featherRadius));
	m_closeGapsSlider->setValue(cfg.value(props::gap));
	checkGroupButton(m_sourceGroup, cfg.value(props::source));
	checkGroupButton(m_areaGroup, cfg.value(props::area));
}

void SelectionSettings::stepAdjust1(bool increase)
{
	if(m_isMagicWand) {
		m_sizeSlider->setValue(stepLogarithmic(
			m_sizeSlider->minimum(), m_sizeSlider->maximum(),
			m_sizeSlider->value(), increase));
	}
}

int SelectionSettings::getSize() const
{
	if(m_isMagicWand) {
		int size = m_sizeSlider->value();
		return calculatePixelSize(size, isSizeUnlimited(size));
	} else {
		return 0;
	}
}

void SelectionSettings::setAction(QAction *starttransform)
{
	Q_ASSERT(m_startTransformButton);
	m_startTransformButton->setIcon(starttransform->icon());
	m_startTransformButton->setText(starttransform->text());
	m_startTransformButton->setStatusTip(starttransform->statusTip());
	m_startTransformButton->setToolTip(starttransform->statusTip());
	connect(
		m_startTransformButton, &QPushButton::clicked, starttransform,
		&QAction::trigger);
}

void SelectionSettings::setActionEnabled(bool enabled)
{
	Q_ASSERT(m_startTransformButton);
	m_startTransformButton->setEnabled(enabled);
}

void SelectionSettings::pushSettings()
{
	ToolController::SelectionParams selectionParams;
	selectionParams.antiAlias = m_antiAliasCheckBox->isChecked();
	selectionParams.defaultOp = m_headerGroup->checkedId();
	int size = m_sizeSlider->value();
	selectionParams.size = isSizeUnlimited(size) ? -1 : size;
	selectionParams.opacity = m_opacitySlider->value() / 100.0;
	selectionParams.tolerance =
		m_toleranceSlider->value() / qreal(m_toleranceSlider->maximum());
	selectionParams.expansion = m_expandShrink->effectiveValue();
	selectionParams.featherRadius = m_featherSlider->value();
	selectionParams.gap = m_closeGapsSlider->value();
	selectionParams.source = m_sourceGroup->checkedId();
	selectionParams.kernel = m_expandShrink->kernel();
	bool continuous = m_areaGroup->checkedId() == int(Area::Continuous);
	selectionParams.continuous = continuous;
	m_closeGapsSlider->setEnabled(continuous);
	ToolController *ctrl = controller();
	ctrl->setSelectionParams(selectionParams);
	if(ctrl->activeTool() == Tool::MAGICWAND) {
		static_cast<MagicWandTool *>(ctrl->getTool(Tool::MAGICWAND))
			->updateParameters();
	}
}

QWidget *SelectionSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	m_headerGroup = new QButtonGroup(m_headerWidget);

	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);
	headerLayout->addStretch(1);

	widgets::GroupedToolButton *replaceButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	replaceButton->setIcon(QIcon::fromTheme("drawpile_selection_replace"));
	replaceButton->setStatusTip(tr("Replace selection"));
	replaceButton->setToolTip(replaceButton->statusTip());
	replaceButton->setCheckable(true);
	replaceButton->setChecked(true);
	headerLayout->addWidget(replaceButton, 3);
	m_headerGroup->addButton(replaceButton, DP_MSG_SELECTION_PUT_OP_REPLACE);

	widgets::GroupedToolButton *uniteButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	uniteButton->setIcon(QIcon::fromTheme("drawpile_selection_unite"));
	uniteButton->setStatusTip(tr("Add to selection"));
	uniteButton->setToolTip(uniteButton->statusTip());
	uniteButton->setCheckable(true);
	headerLayout->addWidget(uniteButton, 3);
	m_headerGroup->addButton(uniteButton, DP_MSG_SELECTION_PUT_OP_UNITE);

	widgets::GroupedToolButton *intersectButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	intersectButton->setIcon(QIcon::fromTheme("drawpile_selection_intersect"));
	intersectButton->setStatusTip(tr("Intersect with selection"));
	intersectButton->setToolTip(intersectButton->statusTip());
	intersectButton->setCheckable(true);
	headerLayout->addWidget(intersectButton, 3);
	m_headerGroup->addButton(
		intersectButton, DP_MSG_SELECTION_PUT_OP_INTERSECT);

	widgets::GroupedToolButton *m_excludeButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_excludeButton->setIcon(QIcon::fromTheme("drawpile_selection_exclude"));
	m_excludeButton->setStatusTip(tr("Remove from selection"));
	m_excludeButton->setToolTip(m_excludeButton->statusTip());
	m_excludeButton->setCheckable(true);
	headerLayout->addWidget(m_excludeButton, 3);
	m_headerGroup->addButton(m_excludeButton, DP_MSG_SELECTION_PUT_OP_EXCLUDE);

	headerLayout->addStretch(1);

	connect(
		m_headerGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&SelectionSettings::pushSettings);

	QWidget *widget = new QWidget(parent);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(3, 3, 3, 3);
	layout->setSpacing(3);

	m_selectionContainer = new QWidget;
	QVBoxLayout *selectionLayout = new QVBoxLayout(m_selectionContainer);
	selectionLayout->setContentsMargins(0, 0, 0, 0);
	selectionLayout->setSpacing(3);
	layout->addWidget(m_selectionContainer);

	m_antiAliasCheckBox = new QCheckBox(tr("Anti-aliasing"));
	m_antiAliasCheckBox->setStatusTip(tr("Smoothe out selection edges"));
	m_antiAliasCheckBox->setToolTip(m_antiAliasCheckBox->statusTip());
	connect(
		m_antiAliasCheckBox, &QCheckBox::clicked, this,
		&SelectionSettings::pushSettings);
	selectionLayout->addWidget(m_antiAliasCheckBox);

	m_magicWandContainer = new QWidget;
	QFormLayout *magicWandLayout = new QFormLayout(m_magicWandContainer);
	magicWandLayout->setContentsMargins(0, 0, 0, 0);
	magicWandLayout->setSpacing(3);
	layout->addWidget(m_magicWandContainer);

	m_sizeSlider = new KisSliderSpinBox;
	m_sizeSlider->setRange(props::size.min, props::size.max);
	m_sizeSlider->setPrefix(
		QCoreApplication::translate("FillSettings", "Size Limit: "));
	m_sizeSlider->setSuffix(QCoreApplication::translate("FillSettings", "px"));
	m_sizeSlider->setBlockUpdateSignalOnDrag(true);
	connect(
		m_sizeSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&SelectionSettings::pushSettings);
	connect(
		m_sizeSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&SelectionSettings::updateSize);
	magicWandLayout->addRow(m_sizeSlider);

	m_opacitySlider = new KisSliderSpinBox;
	m_opacitySlider->setRange(props::opacity.min, props::opacity.max);
	m_opacitySlider->setPrefix(
		QCoreApplication::translate("FillSettings", "Opacity: "));
	m_opacitySlider->setSuffix(
		QCoreApplication::translate("FillSettings", "%"));
	m_opacitySlider->setBlockUpdateSignalOnDrag(true);
	connect(
		m_opacitySlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &SelectionSettings::pushSettings);
	magicWandLayout->addRow(m_opacitySlider);

	m_toleranceSlider = new KisSliderSpinBox;
	m_toleranceSlider->setRange(0, 254);
	m_toleranceSlider->setPrefix(
		QCoreApplication::translate("FillSettings", "Tolerance: "));
	m_toleranceSlider->setBlockUpdateSignalOnDrag(true);
	connect(
		m_toleranceSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &SelectionSettings::pushSettings);
	magicWandLayout->addRow(m_toleranceSlider);

	m_expandShrink = new widgets::ExpandShrinkSpinner;
	m_expandShrink->setSpinnerRange(props::expand.min, props::expand.max);
	m_expandShrink->setBlockUpdateSignalOnDrag(true);
	connect(
		m_expandShrink, &widgets::ExpandShrinkSpinner::spinnerValueChanged,
		this, &SelectionSettings::pushSettings);
	connect(
		m_expandShrink, &widgets::ExpandShrinkSpinner::shrinkChanged, this,
		&SelectionSettings::pushSettings);
	connect(
		m_expandShrink, &widgets::ExpandShrinkSpinner::kernelChanged, this,
		&SelectionSettings::pushSettings);
	magicWandLayout->addRow(m_expandShrink);

	m_featherSlider = new KisSliderSpinBox;
	m_featherSlider->setRange(0, 40);
	m_featherSlider->setPrefix(
		QCoreApplication::translate("FillSettings", "Feather: "));
	m_featherSlider->setSuffix(
		QCoreApplication::translate("FillSettings", "px"));
	m_featherSlider->setBlockUpdateSignalOnDrag(true);
	connect(
		m_featherSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &SelectionSettings::pushSettings);
	magicWandLayout->addRow(m_featherSlider);

	m_closeGapsSlider = new KisSliderSpinBox;
	m_closeGapsSlider->setRange(0, 40);
	m_closeGapsSlider->setPrefix(
		QCoreApplication::translate("FillSettings", "Close Gaps: "));
	m_closeGapsSlider->setSuffix(
		QCoreApplication::translate("FillSettings", "px"));
	m_closeGapsSlider->setBlockUpdateSignalOnDrag(true);
	connect(
		m_closeGapsSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &SelectionSettings::pushSettings);
	magicWandLayout->addRow(m_closeGapsSlider);

	m_sourceGroup = new QButtonGroup(this);
	connect(
		m_sourceGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&SelectionSettings::pushSettings);

	QHBoxLayout *sourceLayout = new QHBoxLayout;
	sourceLayout->setContentsMargins(0, 0, 0, 0);
	sourceLayout->setSpacing(0);

	widgets::GroupedToolButton *sourceMergedButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	sourceMergedButton->setIcon(QIcon::fromTheme("arrow-down-double"));
	sourceMergedButton->setStatusTip(
		QCoreApplication::translate("FillSettings", "Merged image"));
	sourceMergedButton->setToolTip(sourceMergedButton->statusTip());
	sourceMergedButton->setCheckable(true);
	sourceMergedButton->setChecked(true);
	m_sourceGroup->addButton(
		sourceMergedButton, int(ToolController::SelectionSource::Merged));
	sourceLayout->addWidget(sourceMergedButton);

	widgets::GroupedToolButton *sourceMergedWithoutBackgroundButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	sourceMergedWithoutBackgroundButton->setIcon(
		QIcon::fromTheme("arrow-down"));
	sourceMergedWithoutBackgroundButton->setStatusTip(
		QCoreApplication::translate(
			"FillSettings", "Merged without background"));
	sourceMergedWithoutBackgroundButton->setToolTip(
		sourceMergedWithoutBackgroundButton->statusTip());
	sourceMergedWithoutBackgroundButton->setCheckable(true);
	sourceMergedWithoutBackgroundButton->setChecked(false);
	m_sourceGroup->addButton(
		sourceMergedWithoutBackgroundButton,
		int(ToolController::SelectionSource::MergedWithoutBackground));
	sourceLayout->addWidget(sourceMergedWithoutBackgroundButton);

	widgets::GroupedToolButton *sourceLayerButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	sourceLayerButton->setIcon(QIcon::fromTheme("layer-visible-on"));
	sourceLayerButton->setStatusTip(
		QCoreApplication::translate("FillSettings", "Current layer"));
	sourceLayerButton->setToolTip(sourceLayerButton->statusTip());
	sourceLayerButton->setCheckable(true);
	sourceLayerButton->setChecked(false);
	m_sourceGroup->addButton(
		sourceLayerButton, int(ToolController::SelectionSource::Layer));
	sourceLayout->addWidget(sourceLayerButton);

	// Add a dummy hidden combo box to match the fill tool layout exactly.
	// Otherwise the tool buttons end up slightly shorter.
	QComboBox *m_sourceDummyCombo = new QComboBox;
	m_sourceDummyCombo->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	utils::setWidgetRetainSizeWhenHidden(m_sourceDummyCombo, true);
	m_sourceDummyCombo->hide();
	sourceLayout->addWidget(m_sourceDummyCombo);

	magicWandLayout->addRow(
		QCoreApplication::translate("FillSettings", "Source:"), sourceLayout);

	m_areaGroup = new QButtonGroup(this);
	connect(
		m_areaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&SelectionSettings::pushSettings);

	QHBoxLayout *areaLayout = new QHBoxLayout;
	areaLayout->setContentsMargins(0, 0, 0, 0);
	areaLayout->setSpacing(0);

	widgets::GroupedToolButton *areaContinuousButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	areaContinuousButton->setIcon(QIcon::fromTheme("fill-color"));
	areaContinuousButton->setStatusTip(tr("Select continuous area"));
	areaContinuousButton->setToolTip(areaContinuousButton->statusTip());
	areaContinuousButton->setCheckable(true);
	areaContinuousButton->setChecked(true);
	m_areaGroup->addButton(areaContinuousButton, int(Area::Continuous));
	areaLayout->addWidget(areaContinuousButton);

	widgets::GroupedToolButton *areaSimilarButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	areaSimilarButton->setIcon(QIcon::fromTheme("color-picker"));
	areaSimilarButton->setStatusTip(tr("Select similar color"));
	areaSimilarButton->setToolTip(areaSimilarButton->statusTip());
	areaSimilarButton->setCheckable(true);
	areaSimilarButton->setChecked(false);
	m_areaGroup->addButton(areaSimilarButton, int(Area::Similar));
	areaLayout->addWidget(areaSimilarButton);

	// See sourceDummyCombo above.
	QComboBox *m_areaDummyCombo = new QComboBox;
	m_areaDummyCombo->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Preferred);
	utils::setWidgetRetainSizeWhenHidden(m_areaDummyCombo, true);
	m_areaDummyCombo->hide();
	areaLayout->addWidget(m_areaDummyCombo);

	magicWandLayout->addRow(
		QCoreApplication::translate("FillSettings", "Mode:"), areaLayout);

	m_startTransformButton = new QPushButton;
	m_startTransformButton->setEnabled(false);
	layout->addWidget(m_startTransformButton);

	layout->addStretch();

	setActiveTool(tools::Tool::SELECTION);
	updateSize(m_sizeSlider->value());
	return widget;
}

void SelectionSettings::updateSize(int size)
{
	bool unlimited = isSizeUnlimited(size);
	m_sizeSlider->setOverrideText(
		unlimited ? QCoreApplication::translate(
						"FillSettings", "Size Limit: Unlimited")
				  : QString());
	emit pixelSizeChanged(calculatePixelSize(size, unlimited));
}

bool SelectionSettings::isSizeUnlimited(int size)
{
	return size >= props::size.max;
}

int SelectionSettings::calculatePixelSize(int size, bool unlimited)
{
	return unlimited ? 0 : size * 2 + 1;
}

}

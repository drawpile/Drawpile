// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/main.h"
#include "desktop/utils/blendmodes.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_fillsettings.h"
#include <QAction>
#include <QButtonGroup>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> shrink{
	QStringLiteral("shrink"), false},
	editable{QStringLiteral("editable"), false},
	confirm{QStringLiteral("confirm"), false};
static const ToolProperties::RangedValue<int> expand{
	QStringLiteral("expand"), 0, 0, 100},
	featherRadius{QStringLiteral("featherRadius"), 0, 0, 40},
	blendMode{
		QStringLiteral("blendMode"), DP_BLEND_MODE_NORMAL, 0,
		DP_BLEND_MODE_MAX},
	size{QStringLiteral("limit"), 5000, 10, 5000},
	opacity{QStringLiteral("opacity"), 100, 1, 100},
	gap{QStringLiteral("gap"), 0, 0, 32},
	source{QStringLiteral("source"), 2, 0, 3},
	area{QStringLiteral("area"), 0, 0, 2},
	kernel{QStringLiteral("kernel"), 0, 0, 1};
static const ToolProperties::RangedValue<double> tolerance{
	QStringLiteral("tolerance"), 0.0, 0.0, 1.0};
}

FillSettings::FillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, m_previousMode(DP_BLEND_MODE_NORMAL)
	, m_previousEraseMode(DP_BLEND_MODE_ERASE)
{
}

FillSettings::~FillSettings()
{
	delete m_ui;
}

bool FillSettings::isLocked()
{
	return !m_featureAccess;
}

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setSpacing(0);
	headerLayout->setContentsMargins(0, 0, 0, 0);

	widgets::GroupedToolButton *menuButton = new widgets::GroupedToolButton(
		widgets::GroupedToolButton::NotGrouped, m_headerWidget);
	menuButton->setIcon(QIcon::fromTheme("application-menu"));
	menuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	menuButton->setPopupMode(QToolButton::InstantPopup);
	menuButton->setStatusTip(tr("Fill tool settings"));
	menuButton->setToolTip(menuButton->statusTip());
	headerLayout->addWidget(menuButton);

	m_menu = new QMenu(menuButton);
	menuButton->setMenu(m_menu);

	m_editableAction = m_menu->addAction(tr("&Edit pending fills"));
	m_editableAction->setStatusTip(tr(
		"Apply changes in settings, color and layer to fills not yet applied"));
	m_editableAction->setCheckable(true);
	connect(
		m_editableAction, &QAction::triggered, this,
		&FillSettings::updateSettings);

	m_confirmAction = m_menu->addAction(tr("&Confirm fills with second click"));
	m_confirmAction->setStatusTip(tr(
		"Lets you apply fills with a click instead of starting another fill"));
	m_confirmAction->setCheckable(true);
	connect(
		m_confirmAction, &QAction::triggered, this,
		&FillSettings::updateSettings);

	headerLayout->addStretch();

	m_stack = new QStackedWidget(parent);
	m_stack->setContentsMargins(0, 0, 0, 0);

	QWidget *uiwidget = new QWidget(parent);
	m_ui = new Ui_FillSettings;
	m_ui->setupUi(uiwidget);
	m_stack->addWidget(uiwidget);

	m_ui->size->setExponentRatio(3.0);
	m_ui->expandShrink->setBlockUpdateSignalOnDrag(true);
	m_ui->expandShrink->setSpinnerRange(props::expand.min, props::expand.max);
	utils::setWidgetRetainSizeWhenHidden(m_ui->sourceDummyCombo, true);
	m_ui->sourceDummyCombo->hide();
	utils::setWidgetRetainSizeWhenHidden(m_ui->sourceFillSource, true);

	m_sourceGroup = new QButtonGroup(this);
	m_sourceGroup->setExclusive(true);
	m_sourceGroup->addButton(
		m_ui->sourceMerged, int(FloodFill::Source::Merged));
	m_sourceGroup->addButton(
		m_ui->sourceMergedWithoutBackground,
		int(FloodFill::Source::MergedWithoutBackground));
	m_sourceGroup->addButton(
		m_ui->sourceLayer, int(FloodFill::Source::CurrentLayer));
	m_sourceGroup->addButton(
		m_ui->sourceFillSource, int(FloodFill::Source::FillSourceLayer));

	m_areaGroup = new QButtonGroup(this);
	m_areaGroup->setExclusive(true);
	m_areaGroup->addButton(
		m_ui->areaContinuous, int(FloodFill::Area::Continuous));
	m_areaGroup->addButton(m_ui->areaSimilar, int(FloodFill::Area::Similar));
	m_areaGroup->addButton(
		m_ui->areaSelection, int(FloodFill::Area::Selection));

	m_ui->alphaPreserve->setToolTip(
		QCoreApplication::translate("BrushDock", "Preserve alpha"));
	m_ui->alphaPreserve->setStatusTip(m_ui->alphaPreserve->toolTip());

	initBlendModeOptions(false);

	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSize);
	connect(
		m_ui->opacity, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSettings);
	connect(
		m_ui->tolerance, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateTolerance);
	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSettings);
	connect(
		m_ui->expandShrink, &widgets::ExpandShrinkSpinner::spinnerValueChanged,
		this, &FillSettings::updateSettings);
	connect(
		m_ui->expandShrink, &widgets::ExpandShrinkSpinner::shrinkChanged, this,
		&FillSettings::updateSettings);
	connect(
		m_ui->expandShrink, &widgets::ExpandShrinkSpinner::kernelChanged, this,
		&FillSettings::updateSettings);
	connect(
		m_ui->feather, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSettings);
	connect(
		m_ui->gap, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSettings);
	connect(
		m_sourceGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::updateSettings);
	connect(
		m_areaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::updateSettings);
	connect(
		m_areaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::updateSize);
	connect(
		m_ui->alphaPreserve, &widgets::GroupedToolButton::clicked, this,
		&FillSettings::updateAlphaPreserve);
	connect(
		m_ui->blendModeCombo,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&FillSettings::updateBlendMode);
	updateSize();

	m_ui->applyButton->setIcon(
		uiwidget->style()->standardIcon(QStyle::SP_DialogApplyButton));
	connect(
		m_ui->applyButton, &QPushButton::clicked, controller(),
		&ToolController::finishMultipartDrawing);

	m_ui->cancelButton->setIcon(
		uiwidget->style()->standardIcon(QStyle::SP_DialogCancelButton));
	connect(
		m_ui->cancelButton, &QPushButton::clicked, controller(),
		&ToolController::cancelMultipartDrawing);

	connect(
		controller(), &ToolController::floodFillStateChanged, this,
		&FillSettings::setButtonState);
	setButtonState(false, false);

	connect(
		controller(), &ToolController::floodFillDragChanged, this,
		&FillSettings::setDragState);

	QWidget *disabledWidget = new QWidget;
	QVBoxLayout *disabledLayout = new QVBoxLayout(disabledWidget);

	m_permissionDeniedLabel =
		new QLabel(tr("You don't have permission to use the fill tool."));
	m_permissionDeniedLabel->setTextFormat(Qt::PlainText);
	m_permissionDeniedLabel->setWordWrap(true);
	disabledLayout->addWidget(m_permissionDeniedLabel);

	disabledLayout->addStretch();
	m_stack->addWidget(disabledWidget);

	updateWidgets();

	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindAutomaticAlphaPreserve(
		this, &FillSettings::setAutomaticAlphaPerserve);

	return m_stack;
}

void FillSettings::setActions(QAction *automaticAlphaPreserve)
{
	m_menu->addSeparator();
	m_menu->addAction(automaticAlphaPreserve);
}

void FillSettings::setFeatureAccess(bool featureAccess)
{
	if(!featureAccess && m_featureAccess) {
		controller()->getTool(Tool::FLOODFILL)->cancelMultipart();
	}
	m_featureAccess = featureAccess;
	updateWidgets();
}

void FillSettings::setCompatibilityMode(bool compatibilityMode)
{
	initBlendModeOptions(compatibilityMode);
	m_ui->alphaPreserve->setEnabled(!compatibilityMode);
}

void FillSettings::pushSettings()
{
	int area = m_areaGroup->checkedId();
	bool floodOptionsEnabled = area != int(FloodFill::Area::Selection);
	m_ui->size->setEnabled(floodOptionsEnabled && !m_haveSelection);
	m_ui->tolerance->setEnabled(floodOptionsEnabled);
	m_ui->gap->setEnabled(area == int(FloodFill::Area::Continuous));
	m_ui->sourceMerged->setEnabled(floodOptionsEnabled);
	m_ui->sourceMergedWithoutBackground->setEnabled(floodOptionsEnabled);
	m_ui->sourceLayer->setEnabled(floodOptionsEnabled);
	canvas::CanvasModel *canvas = controller()->model();
	bool haveFillSource =
		canvas && canvas->layerlist()->fillSourceLayerId() != 0;
	m_ui->sourceLayer->setGroupPosition(
		haveFillSource ? widgets::GroupedToolButton::GroupCenter
					   : widgets::GroupedToolButton::GroupRight);
	m_ui->sourceFillSource->setEnabled(floodOptionsEnabled && haveFillSource);
	m_ui->sourceFillSource->setVisible(haveFillSource);

	int blendMode = getCurrentBlendMode();
	if(canvas::blendmode::presentsAsEraser(blendMode)) {
		m_previousEraseMode = blendMode;
	} else {
		m_previousMode = blendMode;
	}

	FloodFill *tool =
		static_cast<FloodFill *>(controller()->getTool(Tool::FLOODFILL));
	int size = m_ui->size->value();
	tool->setParameters(
		m_toleranceBeforeDrag < 0 ? m_ui->tolerance->value()
								  : m_toleranceBeforeDrag,
		m_ui->expandShrink->effectiveValue(), m_ui->expandShrink->kernel(),
		m_ui->feather->value(), isSizeUnlimited(size) ? -1 : size,
		m_ui->opacity->value() / 100.0, m_ui->gap->value(),
		FloodFill::Source(m_sourceGroup->checkedId()), blendMode,
		FloodFill::Area(area), m_editableAction->isChecked(),
		m_confirmAction->isChecked());

	if(!m_ui->sourceFillSource->isEnabled() &&
	   m_ui->sourceFillSource->isChecked()) {
		m_ui->sourceLayer->click();
	}
}

void FillSettings::toggleEraserMode()
{
	int blendMode = canvas::blendmode::presentsAsEraser(getCurrentBlendMode())
						? m_previousMode
						: m_previousEraseMode;
	int index = searchBlendModeComboIndex(m_ui->blendModeCombo, blendMode);
	if(index != -1) {
		{
			QSignalBlocker blocker(m_ui->blendModeCombo);
			m_ui->blendModeCombo->setCurrentIndex(index);
		}
		if(m_automaticAlphaPreserve) {
			QSignalBlocker blocker(m_ui->alphaPreserve);
			m_ui->alphaPreserve->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(
					m_ui->blendModeCombo->currentData().toInt()));
		}
		pushSettings();
	}
}

void FillSettings::toggleAlphaPreserve()
{
	int blendMode = getCurrentBlendMode();
	selectBlendMode(
		canvas::blendmode::presentsAsAlphaPreserving(blendMode)
			? canvas::blendmode::toAlphaAffecting(blendMode)
			: canvas::blendmode::toAlphaPreserving(blendMode));
}

void FillSettings::updateSelection()
{
	canvas::CanvasModel *canvas = controller()->model();
	m_haveSelection = canvas && canvas->selection()->isValid();
	m_ui->size->setEnabled(
		m_areaGroup->checkedId() != int(FloodFill::Area::Selection) &&
		!m_haveSelection);
	updateSize();
}

void FillSettings::updateFillSourceLayerId(int layerId)
{
	bool haveFillSource = layerId != 0;
	m_ui->sourceFillSource->setEnabled(
		haveFillSource &&
		m_areaGroup->checkedId() != int(FloodFill::Area::Selection));
	m_ui->sourceFillSource->setVisible(haveFillSource);
	m_ui->sourceLayer->setGroupPosition(
		haveFillSource ? widgets::GroupedToolButton::GroupCenter
					   : widgets::GroupedToolButton::GroupRight);
	if(haveFillSource) {
		m_ui->sourceFillSource->click();
	} else {
		m_ui->sourceLayer->click();
	}
}

ToolProperties FillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(
		props::tolerance,
		m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()));
	cfg.setValue(props::expand, m_ui->expandShrink->spinnerValue());
	cfg.setValue(props::editable, m_editableAction->isChecked());
	cfg.setValue(props::confirm, m_confirmAction->isChecked());
	cfg.setValue(props::shrink, m_ui->expandShrink->isShrink());
	cfg.setValue(props::kernel, m_ui->expandShrink->kernel());
	cfg.setValue(props::featherRadius, m_ui->feather->value());
	cfg.setValue(props::size, m_ui->size->value());
	cfg.setValue(props::opacity, m_ui->opacity->value());
	cfg.setValue(props::gap, m_ui->gap->value());
	cfg.setValue(props::blendMode, getCurrentBlendMode());
	int source = m_sourceGroup->checkedId();
	cfg.setValue(
		props::source, source == int(FloodFill::Source::FillSourceLayer)
						   ? int(FloodFill::Source::CurrentLayer)
						   : source);
	cfg.setValue(props::area, m_areaGroup->checkedId());
	return cfg;
}

void FillSettings::setForeground(const QColor &color)
{
	brushes::ActiveBrush b;
	b.classic().size.max = 1;
	b.setQColor(color);
	controller()->setActiveBrush(b);
}

int FillSettings::getSize() const
{
	return calculatePixelSize(m_ui->size->value());
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	QScopedValueRollback<bool> rollback(m_updating);

	m_editableAction->setChecked(cfg.value(props::editable));
	m_confirmAction->setChecked(cfg.value(props::confirm));
	m_ui->tolerance->setValue(
		cfg.value(props::tolerance) * m_ui->tolerance->maximum());
	m_ui->expandShrink->setSpinnerValue(cfg.value(props::expand));
	m_ui->expandShrink->setShrink(cfg.value(props::shrink));
	m_ui->expandShrink->setKernel(cfg.value(props::kernel));
	m_ui->feather->setValue(cfg.value(props::featherRadius));
	m_ui->size->setValue(cfg.value(props::size));
	m_ui->opacity->setValue(cfg.value(props::opacity));
	m_ui->gap->setValue(cfg.value(props::gap));

	int blendMode = cfg.value(props::blendMode);
	selectBlendMode(blendMode);
	m_ui->alphaPreserve->setChecked(
		canvas::blendmode::presentsAsAlphaPreserving(blendMode));
	if(canvas::blendmode::presentsAsEraser(blendMode)) {
		m_previousMode = DP_BLEND_MODE_NORMAL;
		m_previousEraseMode = blendMode;
	} else {
		m_previousMode = blendMode;
		m_previousEraseMode = DP_BLEND_MODE_ERASE;
	}

	checkGroupButton(m_sourceGroup, cfg.value(props::source));
	checkGroupButton(m_areaGroup, cfg.value(props::area));

	pushSettings();
}

void FillSettings::quickAdjust1(qreal adjustment)
{
	KisSliderSpinBox *size = m_ui->size;
	if(size->isEnabled()) {
		m_quickAdjust1 += adjustment * 10.0;
		qreal i;
		qreal f = modf(m_quickAdjust1, &i);
		if(int(i)) {
			m_quickAdjust1 = f;
			size->setValue(size->value() + i);
		}
	}
}

void FillSettings::stepAdjust1(bool increase)
{
	KisSliderSpinBox *size = m_ui->size;
	if(size->isEnabled()) {
		size->setValue(stepLogarithmic(
			size->minimum(), size->maximum(), size->value(), increase));
	}
}

void FillSettings::updateTolerance()
{
	if(m_toleranceBeforeDrag < 0) {
		updateSettings();
	}
}

void FillSettings::updateSettings()
{
	if(!m_updating) {
		pushSettings();
	}
}

void FillSettings::updateSize()
{
	int size = m_ui->size->value();
	int pixelSize = calculatePixelSize(size);
	m_ui->size->setOverrideText(
		m_haveSelection	 ? tr("Size Limit: Selection")
		: pixelSize == 0 ? tr("Size Limit: Unlimited")
						 : QString());
	emit pixelSizeChanged(pixelSize);
}

bool FillSettings::isSizeUnlimited(int size)
{
	return size >= props::size.max;
}

int FillSettings::calculatePixelSize(int size) const
{
	bool unlimited =
		m_haveSelection || isSizeUnlimited(size) ||
		m_areaGroup->checkedId() == int(FloodFill::Area::Selection);
	return unlimited ? 0 : size * 2 + 1;
}

void FillSettings::initBlendModeOptions(bool compatibilityMode)
{
	int selectedBlendMode = getCurrentBlendMode();
	{
		QSignalBlocker blockerc(m_ui->blendModeCombo);
		m_ui->blendModeCombo->setModel(
			getFillBlendModesFor(m_automaticAlphaPreserve, compatibilityMode));
	}
	selectBlendMode(selectedBlendMode);
}

void FillSettings::updateAlphaPreserve()
{
	if(!m_updating) {
		QScopedValueRollback<bool> rollback(m_updating, true);
		int index = searchBlendModeComboIndex(
			m_ui->blendModeCombo, getCurrentBlendMode());
		if(index != -1) {
			QSignalBlocker blocker(m_ui->blendModeCombo);
			m_ui->blendModeCombo->setCurrentIndex(index);
		}
		pushSettings();
	}
}

void FillSettings::updateBlendMode(int index)
{
	if(!m_updating) {
		if(m_automaticAlphaPreserve) {
			QScopedValueRollback<bool> rollback(m_updating, true);
			m_ui->alphaPreserve->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(
					m_ui->blendModeCombo->itemData(index).toInt()));
		}
		pushSettings();
	}
}

void FillSettings::selectBlendMode(int blendMode)
{
	int index = searchBlendModeComboIndex(m_ui->blendModeCombo, blendMode);
	if(index != -1) {
		{
			QSignalBlocker blocker(m_ui->blendModeCombo);
			m_ui->blendModeCombo->setCurrentIndex(index);
		}
		{
			QSignalBlocker blocker(m_ui->alphaPreserve);
			m_ui->alphaPreserve->setChecked(
				canvas::blendmode::presentsAsAlphaPreserving(blendMode));
		}
		pushSettings();
	}
}

int FillSettings::getCurrentBlendMode() const
{
	int blendMode = m_ui->blendModeCombo->count() == 0
						? DP_BLEND_MODE_NORMAL
						: m_ui->blendModeCombo->currentData().toInt();
	canvas::blendmode::adjustAlphaBehavior(
		blendMode, m_ui->alphaPreserve->isChecked());
	return blendMode;
}

void FillSettings::setButtonState(bool running, bool pending)
{
	m_ui->applyButton->setEnabled(pending);
	m_ui->cancelButton->setEnabled(running || pending);
}

void FillSettings::setDragState(bool dragging, int tolerance)
{
	if(dragging) {
		if(m_toleranceBeforeDrag < 0) {
			m_toleranceBeforeDrag = m_ui->tolerance->value();
		}
		m_ui->tolerance->setValue(tolerance);
	} else if(m_toleranceBeforeDrag >= 0) {
		m_ui->tolerance->setValue(m_toleranceBeforeDrag);
		m_toleranceBeforeDrag = -1;
	}
}

void FillSettings::updateWidgets()
{
	if(m_stack) {
		utils::ScopedUpdateDisabler disabler(m_stack);
		m_stack->setCurrentIndex(m_featureAccess ? 0 : 1);
		m_permissionDeniedLabel->setVisible(!m_featureAccess);
	}
}

void FillSettings::setAutomaticAlphaPerserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != m_automaticAlphaPreserve) {
		m_automaticAlphaPreserve = automaticAlphaPreserve;
		canvas::CanvasModel *canvas = controller()->model();
		initBlendModeOptions(canvas && canvas->isCompatibilityMode());
	}
}

}

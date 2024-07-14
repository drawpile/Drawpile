// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_fillsettings.h"
#include <QButtonGroup>
#include <QIcon>
#include <QSignalBlocker>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> shrink{
	QStringLiteral("shrink"), false};
static const ToolProperties::RangedValue<int> expand{
	QStringLiteral("expand"), 0, 0, 100},
	featherRadius{QStringLiteral("featherRadius"), 0, 0, 40},
	blendMode{
		QStringLiteral("blendMode"), DP_BLEND_MODE_NORMAL, 0,
		DP_BLEND_MODE_MAX},
	size{QStringLiteral("size"), 500, 10, 5000},
	opacity{QStringLiteral("opacity"), 100, 1, 100},
	gap{QStringLiteral("gap"), 0, 0, 32},
	source{QStringLiteral("source"), 2, 0, 3},
	area{QStringLiteral("area"), 0, 0, 2};
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

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	m_ui = new Ui_FillSettings;
	m_ui->setupUi(uiwidget);
	m_ui->size->setExponentRatio(3.0);
	utils::setWidgetRetainSizeWhenHidden(m_ui->sourceDummyCombo, true);
	m_ui->sourceDummyCombo->hide();
	utils::setWidgetRetainSizeWhenHidden(m_ui->sourceFillSource, true);

	m_expandGroup = new QButtonGroup(this);
	m_expandGroup->addButton(m_ui->expandButton, 0);
	m_expandGroup->addButton(m_ui->shrinkButton, 1);

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

	initBlendModeOptions();

	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::updateSize);
	connect(
		m_ui->opacity, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->tolerance, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_expandGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->expand, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->feather, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->gap, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_sourceGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::pushSettings);
	connect(
		m_areaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::pushSettings);
	connect(
		m_areaGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		&FillSettings::updateSize);
	connect(
		m_ui->blendModeCombo,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&FillSettings::pushSettings);
	updateSize();
	return uiwidget;
}

void FillSettings::setCompatibilityMode(bool compatibilityMode)
{
	if((compatibilityMode && !m_compatibilityMode) ||
	   (!compatibilityMode && m_compatibilityMode)) {
		m_compatibilityMode = compatibilityMode;
		initBlendModeOptions();
	}
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

	int blendMode = m_ui->blendModeCombo->currentData().toInt();
	if(canvas::blendmode::isValidEraseMode(DP_BlendMode(blendMode))) {
		m_previousEraseMode = blendMode;
	} else {
		m_previousMode = blendMode;
	}

	FloodFill *tool =
		static_cast<FloodFill *>(controller()->getTool(Tool::FLOODFILL));
	bool shrink = m_expandGroup->checkedId() != 0;
	int size = m_ui->size->value();
	tool->setParameters(
		m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()),
		m_ui->expand->value() * (shrink ? -1 : 1), m_ui->feather->value(),
		isSizeUnlimited(size) ? -1 : size, m_ui->opacity->value() / 100.0,
		m_ui->gap->value(), FloodFill::Source(m_sourceGroup->checkedId()),
		blendMode, FloodFill::Area(area));

	m_ui->expand->setPrefix(shrink ? tr("Shrink: ") : tr("Expand: "));

	if(!m_ui->sourceFillSource->isEnabled() &&
	   m_ui->sourceFillSource->isChecked()) {
		m_ui->sourceLayer->click();
	}
}

void FillSettings::toggleEraserMode()
{
	int blendMode = m_ui->blendModeCombo->currentData().toInt();
	selectBlendMode(
		canvas::blendmode::isValidEraseMode(DP_BlendMode(blendMode))
			? m_previousMode
			: m_previousEraseMode);
}

void FillSettings::toggleRecolorMode()
{
	int blendMode = m_ui->blendModeCombo->currentData().toInt();
	selectBlendMode(
		blendMode == DP_BLEND_MODE_RECOLOR ? DP_BLEND_MODE_NORMAL
										   : DP_BLEND_MODE_RECOLOR);
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
	cfg.setValue(props::shrink, m_expandGroup->checkedId() != 0);
	cfg.setValue(props::expand, m_ui->expand->value());
	cfg.setValue(props::featherRadius, m_ui->feather->value());
	cfg.setValue(props::size, m_ui->size->value());
	cfg.setValue(props::opacity, m_ui->opacity->value());
	cfg.setValue(props::gap, m_ui->gap->value());
	cfg.setValue(props::blendMode, m_ui->blendModeCombo->currentData().toInt());
	cfg.setValue(props::source, m_sourceGroup->checkedId());
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
	m_ui->tolerance->setValue(
		cfg.value(props::tolerance) * m_ui->tolerance->maximum());
	checkGroupButton(m_expandGroup, cfg.value(props::shrink) ? 1 : 0);
	m_ui->expand->setValue(cfg.value(props::expand));
	m_ui->feather->setValue(cfg.value(props::featherRadius));
	m_ui->size->setValue(cfg.value(props::size));
	m_ui->opacity->setValue(cfg.value(props::opacity));
	m_ui->gap->setValue(cfg.value(props::gap));

	int blendMode = cfg.value(props::blendMode);
	selectBlendMode(blendMode);
	if(canvas::blendmode::isValidEraseMode(DP_BlendMode(blendMode))) {
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

void FillSettings::initBlendModeOptions()
{
	int selectedBlendMode = m_ui->blendModeCombo->count() == 0
								? DP_BLEND_MODE_NORMAL
								: m_ui->blendModeCombo->currentData().toInt();
	{
		QSignalBlocker blocker(m_ui->blendModeCombo);
		m_ui->blendModeCombo->clear();
		for(const canvas::blendmode::Named &named :
			canvas::blendmode::pasteModeNames()) {
			if(!m_compatibilityMode ||
			   canvas::blendmode::isBackwardCompatibleMode(named.mode)) {
				m_ui->blendModeCombo->addItem(named.name, int(named.mode));
			}
		}
	}
	selectBlendMode(
		!m_compatibilityMode || canvas::blendmode::isBackwardCompatibleMode(
									DP_BlendMode(selectedBlendMode))
			? selectedBlendMode
			: DP_BLEND_MODE_NORMAL);
}

void FillSettings::selectBlendMode(int blendMode)
{
	int count = m_ui->blendModeCombo->count();
	for(int i = 0; i < count; ++i) {
		if(m_ui->blendModeCombo->itemData(i).toInt() == blendMode) {
			m_ui->blendModeCombo->setCurrentIndex(i);
			break;
		}
	}
}

}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/brushsettingsdialog.h"
#include "desktop/widgets/curvewidget.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/toolmessage.h"
#include "libclient/canvas/blendmodes.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <dpengine/libmypaint/mypaint-brush-settings.h>

namespace dialogs {

struct BrushSettingsDialog::Private {
	struct MyPaintPage {
		KisDoubleSliderSpinBox *baseValueSpinner;
		widgets::MyPaintInput *inputs[MYPAINT_BRUSH_INPUTS_COUNT];
		QLabel *constantLabel;
		QLabel *indirectLabel;
	};

	QListWidget *categoryWidget;
	QStackedWidget *stackedWidget;
	QComboBox *brushTypeCombo;
	QLabel *brushModeLabel;
	QComboBox *brushModeCombo;
	QLabel *eraseModeLabel;
	QComboBox *eraseModeCombo;
	QLabel *paintModeLabel;
	QComboBox *paintModeCombo;
	QCheckBox *eraseModeBox;
	QCheckBox *colorPickBox;
	QCheckBox *lockAlphaBox;
	KisSliderSpinBox *spacingSpinner;
	KisSliderSpinBox *stabilizerSpinner;
	QLabel *stabilizerExplanationLabel;
	KisSliderSpinBox *classicSizeSpinner;
	QCheckBox *classicSizePressureBox;
	KisSliderSpinBox *classicSizeMinSpinner;
	widgets::CurveWidget *classicSizeCurve;
	KisSliderSpinBox *classicOpacitySpinner;
	QCheckBox *classicOpacityPressureBox;
	KisSliderSpinBox *classicOpacityMinSpinner;
	widgets::CurveWidget *classicOpacityCurve;
	KisSliderSpinBox *classicHardnessSpinner;
	QCheckBox *classicHardnessPressureBox;
	KisSliderSpinBox *classicHardnessMinSpinner;
	widgets::CurveWidget *classicHardnessCurve;
	KisSliderSpinBox *classicSmudgingSpinner;
	KisSliderSpinBox *classicColorPickupSpinner;
	QCheckBox *classicSmudgingPressureBox;
	KisSliderSpinBox *classicSmudgingMinSpinner;
	widgets::CurveWidget *classicSmudgingCurve;
	MyPaintPage myPaintPages[MYPAINT_BRUSH_SETTINGS_COUNT];
	int generalPageIndex;
	int classicSizePageIndex;
	int classicOpacityPageIndex;
	int classicHardnessPageIndex;
	int classicSmudgePageIndex;
	int mypaintPageIndexes[MYPAINT_BRUSH_SETTINGS_COUNT];
	DP_BrushShape lastShape;
	brushes::ActiveBrush brush;
};

BrushSettingsDialog::BrushSettingsDialog(QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	setWindowTitle(tr("Brush Editor"));
	resize(QSize{800, 600});
	buildDialogUi();
	d->lastShape = DP_BRUSH_SHAPE_COUNT;
}

BrushSettingsDialog::~BrushSettingsDialog()
{
	delete d;
}


void BrushSettingsDialog::setForceEraseMode(bool forceEraseMode)
{
	d->eraseModeBox->setEnabled(!forceEraseMode);
}

void BrushSettingsDialog::setStabilizerUseBrushSampleCount(
	bool useBrushSampleCount)
{
	d->stabilizerSpinner->setEnabled(useBrushSampleCount);
	d->stabilizerExplanationLabel->setHidden(useBrushSampleCount);
}

void BrushSettingsDialog::updateUiFromActiveBrush(
	const brushes::ActiveBrush &brush)
{
	QSignalBlocker blocker{this};
	d->brush = brush;

	DP_BrushShape shape = brush.shape();
	bool shapeChanged = shape != d->lastShape;
	if(shapeChanged) {
		d->lastShape = shape;
		d->categoryWidget->clear();
		addCategory(
			tr("General"), tr("Core brush settings."), d->generalPageIndex);
		setComboBoxIndexByData(d->brushTypeCombo, int(shape));
	}

	switch(shape) {
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
	case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
	case DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND:
		if(shapeChanged) {
			addClassicCategories(shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND);
		}
		updateUiFromClassicBrush();
		break;
	case DP_BRUSH_SHAPE_MYPAINT:
		if(shapeChanged) {
			addMyPaintCategories();
		}
		updateUiFromMyPaintBrush();
		break;
	default:
		break;
	}

	if(shapeChanged) {
		d->categoryWidget->setCurrentRow(0);
		d->stackedWidget->setCurrentIndex(d->generalPageIndex);
	}
}


void BrushSettingsDialog::categoryChanged(
	QListWidgetItem *current, QListWidgetItem *)
{
	if(current) {
		d->stackedWidget->setCurrentIndex(current->data(Qt::UserRole).toInt());
	}
}


void BrushSettingsDialog::buildDialogUi()
{
	QVBoxLayout *dialogLayout = new QVBoxLayout;
	setLayout(dialogLayout);

	QHBoxLayout *splitLayout = new QHBoxLayout;
	dialogLayout->addLayout(splitLayout);

	QDialogButtonBox *buttonBox =
		new QDialogButtonBox{QDialogButtonBox::Close, this};
	dialogLayout->addWidget(buttonBox);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	d->categoryWidget = new QListWidget{this};
	splitLayout->addWidget(d->categoryWidget);
	d->categoryWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	d->categoryWidget->setSizePolicy(
		QSizePolicy::Preferred, QSizePolicy::Expanding);
	connect(
		d->categoryWidget, &QListWidget::currentItemChanged, this,
		&BrushSettingsDialog::categoryChanged);

	d->stackedWidget = new QStackedWidget{this};
	splitLayout->addWidget(d->stackedWidget);
	d->stackedWidget->setSizePolicy(
		QSizePolicy::Expanding, QSizePolicy::Expanding);

	d->generalPageIndex = d->stackedWidget->addWidget(buildGeneralPageUi());
	d->classicSizePageIndex =
		d->stackedWidget->addWidget(buildClassicSizePageUi());
	d->classicOpacityPageIndex =
		d->stackedWidget->addWidget(buildClassicOpacityPageUi());
	d->classicHardnessPageIndex =
		d->stackedWidget->addWidget(buildClassicHardnessPageUi());
	d->classicSmudgePageIndex =
		d->stackedWidget->addWidget(buildClassicSmudgingPageUi());
	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		d->mypaintPageIndexes[setting] =
			shouldIncludeMyPaintSetting(setting)
				? d->stackedWidget->addWidget(buildMyPaintPageUi(setting))
				: -1;
	}
}

QWidget *BrushSettingsDialog::buildGeneralPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QFormLayout *layout = new QFormLayout;
	widget->setLayout(layout);

	d->brushTypeCombo = new QComboBox{widget};
	layout->addRow(tr("Brush Type:"), d->brushTypeCombo);
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_pixelround"), tr("Round Pixel Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_square"), tr("Square Pixel Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("drawpile_round"), tr("Soft Round Brush"),
		int(DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND));
	d->brushTypeCombo->addItem(
		QIcon::fromTheme("draw-brush"), tr("MyPaint Brush"),
		int(DP_BRUSH_SHAPE_MYPAINT));
	connect(
		d->brushTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int index) {
			int brushType = d->brushTypeCombo->itemData(index).toInt();
			switch(brushType) {
			case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
			case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
			case DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND:
				d->brush.setActiveType(brushes::ActiveBrush::CLASSIC);
				d->brush.classic().shape = DP_BrushShape(brushType);
				break;
			case DP_BRUSH_SHAPE_MYPAINT:
				d->brush.setActiveType(brushes::ActiveBrush::MYPAINT);
				break;
			default:
				qWarning("Unknown brush type %d", brushType);
				break;
			}
			emitChange();
		});

	d->brushModeLabel = new QLabel{tr("Blend Mode:"), widget};
	d->brushModeCombo = new QComboBox{widget};
	layout->addRow(d->brushModeLabel, d->brushModeCombo);
	for(canvas::blendmode::Named m : canvas::blendmode::brushModeNames()) {
		d->brushModeCombo->addItem(m.name, int(m.mode));
	}
	connect(
		d->brushModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int index) {
			d->brush.classic().brush_mode =
				DP_BlendMode(d->brushModeCombo->itemData(index).toInt());
			emitChange();
		});

	d->eraseModeLabel = new QLabel{tr("Blend Mode:"), widget};
	d->eraseModeCombo = new QComboBox{widget};
	layout->addRow(d->eraseModeLabel, d->eraseModeCombo);
	for(canvas::blendmode::Named m : canvas::blendmode::eraserModeNames()) {
		d->eraseModeCombo->addItem(m.name, int(m.mode));
	}
	connect(
		d->eraseModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int index) {
			d->brush.classic().erase_mode =
				DP_BlendMode(d->eraseModeCombo->itemData(index).toInt());
			emitChange();
		});

	d->paintModeLabel = new QLabel{tr("Paint Mode:"), widget};
	d->paintModeCombo = new QComboBox{widget};
	layout->addRow(d->paintModeLabel, d->paintModeCombo);
	d->paintModeCombo->addItem(tr("Build-Up/Direct"), true);
	d->paintModeCombo->addItem(tr("Wash/Indirect"), false);
	connect(
		d->paintModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[this](int index) {
			if(d->brush.activeType() == brushes::ActiveBrush::CLASSIC) {
				d->brush.classic().incremental =
					d->paintModeCombo->itemData(index).toBool();
			} else {
				d->brush.myPaint().brush().incremental =
					d->paintModeCombo->itemData(index).toBool();
			}
			emitChange();
		});

	d->eraseModeBox = new QCheckBox{tr("Eraser Mode"), widget};
	layout->addRow(d->eraseModeBox);
	connect(d->eraseModeBox, &QCheckBox::stateChanged, [this](int state) {
		d->brush.classic().erase = state != Qt::Unchecked;
		d->brush.myPaint().brush().erase = state != Qt::Unchecked;
		emitChange();
	});

	d->colorPickBox =
		new QCheckBox{tr("Pick Initial Color from Layer"), widget};
	layout->addRow(d->colorPickBox);
	connect(d->colorPickBox, &QCheckBox::stateChanged, [this](int state) {
		d->brush.classic().colorpick = state != Qt::Unchecked;
		emitChange();
	});

	d->lockAlphaBox = new QCheckBox{tr("Lock Alpha (Recolor Mode)"), widget};
	layout->addRow(d->lockAlphaBox);
	connect(d->lockAlphaBox, &QCheckBox::stateChanged, [this](int state) {
		d->brush.myPaint().brush().lock_alpha = state != Qt::Unchecked;
		emitChange();
	});

	d->spacingSpinner = new KisSliderSpinBox{widget};
	layout->addRow(d->spacingSpinner);
	d->spacingSpinner->setRange(1, 999);
	d->spacingSpinner->setSingleStep(1);
	d->spacingSpinner->setExponentRatio(3.0);
	d->spacingSpinner->setPrefix(tr("Spacing: "));
	d->spacingSpinner->setSuffix(tr("%"));
	connect(
		d->spacingSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().spacing = value / 100.0;
			emitChange();
		});

	d->stabilizerSpinner = new KisSliderSpinBox{widget};
	layout->addRow(d->stabilizerSpinner);
	d->stabilizerSpinner->setRange(0, 1000);
	d->stabilizerSpinner->setPrefix(tr("Stabilizer: "));
	d->stabilizerSpinner->setSingleStep(1);
	d->stabilizerSpinner->setExponentRatio(3.0);
	connect(
		d->stabilizerSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.setStabilizerSampleCount(value);
			emitChange();
		});

	d->stabilizerExplanationLabel = new QLabel{widget};
	layout->addRow(d->stabilizerExplanationLabel);
	d->stabilizerExplanationLabel->setWordWrap(true);
	d->stabilizerExplanationLabel->setTextFormat(Qt::RichText);
	d->stabilizerExplanationLabel->setText(
		tr("Synchronizing stabilizer settings with brushes disabled."));

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicSizePageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicSizeSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSizeSpinner);
	d->classicSizeSpinner->setRange(0, 255);
	d->classicSizeSpinner->setPrefix(tr("Size: "));
	d->classicSizeSpinner->setSuffix(tr("px"));
	connect(
		d->classicSizeSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().size.max = value;
			emitChange();
		});

	d->classicSizePressureBox =
		new QCheckBox{tr("Pressure Sensitivity"), widget};
	layout->addWidget(d->classicSizePressureBox);
	connect(
		d->classicSizePressureBox, &QCheckBox::stateChanged, [this](int state) {
			d->brush.classic().size_pressure = state != Qt::Unchecked;
			emitChange();
		});

	d->classicSizeMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSizeMinSpinner);
	d->classicSizeMinSpinner->setRange(0, 255);
	d->classicSizeMinSpinner->setPrefix(tr("Minimum Size: "));
	d->classicSizeMinSpinner->setSuffix(tr("px"));
	connect(
		d->classicSizeMinSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().size.min = value;
			emitChange();
		});

	d->classicSizeCurve =
		new widgets::CurveWidget{tr("Pressure"), tr("Size"), false, widget};
	layout->addWidget(d->classicSizeCurve);
	buildClassicApplyToAllButton(d->classicSizeCurve);
	connect(
		d->classicSizeCurve, &widgets::CurveWidget::curveChanged,
		[this](const KisCubicCurve &curve) {
			d->brush.classic().setSizeCurve(curve);
			emitChange();
		});

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicOpacityPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicOpacitySpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicOpacitySpinner);
	d->classicOpacitySpinner->setRange(0, 100);
	d->classicOpacitySpinner->setPrefix(tr("Opacity: "));
	d->classicOpacitySpinner->setSuffix(tr("%"));
	connect(
		d->classicOpacitySpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().opacity.max = value / 100.0;
			emitChange();
		});

	d->classicOpacityPressureBox =
		new QCheckBox{tr("Pressure Sensitivity"), widget};
	layout->addWidget(d->classicOpacityPressureBox);
	connect(
		d->classicOpacityPressureBox, &QCheckBox::stateChanged,
		[this](int state) {
			d->brush.classic().opacity_pressure = state != Qt::Unchecked;
			emitChange();
		});

	d->classicOpacityMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicOpacityMinSpinner);
	d->classicOpacityMinSpinner->setRange(0, 100);
	d->classicOpacityMinSpinner->setPrefix(tr("Minimum Opacity: "));
	d->classicOpacityMinSpinner->setSuffix(tr("%"));
	connect(
		d->classicOpacityMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
			d->brush.classic().opacity.min = value / 100.0;
			emitChange();
		});

	d->classicOpacityCurve =
		new widgets::CurveWidget{tr("Pressure"), tr("Opacity"), false, widget};
	layout->addWidget(d->classicOpacityCurve);
	buildClassicApplyToAllButton(d->classicOpacityCurve);
	connect(
		d->classicOpacityCurve, &widgets::CurveWidget::curveChanged,
		[this](const KisCubicCurve &curve) {
			d->brush.classic().setOpacityCurve(curve);
			emitChange();
		});

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicHardnessPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicHardnessSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicHardnessSpinner);
	d->classicHardnessSpinner->setRange(0, 100);
	d->classicHardnessSpinner->setPrefix(tr("Hardness: "));
	d->classicHardnessSpinner->setSuffix(tr("%"));
	connect(
		d->classicHardnessSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().hardness.max = value / 100.0;
			emitChange();
		});

	d->classicHardnessPressureBox =
		new QCheckBox{tr("Pressure Sensitivity"), widget};
	layout->addWidget(d->classicHardnessPressureBox);
	connect(
		d->classicHardnessPressureBox, &QCheckBox::stateChanged,
		[this](int state) {
			d->brush.classic().hardness_pressure = state != Qt::Unchecked;
			emitChange();
		});

	d->classicHardnessMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicHardnessMinSpinner);
	d->classicHardnessMinSpinner->setRange(0, 100);
	d->classicHardnessMinSpinner->setPrefix(tr("Minimum Hardness: "));
	d->classicHardnessMinSpinner->setSuffix(tr("%"));
	connect(
		d->classicHardnessMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
			d->brush.classic().hardness.min = value / 100.0;
			emitChange();
		});

	d->classicHardnessCurve =
		new widgets::CurveWidget{tr("Pressure"), tr("Hardness"), false, widget};
	layout->addWidget(d->classicHardnessCurve);
	buildClassicApplyToAllButton(d->classicHardnessCurve);
	connect(
		d->classicHardnessCurve, &widgets::CurveWidget::curveChanged,
		[this](const KisCubicCurve &curve) {
			d->brush.classic().setHardnessCurve(curve);
			emitChange();
		});

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

QWidget *BrushSettingsDialog::buildClassicSmudgingPageUi()
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	d->classicSmudgingSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSmudgingSpinner);
	d->classicSmudgingSpinner->setRange(0, 100);
	d->classicSmudgingSpinner->setPrefix(tr("Smudging: "));
	d->classicSmudgingSpinner->setSuffix(tr("%"));
	connect(
		d->classicSmudgingSpinner, QOverload<int>::of(&QSpinBox::valueChanged),
		[this](int value) {
			d->brush.classic().smudge.max = value / 100.0;
			emitChange();
		});

	d->classicColorPickupSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicColorPickupSpinner);
	d->classicColorPickupSpinner->setRange(1, 32);
	d->classicColorPickupSpinner->setPrefix(tr("Color Pickup: 1/"));
	connect(
		d->classicColorPickupSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
			d->brush.classic().resmudge = value;
			emitChange();
		});

	d->classicSmudgingPressureBox =
		new QCheckBox{tr("Pressure Sensitivity"), widget};
	layout->addWidget(d->classicSmudgingPressureBox);
	connect(
		d->classicSmudgingPressureBox, &QCheckBox::stateChanged,
		[this](int state) {
			d->brush.classic().smudge_pressure = state != Qt::Unchecked;
			emitChange();
		});

	d->classicSmudgingMinSpinner = new KisSliderSpinBox{widget};
	layout->addWidget(d->classicSmudgingMinSpinner);
	d->classicSmudgingMinSpinner->setRange(0, 100);
	d->classicSmudgingMinSpinner->setPrefix(tr("Minimum Smudging: "));
	d->classicSmudgingMinSpinner->setSuffix(tr("%"));
	connect(
		d->classicSmudgingMinSpinner,
		QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
			d->brush.classic().smudge.min = value / 100.0;
			emitChange();
		});

	d->classicSmudgingCurve =
		new widgets::CurveWidget{tr("Pressure"), tr("Smudging"), false, widget};
	layout->addWidget(d->classicSmudgingCurve);
	buildClassicApplyToAllButton(d->classicSmudgingCurve);
	connect(
		d->classicSmudgingCurve, &widgets::CurveWidget::curveChanged,
		[this](const KisCubicCurve &curve) {
			d->brush.classic().setSmudgeCurve(curve);
			emitChange();
		});

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

void BrushSettingsDialog::buildClassicApplyToAllButton(
	widgets::CurveWidget *curve)
{
	QPushButton *applyToAllButton =
		new QPushButton{QIcon::fromTheme("fill-color"), tr("Apply to All")};
	applyToAllButton->setToolTip(
		tr("Set this curve for Size, Opacity, Hardness and Smudging at once."));
	curve->addButton(applyToAllButton);
	connect(applyToAllButton, &QPushButton::clicked, [=]() {
		applyCurveToAllClassicSettings(curve->curve());
		ToolMessage::showText(tr("Curve set for all settings in this brush."));
	});
}

QWidget *BrushSettingsDialog::buildMyPaintPageUi(int setting)
{
	QScrollArea *scroll = new QScrollArea{this};
	QWidget *widget = new QWidget{scroll};
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	QVBoxLayout *layout = new QVBoxLayout;
	widget->setLayout(layout);

	const MyPaintBrushSettingInfo *settingInfo =
		mypaint_brush_setting_info(MyPaintBrushSetting(setting));
	Private::MyPaintPage &page = d->myPaintPages[setting];

	page.baseValueSpinner = new KisDoubleSliderSpinBox{widget};
	layout->addWidget(page.baseValueSpinner);
	page.baseValueSpinner->setPrefix(tr("Value: "));
	page.baseValueSpinner->setRange(settingInfo->min, settingInfo->max, 2);
	connect(
		page.baseValueSpinner,
		QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		[=](double value) {
			d->brush.myPaint().settings().mappings[setting].base_value = value;
			emitChange();
		});

	if(settingInfo->constant) {
		page.constantLabel = new QLabel{tr("No brush dynamics."), widget};
		page.constantLabel->setWordWrap(true);
		layout->addWidget(page.constantLabel);
	} else {
		for(int input = 0; input < MYPAINT_BRUSH_INPUTS_COUNT; ++input) {
			layout->addWidget(buildMyPaintInputUi(setting, input, settingInfo));
		}
	}

	if(disableIndirectMyPaintSetting(setting)) {
		page.indirectLabel =
			new QLabel{tr("Not available in Indirect/Wash mode."), widget};
		layout->addWidget(page.indirectLabel);
	} else if(disableIndirectMyPaintInputs(setting)) {
		page.indirectLabel = new QLabel{
			tr("Dynamics not available in Indirect/Wash mode."), widget};
		layout->addWidget(page.indirectLabel);
	} else {
		page.indirectLabel = nullptr;
	}

	layout->addSpacerItem(
		new QSpacerItem{0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding});

	return scroll;
}

widgets::MyPaintInput *BrushSettingsDialog::buildMyPaintInputUi(
	int setting, int input, const MyPaintBrushSettingInfo *settingInfo)
{
	widgets::MyPaintInput *inputWidget = new widgets::MyPaintInput{
		getMyPaintInputTitle(input), getMyPaintInputDescription(input),
		getMyPaintSettingTitle(setting), settingInfo,
		mypaint_brush_input_info(MyPaintBrushInput(input))};
	d->myPaintPages[setting].inputs[input] = inputWidget;
	connect(
		inputWidget, &widgets::MyPaintInput::controlPointsChanged,
		[this, setting, input]() {
			widgets::MyPaintInput *otherWidget =
				d->myPaintPages[setting].inputs[input];
			brushes::MyPaintBrush &mypaint = d->brush.myPaint();
			mypaint.setCurve(setting, input, otherWidget->myPaintCurve());
			mypaint.settings().mappings[setting].inputs[input] =
				otherWidget->controlPoints();
			emitChange();
		});
	return inputWidget;
}


void BrushSettingsDialog::applyCurveToAllClassicSettings(
	const KisCubicCurve &curve)
{
	d->brush.classic().setSizeCurve(curve);
	d->brush.classic().setOpacityCurve(curve);
	d->brush.classic().setHardnessCurve(curve);
	d->brush.classic().setSmudgeCurve(curve);
	emitChange();
}


void BrushSettingsDialog::addCategory(
	const QString &text, const QString &toolTip, int pageIndex)
{
	QListWidgetItem *item = new QListWidgetItem{d->categoryWidget};
	item->setText(text);
	item->setData(Qt::UserRole, pageIndex);
	item->setToolTip(toolTip);
	d->categoryWidget->addItem(item);
}

void BrushSettingsDialog::addClassicCategories(bool withHardness)
{
	addCategory(
		tr("Size"), tr("The radius of the brush."), d->classicSizePageIndex);
	addCategory(
		tr("Opacity"),
		tr("Opaqueness of the brush, 0% is transparent, 100% fully opaque."),
		d->classicOpacityPageIndex);
	if(withHardness) {
		addCategory(
			tr("Hardness"), tr("Edge hardness, 0% is blurry, 100% is sharp."),
			d->classicHardnessPageIndex);
	}
	addCategory(
		tr("Smudging"), tr("Blending of colors on the layer being drawn on."),
		d->classicSmudgePageIndex);
}

void BrushSettingsDialog::addMyPaintCategories()
{
	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		if(shouldIncludeMyPaintSetting(setting)) {
			addCategory(
				getMyPaintSettingTitle(setting),
				getMyPaintSettingDescription(setting),
				d->mypaintPageIndexes[setting]);
		}
	}
}


void BrushSettingsDialog::updateUiFromClassicBrush()
{
	const brushes::ClassicBrush &classic = d->brush.classic();

	d->brushModeLabel->setVisible(!classic.erase);
	d->brushModeCombo->setVisible(!classic.erase);
	setComboBoxIndexByData(d->brushModeCombo, int(classic.brush_mode));

	d->eraseModeLabel->setVisible(classic.erase);
	d->eraseModeCombo->setVisible(classic.erase);
	setComboBoxIndexByData(d->eraseModeCombo, int(classic.erase_mode));

	d->eraseModeBox->setChecked(classic.erase);

	d->colorPickBox->setChecked(classic.colorpick);
	d->colorPickBox->setEnabled(
		DP_classic_brush_blend_mode(&classic) != DP_BLEND_MODE_ERASE);
	d->colorPickBox->setVisible(true);
	d->lockAlphaBox->setVisible(false);

	bool haveSmudge = classic.smudge.max > 0.0f;
	d->paintModeCombo->setCurrentIndex(
		haveSmudge || classic.incremental ? 0 : 1);
	d->paintModeCombo->setDisabled(haveSmudge);

	d->spacingSpinner->setValue(classic.spacing * 100.0 + 0.5);
	d->stabilizerSpinner->setValue(classic.stabilizerSampleCount);
	d->spacingSpinner->setVisible(true);

	d->classicSizeSpinner->setValue(classic.size.max);
	d->classicSizePressureBox->setChecked(classic.size_pressure);
	d->classicSizeMinSpinner->setValue(classic.size.min);
	d->classicSizeMinSpinner->setEnabled(classic.size_pressure);
	d->classicSizeCurve->setCurve(classic.sizeCurve());
	d->classicSizeCurve->setEnabled(classic.size_pressure);

	d->classicOpacitySpinner->setValue(classic.opacity.max * 100.0 + 0.5);
	d->classicOpacityPressureBox->setChecked(classic.opacity_pressure);
	d->classicOpacityMinSpinner->setValue(classic.opacity.min * 100.0 + 0.5);
	d->classicOpacityMinSpinner->setEnabled(classic.opacity_pressure);
	d->classicOpacityCurve->setCurve(classic.opacityCurve());
	d->classicOpacityCurve->setEnabled(classic.opacity_pressure);

	d->classicHardnessSpinner->setValue(classic.hardness.max * 100.0 + 0.5);
	d->classicHardnessPressureBox->setChecked(classic.hardness_pressure);
	d->classicHardnessMinSpinner->setValue(classic.hardness.min * 100.0 + 0.5);
	d->classicHardnessMinSpinner->setEnabled(classic.hardness_pressure);
	d->classicHardnessCurve->setCurve(classic.hardnessCurve());
	d->classicHardnessCurve->setEnabled(classic.hardness_pressure);

	d->classicSmudgingSpinner->setValue(classic.smudge.max * 100.0 + 0.5);
	d->classicColorPickupSpinner->setValue(classic.resmudge);
	d->classicSmudgingPressureBox->setChecked(classic.smudge_pressure);
	d->classicSmudgingMinSpinner->setValue(classic.smudge.min * 100.0 + 0.5);
	d->classicSmudgingMinSpinner->setEnabled(classic.smudge_pressure);
	d->classicSmudgingCurve->setCurve(classic.smudgeCurve());
	d->classicSmudgingCurve->setEnabled(classic.smudge_pressure);
}

void BrushSettingsDialog::updateUiFromMyPaintBrush()
{
	const DP_MyPaintBrush &brush = d->brush.myPaint().constBrush();

	d->brushModeLabel->setVisible(false);
	d->brushModeCombo->setVisible(false);
	d->colorPickBox->setVisible(false);
	d->spacingSpinner->setVisible(false);

	d->paintModeCombo->setCurrentIndex(brush.incremental ? 0 : 1);
	d->eraseModeBox->setChecked(brush.erase);
	d->lockAlphaBox->setChecked(brush.lock_alpha);
	d->lockAlphaBox->setVisible(true);
	d->stabilizerSpinner->setValue(d->brush.myPaint().stabilizerSampleCount());

	for(int setting = 0; setting < MYPAINT_BRUSH_SETTINGS_COUNT; ++setting) {
		if(shouldIncludeMyPaintSetting(setting)) {
			updateMyPaintSettingPage(setting);
		}
	}
}

void BrushSettingsDialog::updateMyPaintSettingPage(int setting)
{
	brushes::MyPaintBrush &mypaint = d->brush.myPaint();
	const DP_MyPaintSettings &settings = mypaint.constSettings();

	const DP_MyPaintMapping &mapping = settings.mappings[setting];
	Private::MyPaintPage &page = d->myPaintPages[setting];
	page.baseValueSpinner->setValue(mapping.base_value);

	bool incremental = mypaint.constBrush().incremental;
	bool disableIndirectSetting = disableIndirectMyPaintSetting(setting);
	bool disableIndirectInputs = disableIndirectMyPaintInputs(setting);
	if(disableIndirectSetting) {
		page.baseValueSpinner->setVisible(incremental);
	}

	const MyPaintBrushSettingInfo *settingInfo =
		mypaint_brush_setting_info(MyPaintBrushSetting(setting));
	if(settingInfo->constant) {
		if(disableIndirectSetting || disableIndirectInputs) {
			page.constantLabel->setVisible(incremental);
		}
	} else {
		for(int input = 0; input < MYPAINT_BRUSH_INPUTS_COUNT; ++input) {
			widgets::MyPaintInput *inputWidget = page.inputs[input];
			brushes::MyPaintCurve curve = mypaint.getCurve(setting, input);
			if(curve.isValid()) {
				inputWidget->setMyPaintCurve(curve);
			} else {
				inputWidget->setControlPoints(mapping.inputs[input]);
				mypaint.setCurve(setting, input, inputWidget->myPaintCurve());
			}
			if(disableIndirectSetting || disableIndirectInputs) {
				inputWidget->setVisible(incremental);
			}
		}
	}

	if(disableIndirectSetting || disableIndirectInputs) {
		page.indirectLabel->setVisible(!incremental);
	}
}

void BrushSettingsDialog::emitChange()
{
	emit brushSettingsChanged(d->brush);
}


void BrushSettingsDialog::setComboBoxIndexByData(QComboBox *combo, int data)
{
	int count = combo->count();
	for(int i = 0; i < count; ++i) {
		if(combo->itemData(i).toInt() == data) {
			combo->setCurrentIndex(i);
			break;
		}
	}
}

bool BrushSettingsDialog::shouldIncludeMyPaintSetting(int setting)
{
	switch(setting) {
	// The brush settings dialog is not the place to set the brush color.
	case MYPAINT_BRUSH_SETTING_COLOR_H:
	case MYPAINT_BRUSH_SETTING_COLOR_S:
	case MYPAINT_BRUSH_SETTING_COLOR_V:
	// This is for storing the color in the brush, which we don't do.
	case MYPAINT_BRUSH_SETTING_RESTORE_COLOR:
	// Spectral painting is not supported in Drawpile.
	case MYPAINT_BRUSH_SETTING_PAINT_MODE:
	// This is a stabilizer, but Drawpile has its own one.
	case MYPAINT_BRUSH_SETTING_SLOW_TRACKING:
		return false;
	default:
		return true;
	}
}

bool BrushSettingsDialog::disableIndirectMyPaintSetting(int setting)
{
	switch(setting) {
	// Opacity linearization is supposed to compensate for direct drawing mode,
	// in indirect mode its behavior is just really wrong, so we disable it.
	case MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE:
	// Indirect mode can't smudge, affect alpha or change color along the way.
	case MYPAINT_BRUSH_SETTING_SMUDGE:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH:
	case MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG:
	case MYPAINT_BRUSH_SETTING_ERASER:
	case MYPAINT_BRUSH_SETTING_LOCK_ALPHA:
	case MYPAINT_BRUSH_SETTING_COLORIZE:
	case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH_LOG:
	case MYPAINT_BRUSH_SETTING_SMUDGE_BUCKET:
	case MYPAINT_BRUSH_SETTING_SMUDGE_TRANSPARENCY:
	case MYPAINT_BRUSH_SETTING_POSTERIZE:
	case MYPAINT_BRUSH_SETTING_POSTERIZE_NUM:
		return true;
	default:
		return false;
	}
}

bool BrushSettingsDialog::disableIndirectMyPaintInputs(int setting)
{
	switch(setting) {
	// Indirect mode can't change color along the way.
	case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H:
	case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L:
	case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S:
	case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V:
	case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S:
		return true;
	default:
		return false;
	}
}

}

// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/brush.h>
#include <dpmsg/blend_mode.h>
}
#include "desktop/main.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/brushes/brush.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_brushdock.h"
#include <QActionGroup>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QStandardItemModel>
#include <mypaint-brush-settings.h>
#include <optional>

namespace tools {

static const int BRUSH_COUNT = 6; // Last is the dedicated eraser slot
static const int ERASER_SLOT = 5; // Index of the dedicated erser slot

struct BrushSettings::Private {
	Ui_BrushDock ui;

	QStandardItemModel *blendModes, *eraseModes;

	brushes::ActiveBrush brushSlots[BRUSH_COUNT];
	std::optional<brushes::ActiveBrush> lastPresets[BRUSH_COUNT];
	widgets::GroupedToolButton *brushSlotButton[BRUSH_COUNT];
	QWidget *brushSlotWidget = nullptr;

	QActionGroup *stabilizationModeGroup;
	QAction *stabilizerAction;
	QAction *smoothingAction;
	QAction *finishStrokesAction;
	QAction *useBrushSampleCountAction;

	int current = 0;
	int previousNonEraser = 0;
	BrushMode previousBrushMode = UnknownMode;

	qreal quickAdjust1 = 0.0;

	bool finishStrokes = true;
	bool useBrushSampleCount = true;
	int globalSmoothing;

	bool shareBrushSlotColor = false;
	bool updateInProgress = false;
	bool myPaintAllowed = true;
	bool compatibilityMode = false;

	inline brushes::ActiveBrush &currentBrush()
	{
		Q_ASSERT(current >= 0 && current < BRUSH_COUNT);
		return brushSlots[current];
	}

	inline bool currentIsMyPaint()
	{
		return currentBrush().activeType() == brushes::ActiveBrush::MYPAINT;
	}

	inline brushes::StabilizationMode stabilizationMode()
	{
		if(smoothingAction->isChecked()) {
			return brushes::Smoothing;
		} else {
			return brushes::Stabilizer;
		}
	}

	Private(BrushSettings *b)
		: globalSmoothing{b->controller()->globalSmoothing()}
	{
		blendModes = new QStandardItemModel(0, 1, b);
		for(const canvas::blendmode::Named &m :
			canvas::blendmode::brushModeNames()) {
			QStandardItem *item = new QStandardItem(m.name);
			item->setData(int(m.mode), Qt::UserRole);
			blendModes->appendRow(item);
		}

		eraseModes = new QStandardItemModel(0, 1, b);
		for(const canvas::blendmode::Named &m :
			canvas::blendmode::eraserModeNames()) {
			QStandardItem *item = new QStandardItem(m.name);
			item->setData(int(m.mode), Qt::UserRole);
			eraseModes->appendRow(item);
		}
	}
};

BrushSettings::BrushSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, d(new Private(this))
{
}

BrushSettings::~BrushSettings()
{
	delete d;
}

QWidget *BrushSettings::createUiWidget(QWidget *parent)
{
	d->brushSlotWidget = new QWidget(parent);
	auto brushSlotWidgetLayout = new QHBoxLayout;
	brushSlotWidgetLayout->setSpacing(0);
	brushSlotWidgetLayout->setContentsMargins(0, 0, 0, 0);

	d->brushSlotWidget->setLayout(brushSlotWidgetLayout);

	widgets::GroupedToolButton *brushSettingsDialogButton =
		new widgets::GroupedToolButton(
			widgets::GroupedToolButton::NotGrouped, d->brushSlotWidget);
	brushSlotWidgetLayout->addWidget(brushSettingsDialogButton);
	brushSettingsDialogButton->setIcon(QIcon::fromTheme("configure"));
	brushSettingsDialogButton->setToolTip(tr("Brush Settings"));
	connect(
		brushSettingsDialogButton, &widgets::GroupedToolButton::clicked, this,
		&BrushSettings::brushSettingsDialogRequested);

	brushSlotWidgetLayout->addStretch();

	for(int i = 0; i < BRUSH_COUNT; ++i) {
		d->brushSlotButton[i] = new widgets::GroupedToolButton(
			widgets::GroupedToolButton::GroupCenter, d->brushSlotWidget);
		d->brushSlotButton[i]->setCheckable(true);
		d->brushSlotButton[i]->setAutoExclusive(true);
		d->brushSlotButton[i]->setText(QString::number(i + 1));
		brushSlotWidgetLayout->addWidget(d->brushSlotButton[i]);

		connect(
			d->brushSlotButton[i], &QToolButton::clicked, this, [this, i]() {
				selectBrushSlot(i);
			});
	}

	brushSlotWidgetLayout->addStretch();

	d->brushSlotButton[0]->setGroupPosition(
		widgets::GroupedToolButton::GroupLeft);
	d->brushSlotButton[BRUSH_COUNT - 1]->setGroupPosition(
		widgets::GroupedToolButton::GroupRight);
	d->brushSlotButton[ERASER_SLOT]->setIcon(QIcon::fromTheme("draw-eraser"));

	QWidget *widget = new QWidget(parent);
	d->ui.setupUi(widget);

	// The blend mode combo is ever so slightly higher than the buttons, so
	// hiding it causes some jerking normally, so we'll tell it to retain its
	// size. The space it's taking up is supposed to be empty anyway.
	utils::setWidgetRetainSizeWhenHidden(d->ui.blendmode, true);
	// It's really hard to see that the indirect mode button is disabled when
	// smudging is turned on, so we just hide it altogether and keep the space.
	utils::setWidgetRetainSizeWhenHidden(d->ui.modeIncremental, true);

	// Exponential sliders for easier picking of small values.
	d->ui.brushsizeBox->setExponentRatio(2.0);
	d->ui.brushspacingBox->setExponentRatio(3.0);
	d->ui.stabilizerBox->setExponentRatio(3.0);

	QMenu *stabilizerMenu = new QMenu{d->ui.stabilizerButton};
	d->stabilizationModeGroup = new QActionGroup{stabilizerMenu};
	d->stabilizerAction =
		stabilizerMenu->addAction(tr("Time-Based Stabilizer"));
	d->smoothingAction = stabilizerMenu->addAction(tr("Average Smoothing"));
	d->stabilizerAction->setStatusTip(tr(
		"Slows down the stroke and stabilizes it over time. Can produce very "
		"smooth results, but may feel sluggish."));
	d->smoothingAction->setStatusTip(
		tr("Simply averages inputs to get a smoother result. Faster than the "
		   "time-based stabilizer, but not as smooth."));
	d->stabilizerAction->setCheckable(true);
	d->smoothingAction->setCheckable(true);
	d->stabilizationModeGroup->addAction(d->stabilizerAction);
	d->stabilizationModeGroup->addAction(d->smoothingAction);
	stabilizerMenu->addSeparator();
	d->finishStrokesAction = stabilizerMenu->addAction(tr("Finish Strokes"));
	d->finishStrokesAction->setStatusTip(tr(
		"Draws stabilized strokes to the end, rather than cutting them off."));
	d->finishStrokesAction->setCheckable(true);
	d->useBrushSampleCountAction =
		stabilizerMenu->addAction(tr("Synchronize With Brush"));
	d->useBrushSampleCountAction->setStatusTip(tr(
		"Makes the stabilizer a brush setting, like in MyPaint, rather than an "
		"independent setting, like in Krita."));
	d->useBrushSampleCountAction->setCheckable(true);
	d->ui.stabilizerButton->setMenu(stabilizerMenu);

	// Outside communication
	connect(
		d->ui.preview, SIGNAL(requestColorChange()), parent,
		SLOT(changeForegroundColor()));

	// Internal updates
	connect(
		d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::changeSizeSetting);
	connect(
		d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::changeRadiusLogarithmicSetting);

	connect(
		d->ui.blendmode, QOverload<int>::of(&QComboBox::activated), this,
		&BrushSettings::selectBlendMode);
	connect(
		d->ui.modeEraser, &QToolButton::clicked, this,
		&BrushSettings::setEraserMode);

	connect(
		d->ui.hardedgeMode, &QToolButton::clicked, this,
		&BrushSettings::changeBrushType);
	connect(
		d->ui.squareMode, &QToolButton::clicked, this,
		&BrushSettings::changeBrushType);
	connect(
		d->ui.softedgeMode, &QToolButton::clicked, this,
		&BrushSettings::changeBrushType);
	connect(
		d->ui.mypaintMode, &QToolButton::clicked, this,
		&BrushSettings::changeBrushType);

	connect(
		d->ui.brushsizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureSize, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.opacityBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureOpacity, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.hardnessBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureHardness, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.smudgingBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.pressureSmudging, &QToolButton::toggled, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.radiusLogarithmicBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::updateFromUi);
	connect(
		d->ui.colorpickupBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.brushspacingBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &BrushSettings::updateFromUi);
	connect(
		d->ui.gainBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.stabilizerBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.smoothingBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.modeIncremental, &QToolButton::clicked, this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.modeColorpick, &QToolButton::clicked, this,
		&BrushSettings::updateFromUi);
	connect(
		d->ui.modeLockAlpha, &QToolButton::clicked, this,
		&BrushSettings::updateFromUi);

	connect(
		d->stabilizerAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->smoothingAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->stabilizerAction, &QAction::triggered, this,
		&BrushSettings::updateStabilizationSettingVisibility);
	connect(
		d->smoothingAction, &QAction::triggered, this,
		&BrushSettings::updateStabilizationSettingVisibility);

	connect(
		d->finishStrokesAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);
	connect(
		d->useBrushSampleCountAction, &QAction::triggered, this,
		&BrushSettings::updateFromUi);

	connect(
		d->ui.noPermissionLabel, &QLabel::linkActivated, this,
		[this](const QString &link) {
			QSignalBlocker blocker{d->ui.modeIncremental};
			if(link == QStringLiteral("#inc")) {
				d->ui.modeIncremental->setChecked(true);
			} else if(link == QStringLiteral("#opa")) {
				d->ui.pressureOpacity->setChecked(false);
			} else {
				qWarning("Unknown link '%s' clicked", qUtf8Printable(link));
			}
			updateFromUi();
		});

	// By default, the docker shows all settings at once, making it bigger on
	// startup and messing up the application layout. This will hide the excess
	// UI elements and bring the docker to a reasonable size to begin with.
	adjustSettingVisibilities(false, false);

	return widget;
}

QWidget *BrushSettings::getHeaderWidget()
{
	return d->brushSlotWidget;
}

bool BrushSettings::isLocked()
{
	return getLock() != Lock::None;
}

BrushSettings::Lock BrushSettings::getLock()
{
	if(d->compatibilityMode) {
		if(d->currentIsMyPaint()) {
			return Lock::MyPaintCompat;
		} else {
			const brushes::ClassicBrush &classic = d->currentBrush().classic();
			if(!classic.incremental &&
			   classic.opacity_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE) {
				return Lock::IndirectCompat;
			}
		}
	} else if(!d->myPaintAllowed && d->currentIsMyPaint()) {
		return Lock::MyPaintPermission;
	}
	return Lock::None;
}

QString BrushSettings::getLockDescription(Lock lock)
{
	switch(lock) {
	case Lock::MyPaintPermission:
		return tr("You don't have permission to use MyPaint brushes.");
	case Lock::MyPaintCompat:
		return tr("This session is hosted with Drawpile 2.1, MyPaint brushes "
				  "are unavailable.");
	case Lock::IndirectCompat:
		return tr(
			"This session is hosted with Drawpile 2.1, Indirect/Wash Mode with "
			"opacity dynamics is unavailable. <a href=\"#inc\">Click here to "
			"switch to Direct/Build-Up Mode</a> or <a href=\"#opa\">here to "
			"disable opacity dynamics</a>.");
	default:
		return QString{};
	}
}

void BrushSettings::setMyPaintAllowed(bool myPaintAllowed)
{
	d->myPaintAllowed = myPaintAllowed;
	updateUi();
}

void BrushSettings::setCompatibilityMode(bool compatibilityMode)
{
	d->compatibilityMode = compatibilityMode;
	canvas::blendmode::setCompatibilityMode(d->blendModes, compatibilityMode);
	updateUi();
}

void BrushSettings::setShareBrushSlotColor(bool sameColor)
{
	d->shareBrushSlotColor = sameColor;
	for(int i = 0; i < BRUSH_COUNT; ++i) {
		d->brushSlotButton[i]->setColorSwatch(
			sameColor ? QColor() : d->brushSlots[i].qColor());
	}
}

void BrushSettings::setCurrentBrush(brushes::ActiveBrush brush)
{
	brush.setQColor(d->currentBrush().qColor());

	brushes::ActiveBrush &currentBrush = d->currentBrush();
	brushes::ActiveBrush::ActiveType activeType = brush.activeType();
	switch(activeType) {
	case brushes::ActiveBrush::CLASSIC:
		if(d->current == ERASER_SLOT) {
			brush.classic().erase = true;
		}
		currentBrush.setClassic(brush.classic());
		break;
	case brushes::ActiveBrush::MYPAINT:
		if(d->current == ERASER_SLOT) {
			brush.myPaint().brush().erase = true;
		}
		currentBrush.setMyPaint(brush.myPaint());
		break;
	default:
		qWarning("Invalid brush type %d", static_cast<int>(brush.activeType()));
		return;
	}

	currentBrush.setActiveType(activeType);
	d->lastPresets[d->current] = brush;
	updateUi();
}

brushes::ActiveBrush BrushSettings::currentBrush() const
{
	return d->currentBrush();
}

int BrushSettings::currentBrushSlot() const
{
	return d->current;
}

void BrushSettings::selectBrushSlot(int i)
{
	if(i < 0 || i >= BRUSH_COUNT) {
		qWarning("selectBrushSlot(%d): invalid slot index!", i);
		return;
	}
	const int previousSlot = d->current;

	d->brushSlotButton[i]->setChecked(true);
	d->current = i;

	if(!d->shareBrushSlotColor)
		emit colorChanged(d->currentBrush().qColor());

	updateUi();

	if((previousSlot == ERASER_SLOT) != (i == ERASER_SLOT))
		emit eraseModeChanged(i == ERASER_SLOT);
}

void BrushSettings::toggleEraserMode()
{
	if(!isCurrentEraserSlot()) {
		// Eraser mode is fixed in dedicated eraser slot
		setEraserMode(!d->currentBrush().isEraser());
	}
}

void BrushSettings::toggleRecolorMode()
{
	if(!isCurrentEraserSlot()) {
		brushes::ActiveBrush &brush = d->currentBrush();
		switch(brush.activeType()) {
		case brushes::ActiveBrush::CLASSIC: {
			brushes::ClassicBrush &cb = brush.classic();
			if(cb.brush_mode == DP_BLEND_MODE_RECOLOR) {
				cb.brush_mode = DP_BLEND_MODE_NORMAL;
			} else {
				cb.brush_mode = DP_BLEND_MODE_RECOLOR;
			}
			break;
		}
		case brushes::ActiveBrush::MYPAINT: {
			DP_MyPaintBrush &mpb = brush.myPaint().brush();
			mpb.lock_alpha = !mpb.lock_alpha;
			break;
		}
		default:
			break;
		}
		updateUi();
	}
}

void BrushSettings::reloadPreset()
{
	const std::optional<brushes::ActiveBrush> &opt = d->lastPresets[d->current];
	if(opt.has_value()) {
		setCurrentBrush(opt.value());
	}
}

void BrushSettings::setEraserMode(bool erase)
{
	Q_ASSERT(!isCurrentEraserSlot());

	brushes::ActiveBrush &brush = d->currentBrush();
	brushes::ClassicBrush &classic = brush.classic();
	classic.erase = erase;
	brush.myPaint().brush().erase = erase;

	if(!canvas::blendmode::isValidBrushMode(classic.brush_mode)) {
		qWarning(
			"setEraserMode(%d): wrong brush mode %d", erase,
			int(classic.brush_mode));
		classic.brush_mode = DP_BLEND_MODE_NORMAL;
	}
	if(!canvas::blendmode::isValidEraseMode(classic.erase_mode)) {
		qWarning(
			"setEraserMode(%d): wrong erase mode %d", erase,
			int(classic.erase_mode));
		classic.erase_mode = DP_BLEND_MODE_ERASE;
	}

	updateUi();
}

void BrushSettings::selectEraserSlot(bool eraser)
{
	if(eraser) {
		if(!isCurrentEraserSlot()) {
			d->previousNonEraser = d->current;
			selectBrushSlot(ERASER_SLOT);
		}
	} else {
		if(isCurrentEraserSlot()) {
			selectBrushSlot(d->previousNonEraser);
		}
	}
}

void BrushSettings::swapWithSlot(int i)
{
	Q_ASSERT(i >= 0);
	Q_ASSERT(i < ERASER_SLOT);
	if(i != d->current && !isCurrentEraserSlot()) {
		std::swap(d->brushSlots[d->current], d->brushSlots[i]);
		std::swap(d->lastPresets[d->current], d->lastPresets[i]);
		updateUi();
	}
}

void BrushSettings::setGlobalSmoothing(int smoothing)
{
	QSignalBlocker blocker{d->ui.smoothingBox};
	d->ui.smoothingBox->setValue(
		d->ui.smoothingBox->value() - d->globalSmoothing + smoothing);
	d->globalSmoothing = smoothing;
}

bool BrushSettings::isCurrentEraserSlot() const
{
	return d->current == ERASER_SLOT;
}

void BrushSettings::changeBrushType()
{
	updateFromUiWith(false);
	updateUi();
}

void BrushSettings::changeSizeSetting(int size)
{
	if(d->currentBrush().activeType() == brushes::ActiveBrush::CLASSIC) {
		emit pixelSizeChanged(size);
	}
}

void BrushSettings::changeRadiusLogarithmicSetting(int radiusLogarithmic)
{
	if(d->currentBrush().activeType() == brushes::ActiveBrush::MYPAINT) {
		emit pixelSizeChanged(radiusLogarithmicToPixelSize(radiusLogarithmic));
	}
}

void BrushSettings::selectBlendMode(int modeIndex)
{
	const DP_BlendMode mode = DP_BlendMode(d->ui.blendmode->model()
											   ->index(modeIndex, 0)
											   .data(Qt::UserRole)
											   .toInt());
	brushes::ClassicBrush &classic = d->currentBrush().classic();
	if(classic.erase) {
		classic.erase_mode = mode;
	} else {
		classic.brush_mode = mode;
	}
	updateUi();
}

static void setSliderFromMyPaintSetting(
	KisSliderSpinBox *slider, const DP_MyPaintSettings &myPaintSettings,
	MyPaintBrushSetting setting, bool adjustMinToZero = false)
{
	qreal value = myPaintSettings.mappings[setting].base_value;
	// The radius setting starts at -2, but we don't want to present it that way
	// to the user and confuse them horribly. So we offset by the minimum.
	if(adjustMinToZero) {
		value -= mypaint_brush_setting_info(setting)->min;
	}
	slider->setValue(qRound(value * 100.0));
}

static void setMyPaintSettingFromSlider(
	const KisSliderSpinBox *slider, DP_MyPaintSettings &myPaintSettings,
	MyPaintBrushSetting setting, bool adjustMinToZero = false)
{
	float value = slider->value() / 100.0f;
	if(adjustMinToZero) {
		value += mypaint_brush_setting_info(setting)->min;
	}
	myPaintSettings.mappings[setting].base_value = value;
}

void BrushSettings::updateUi()
{
	// Update the UI to match the currently selected brush
	if(d->updateInProgress)
		return;

	d->updateInProgress = true;
	d->finishStrokesAction->setChecked(d->finishStrokes);
	d->useBrushSampleCountAction->setChecked(d->useBrushSampleCount);

	const brushes::ActiveBrush &brush = d->currentBrush();
	const brushes::ClassicBrush &classic = brush.classic();
	const brushes::MyPaintBrush &myPaint = brush.myPaint();

	// Select brush type
	const bool mypaintmode =
		brush.activeType() == brushes::ActiveBrush::MYPAINT;
	const bool softmode =
		mypaintmode || classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;

	if(mypaintmode) {
		d->ui.mypaintMode->setChecked(true);
	} else {
		switch(classic.shape) {
		case DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND:
			d->ui.hardedgeMode->setChecked(true);
			break;
		case DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE:
			d->ui.squareMode->setChecked(true);
			break;
		default:
			d->ui.softedgeMode->setChecked(true);
			break;
		}
	}

	emit subpixelModeChanged(getSubpixelMode(), isSquare());
	adjustSettingVisibilities(softmode, mypaintmode);

	// Show correct blending mode
	d->ui.blendmode->setModel(classic.erase ? d->eraseModes : d->blendModes);
	d->ui.modeEraser->setChecked(brush.isEraser());
	d->ui.modeEraser->setEnabled(d->current != ERASER_SLOT);

	int mode = DP_classic_brush_blend_mode(&classic);
	d->ui.modeColorpick->setEnabled(mode != DP_BLEND_MODE_ERASE);
	for(int i = 0; i < d->ui.blendmode->model()->rowCount(); ++i) {
		if(d->ui.blendmode->model()->index(i, 0).data(Qt::UserRole) == mode) {
			d->ui.blendmode->setCurrentIndex(i);
			break;
		}
	}

	// Set UI elements that are distinct between classic and MyPaint brushes
	// unconditionally, the ones that are shared only for the active type.
	// This is so that when loading a brush from settings, the hidden elements
	// don't get left in their default state.
	d->ui.brushsizeBox->setValue(classic.size.max);
	d->ui.pressureSize->setChecked(
		classic.size_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.pressureOpacity->setChecked(
		classic.opacity_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.smudgingBox->setValue(qRound(classic.smudge.max * 100.0));
	d->ui.pressureSmudging->setChecked(
		classic.smudge_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	d->ui.colorpickupBox->setValue(classic.resmudge);
	d->ui.brushspacingBox->setValue(qRound(classic.spacing * 100.0));
	d->ui.modeColorpick->setChecked(classic.colorpick);
	if(softmode) {
		d->ui.pressureHardness->setChecked(
			classic.hardness_dynamic.type != DP_CLASSIC_BRUSH_DYNAMIC_NONE);
	}

	const DP_MyPaintSettings &myPaintSettings = myPaint.constSettings();
	setSliderFromMyPaintSetting(
		d->ui.radiusLogarithmicBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, true);
	setSliderFromMyPaintSetting(
		d->ui.gainBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG);
	d->ui.modeLockAlpha->setChecked(myPaint.constBrush().lock_alpha);

	if(mypaintmode) {
		setSliderFromMyPaintSetting(
			d->ui.opacityBox, myPaintSettings, MYPAINT_BRUSH_SETTING_OPAQUE);
		setSliderFromMyPaintSetting(
			d->ui.hardnessBox, myPaintSettings, MYPAINT_BRUSH_SETTING_HARDNESS);
		d->ui.modeIncremental->setChecked(myPaint.constBrush().incremental);
		bool canUseIncrementalMode = !isLocked();
		d->ui.modeIncremental->setEnabled(canUseIncrementalMode);
		d->ui.modeIncremental->setVisible(canUseIncrementalMode);
	} else {
		d->ui.opacityBox->setValue(qRound(classic.opacity.max * 100.0));
		d->ui.hardnessBox->setValue(qRound(classic.hardness.max * 100.0));
		d->ui.modeIncremental->setChecked(classic.incremental);
		// Smudging only works right in incremental mode
		bool canUseIncrementalMode;
		if(classic.smudge.max == 0.0) {
			Lock lock = getLock();
			canUseIncrementalMode =
				lock == Lock::None || lock == Lock::IndirectCompat;
		} else {
			canUseIncrementalMode = false;
		}
		d->ui.modeIncremental->setEnabled(canUseIncrementalMode);
		d->ui.modeIncremental->setVisible(canUseIncrementalMode);
	}

	if(d->useBrushSampleCount) {
		brushes::StabilizationMode stabilizationMode =
			brush.stabilizationMode();
		d->stabilizerAction->setChecked(
			stabilizationMode == brushes::Stabilizer);
		d->smoothingAction->setChecked(stabilizationMode == brushes::Smoothing);
		d->ui.stabilizerBox->setValue(brush.stabilizerSampleCount());
		d->ui.smoothingBox->setValue(brush.smoothing() + d->globalSmoothing);
	}

	// Communicate the current size of the brush cursor to the outside. These
	// functions check for their applicability themselves, so we just call both.
	changeSizeSetting(d->ui.brushsizeBox->value());
	changeRadiusLogarithmicSetting(d->ui.radiusLogarithmicBox->value());
	updateStabilizationSettingVisibility();
	emitBrushModeChanged();

	d->updateInProgress = false;
	d->ui.preview->setBrush(d->currentBrush());
	pushSettings();
}

void BrushSettings::updateFromUi()
{
	updateFromUiWith(true);
}

void BrushSettings::updateFromUiWith(bool updateShared)
{
	if(d->updateInProgress)
		return;

	d->finishStrokes = d->finishStrokesAction->isChecked();
	d->useBrushSampleCount = d->useBrushSampleCountAction->isChecked();

	bool mypaintmode = d->ui.mypaintMode->isChecked();
	d->currentBrush().setActiveType(
		mypaintmode ? brushes::ActiveBrush::MYPAINT
					: brushes::ActiveBrush::CLASSIC);

	// Copy changes from the UI to the brush properties object,
	// then update the brush
	brushes::ActiveBrush &brush = d->currentBrush();
	brushes::ClassicBrush &classic = brush.classic();
	brushes::MyPaintBrush &myPaint = brush.myPaint();

	if(d->ui.hardedgeMode->isChecked())
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_ROUND;
	else if(d->ui.squareMode->isChecked())
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
	else
		classic.shape = DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;

	classic.size.max = d->ui.brushsizeBox->value();
	classic.size_dynamic.type = d->ui.pressureSize->isChecked()
									? classic.lastSizeDynamicType()
									: DP_CLASSIC_BRUSH_DYNAMIC_NONE;

	classic.smudge.max = d->ui.smudgingBox->value() / 100.0;
	classic.smudge_dynamic.type = d->ui.pressureSmudging->isChecked()
									  ? classic.lastSmudgeDynamicType()
									  : DP_CLASSIC_BRUSH_DYNAMIC_NONE;
	classic.resmudge = d->ui.colorpickupBox->value();

	classic.spacing = d->ui.brushspacingBox->value() / 100.0;
	classic.colorpick = d->ui.modeColorpick->isChecked();

	DP_BlendMode mode =
		DP_BlendMode(d->ui.blendmode->currentData(Qt::UserRole).toInt());
	if(classic.erase && canvas::blendmode::isValidEraseMode(mode)) {
		classic.erase_mode = mode;
	} else if(canvas::blendmode::isValidBrushMode(mode)) {
		classic.brush_mode = mode;
	}

	DP_MyPaintSettings &myPaintSettings = myPaint.settings();
	setMyPaintSettingFromSlider(
		d->ui.radiusLogarithmicBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, true);
	setMyPaintSettingFromSlider(
		d->ui.gainBox, myPaintSettings,
		MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG);
	myPaint.brush().lock_alpha = d->ui.modeLockAlpha->isChecked();

	// We want to keep MyPaint and classic brush opacity and hardness separate,
	// since they work so differently from each other. So this will be false
	// when switching types as to not overwrite the values with each other.
	if(updateShared) {
		if(mypaintmode) {
			setMyPaintSettingFromSlider(
				d->ui.opacityBox, myPaintSettings,
				MYPAINT_BRUSH_SETTING_OPAQUE);
			setMyPaintSettingFromSlider(
				d->ui.hardnessBox, myPaintSettings,
				MYPAINT_BRUSH_SETTING_HARDNESS);
			myPaint.brush().incremental = d->ui.modeIncremental->isChecked();
			bool canUseIncrementalMode = !isLocked();
			d->ui.modeIncremental->setEnabled(canUseIncrementalMode);
			d->ui.modeIncremental->setVisible(canUseIncrementalMode);
		} else {
			classic.opacity.max = d->ui.opacityBox->value() / 100.0;
			classic.opacity_dynamic.type =
				d->ui.pressureOpacity->isChecked()
					? classic.lastOpacityDynamicType()
					: DP_CLASSIC_BRUSH_DYNAMIC_NONE;
			classic.hardness.max = d->ui.hardnessBox->value() / 100.0;
			if(classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND) {
				classic.hardness_dynamic.type =
					d->ui.pressureHardness->isChecked()
						? classic.lastHardnessDynamicType()
						: DP_CLASSIC_BRUSH_DYNAMIC_NONE;
			}
			classic.incremental = d->ui.modeIncremental->isChecked();
			// Smudging only works right in incremental mode
			bool canUseIncrementalMode;
			if(classic.smudge.max == 0.0) {
				Lock lock = getLock();
				canUseIncrementalMode =
					lock == Lock::None || lock == Lock::IndirectCompat;
			} else {
				canUseIncrementalMode = false;
			}
			d->ui.modeIncremental->setEnabled(canUseIncrementalMode);
			d->ui.modeIncremental->setVisible(canUseIncrementalMode);
		}
	}

	if(d->useBrushSampleCount) {
		brush.setStabilizationMode(d->stabilizationMode());
		brush.setStabilizerSampleCount(d->ui.stabilizerBox->value());
		brush.setSmoothing(d->ui.smoothingBox->value() - d->globalSmoothing);
	}

	d->ui.preview->setBrush(brush);

	pushSettings();

	adjustSettingVisibilities(
		mypaintmode || classic.shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND,
		mypaintmode);
	emitBrushModeChanged();
}

void BrushSettings::updateStabilizationSettingVisibility()
{
	brushes::StabilizationMode stabilizationMode = d->stabilizationMode();
	d->ui.stabilizerBox->setVisible(false);
	d->ui.smoothingBox->setVisible(false);
	d->ui.stabilizerBox->setVisible(stabilizationMode == brushes::Stabilizer);
	d->ui.smoothingBox->setVisible(stabilizationMode == brushes::Smoothing);
}

void BrushSettings::adjustSettingVisibilities(bool softmode, bool mypaintmode)
{
	utils::ScopedUpdateDisabler disabler(getUi());
	Lock lock = getLock();
	bool locked = lock != Lock::None;
	QPair<QWidget *, bool> widgetVisibilities[] = {
		{d->ui.modeColorpick, !locked && !mypaintmode},
		{d->ui.modeLockAlpha, !locked && mypaintmode},
		{d->ui.modeEraser, !locked},
		{d->ui.modeIncremental,
		 d->ui.modeIncremental->isEnabled() &&
			 (!locked || (!mypaintmode && lock == Lock::IndirectCompat))},
		{d->ui.blendmode, !locked && !mypaintmode},
		{d->ui.pressureHardness, !locked && softmode && !mypaintmode},
		{d->ui.hardnessBox, !locked && softmode},
		{d->ui.brushsizeBox, !locked && !mypaintmode},
		{d->ui.pressureSize, !locked && !mypaintmode},
		{d->ui.radiusLogarithmicBox, !locked && mypaintmode},
		{d->ui.opacityBox, !locked},
		{d->ui.pressureOpacity,
		 !mypaintmode && (!locked || lock == Lock::IndirectCompat)},
		{d->ui.smudgingBox, !locked && !mypaintmode},
		{d->ui.pressureSmudging, !locked && !mypaintmode},
		{d->ui.colorpickupBox, !locked && !mypaintmode},
		{d->ui.brushspacingBox, !locked && !mypaintmode},
		{d->ui.gainBox, !locked && mypaintmode},
		{d->ui.stabilizationWrapper, !locked},
		{d->ui.stabilizerButton, !locked},
	};

	d->ui.preview->setDisabled(locked);
	d->ui.noPermissionLabel->setVisible(locked);
	if(locked) {
		d->ui.noPermissionLabel->setText(getLockDescription(lock));
	}

	for(const QPair<QWidget *, bool> &pair : widgetVisibilities) {
		pair.first->setVisible(pair.second);
	}
}

void BrushSettings::emitBrushModeChanged()
{
	BrushMode currentBrushMode = getBrushMode();
	if(d->previousBrushMode != currentBrushMode) {
		d->previousBrushMode = currentBrushMode;
		emit brushModeChanged(currentBrushMode);
	}
}

void BrushSettings::pushSettings()
{
	tools::ToolController *ctrl = controller();
	ctrl->setActiveBrush(d->ui.preview->brush());
	ctrl->setFinishStrokes(d->finishStrokes);
	ctrl->setStabilizationMode(d->stabilizationMode());
	ctrl->setStabilizerSampleCount(d->ui.stabilizerBox->value());
	ctrl->setSmoothing(d->ui.smoothingBox->value() - d->globalSmoothing);
	ctrl->setStabilizerUseBrushSampleCount(d->useBrushSampleCount);
}

namespace toolprop {
static const ToolProperties::RangedValue<int>
	activeSlot = {QStringLiteral("active"), 0, 0, BRUSH_COUNT - 1},
	stabilizationMode{
		QString("stabilizationmode"), 0, 0,
		int(brushes::LastStabilizationMode)},
	stabilizer = {QStringLiteral("stabilizer"), 0, 0, 1000},
	smoothing = {
		QStringLiteral("smoothing"), 0, 0, libclient::settings::maxSmoothing};
static const ToolProperties::Value<bool>
	finishStrokes = {QStringLiteral("finishstrokes"), true},
	useBrushSampleCount = {QStringLiteral("usebrushsamplecount"), true};
// dynamic toolprops: brush[0-5] (serialized JSON)
}

ToolProperties BrushSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());

	cfg.setValue(toolprop::activeSlot, d->current);
	cfg.setValue(toolprop::finishStrokes, d->finishStrokes);
	cfg.setValue(toolprop::useBrushSampleCount, d->useBrushSampleCount);
	cfg.setValue(toolprop::stabilizationMode, int(d->stabilizationMode()));
	cfg.setValue(toolprop::stabilizer, d->ui.stabilizerBox->value());
	cfg.setValue(
		toolprop::smoothing, d->ui.smoothingBox->value() - d->globalSmoothing);

	for(int i = 0; i < BRUSH_COUNT; ++i) {
		cfg.setValue(
			ToolProperties::Value<QByteArray>{
				QStringLiteral("brush%1").arg(i), QByteArray()},
			d->brushSlots[i].toJson(true));

		const std::optional<brushes::ActiveBrush> &opt = d->lastPresets[i];
		if(opt.has_value()) {
			cfg.setValue(
				ToolProperties::Value<QByteArray>{
					QStringLiteral("last%1").arg(i), QByteArray()},
				opt.value().toJson());
		}
	}

	return cfg;
}

void BrushSettings::restoreToolSettings(const ToolProperties &cfg)
{

	for(int i = 0; i < BRUSH_COUNT; ++i) {
		brushes::ActiveBrush &brush = d->brushSlots[i];

		const QJsonObject o =
			QJsonDocument::fromJson(cfg.value(ToolProperties::Value<QByteArray>{
										QStringLiteral("brush%1").arg(i),
										getDefaultBrushForSlot(i)}))
				.object();
		brush = brushes::ActiveBrush::fromJson(o, true);

		const QJsonObject lo =
			QJsonDocument::fromJson(cfg.value(ToolProperties::Value<QByteArray>{
										QStringLiteral("last%1").arg(i),
										getDefaultBrushForSlot(i)}))
				.object();
		if(!lo.isEmpty()) {
			d->lastPresets[i] = brushes::ActiveBrush::fromJson(lo);
		}

		if(!d->shareBrushSlotColor)
			d->brushSlotButton[i]->setColorSwatch(brush.qColor());
	}

	brushes::ActiveBrush &eraser = d->brushSlots[ERASER_SLOT];
	eraser.classic().erase = true;
	eraser.myPaint().brush().erase = true;

	selectBrushSlot(cfg.value(toolprop::activeSlot));
	d->previousNonEraser = d->current != ERASER_SLOT ? d->current : 0;
	d->finishStrokes = cfg.value(toolprop::finishStrokes);
	d->useBrushSampleCount = cfg.value(toolprop::useBrushSampleCount);
	if(!d->useBrushSampleCount) {
		if(cfg.value(toolprop::stabilizationMode) == brushes::Smoothing) {
			d->smoothingAction->setChecked(true);
		} else {
			d->stabilizerAction->setChecked(true);
		}
		d->ui.stabilizerBox->setValue(cfg.value(toolprop::stabilizer));
		d->ui.smoothingBox->setValue(
			cfg.value(toolprop::smoothing) + d->globalSmoothing);
	}
}

void BrushSettings::setActiveTool(const tools::Tool::Type tool)
{
	switch(tool) {
	case tools::Tool::LINE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_LINE);
		break;
	case tools::Tool::RECTANGLE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_RECTANGLE);
		break;
	case tools::Tool::ELLIPSE:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_ELLIPSE);
		break;
	default:
		d->ui.preview->setPreviewShape(DP_BRUSH_PREVIEW_STROKE);
		break;
	}

	if(tool == tools::Tool::ERASER) {
		selectEraserSlot(true);
	} else {
		selectEraserSlot(false);
	}
}

void BrushSettings::setForeground(const QColor &color)
{
	if(color != d->currentBrush().qColor()) {
		if(d->shareBrushSlotColor) {
			for(int i = 0; i < BRUSH_COUNT; ++i)
				d->brushSlots[i].setQColor(color);

		} else {
			Q_ASSERT(d->current >= 0 && d->current < BRUSH_COUNT);
			d->currentBrush().setQColor(color);
			d->brushSlotButton[d->current]->setColorSwatch(color);
		}

		d->ui.preview->setBrush(d->currentBrush());
		pushSettings();
	}
}

void BrushSettings::quickAdjust1(qreal adjustment)
{
	// Adjust the currently visible box. Since the MyPaint radius has a much
	// larger range, we double the adjustment so that it feels responsive.
	if(d->currentIsMyPaint()) {
		quickAdjustOn(d->ui.radiusLogarithmicBox, adjustment * 2.0);
	} else {
		quickAdjustOn(d->ui.brushsizeBox, adjustment);
	}
}

void BrushSettings::stepAdjust1(bool increase)
{
	if(d->currentIsMyPaint()) {
		QSpinBox *radiusLogarithmicBox = d->ui.radiusLogarithmicBox;
		radiusLogarithmicBox->setValue(stepLinear(
			radiusLogarithmicBox->minimum(), radiusLogarithmicBox->maximum(),
			radiusLogarithmicBox->value(), increase));
	} else {
		QSpinBox *brushsizeBox = d->ui.brushsizeBox;
		brushsizeBox->setValue(stepLogarithmic(
			brushsizeBox->minimum(), brushsizeBox->maximum(),
			brushsizeBox->value(), increase));
	}
}

void BrushSettings::quickAdjustOn(QSpinBox *box, qreal adjustment)
{
	d->quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(d->quickAdjust1, &i);
	if(int(i)) {
		d->quickAdjust1 = f;
		box->setValue(box->value() + int(i));
	}
}

int BrushSettings::getSize() const
{
	if(d->currentIsMyPaint()) {
		return radiusLogarithmicToPixelSize(
			d->ui.radiusLogarithmicBox->value());
	} else {
		return d->ui.brushsizeBox->value();
	}
}

bool BrushSettings::getSubpixelMode() const
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	return brush.activeType() != brushes::ActiveBrush::CLASSIC ||
		   brush.classic().shape == DP_BRUSH_SHAPE_CLASSIC_SOFT_ROUND;
}

bool BrushSettings::isSquare() const
{
	const brushes::ActiveBrush &brush = d->currentBrush();
	return brush.activeType() == brushes::ActiveBrush::CLASSIC &&
		   brush.classic().shape == DP_BRUSH_SHAPE_CLASSIC_PIXEL_SQUARE;
}

BrushSettings::BrushMode BrushSettings::getBrushMode() const
{
	if(isCurrentEraserSlot()) {
		return UnknownMode;
	} else {
		const brushes::ActiveBrush &brush = d->currentBrush();
		if(brush.activeType() == brushes::ActiveBrush::MYPAINT) {
			const DP_MyPaintBrush &b = brush.myPaint().constBrush();
			return b.erase		  ? EraseMode
				   : b.lock_alpha ? AlphaLockMode
								  : NormalMode;
		} else {
			const DP_ClassicBrush &b = brush.classic();
			return b.erase								  ? EraseMode
				   : b.brush_mode == DP_BLEND_MODE_NORMAL ? NormalMode
														  : AlphaLockMode;
		}
	}
}

void BrushSettings::resetBrushMode()
{
	brushes::ActiveBrush &brush = d->currentBrush();
	if(brush.activeType() == brushes::ActiveBrush::MYPAINT) {
		DP_MyPaintBrush &b = brush.myPaint().brush();
		b.erase = false;
		b.lock_alpha = false;
	} else {
		DP_ClassicBrush &b = brush.classic();
		b.erase = false;
		b.brush_mode = DP_BLEND_MODE_NORMAL;
	}
	updateUi();
}

double BrushSettings::radiusLogarithmicToPixelSize(int radiusLogarithmic)
{
	return std::exp(radiusLogarithmic / 100.0 - 2.0) * 2.0;
}

QByteArray BrushSettings::getDefaultBrushForSlot(int i)
{
	switch(i) {
	case 0:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.800000011920929,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":4,\"sizecurve\":\"0,0;0.25,0.0625;0.5,0.25;0."
			   "75,0.5625;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 1:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":1,\"hardcurve\":\"0,0;1,1;"
			   "\",\"hardp\":true,\"opacity\":1,\"opacitycurve\":\"0,0;1,1;\","
			   "\"resmudge\":1,\"shape\":\"round-soft\",\"size\":3,"
			   "\"sizecurve\":\"0,0;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.5,"
			   "\"stabilizationmode\":0,\"stabilizer\":100},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 2:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.699999988079071,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":20,\"sizecurve\":\"0,0;1,1;\",\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.20000000298023224,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case 3:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.8999999761581421,"
			   "\"hardcurve\":\"0,0;1,1;\",\"indirect\":true,\"opacity\":1,"
			   "\"opacitycurve\":\"0,0;1,1;\",\"opacityp\":true,\"resmudge\":1,"
			   "\"shape\":\"round-soft\",\"size\":20,\"sizecurve\":\"0,0;1,1;"
			   "\",\"smoothing\":0,\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0."
			   "05000000074505806,\"stabilizationmode\":0,\"stabilizer\":0},"
			   "\"type\":\"dp-classic\",\"version\":1}";
	case 4:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":false,\"hard\":0.8899999856948853,"
			   "\"hardcurve\":\"0,0;1,1;\",\"indirect\":true,\"opacity\":1,"
			   "\"opacitycurve\":\"0,0;1,1;\",\"opacityp\":true,\"resmudge\":1,"
			   "\"shape\":\"round-pixel\",\"size\":4,\"size2\":1,\"sizecurve\":"
			   "\"0,0;1,1;\",\"sizep\":true,\"smoothing\":0,\"smudgecurve\":"
			   "\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	case ERASER_SLOT:
		return "{\"settings\":{\"blend\":\"svg:src-over\",\"blenderase\":\"svg:"
			   "dst-out\",\"erase\":true,\"hard\":0.800000011920929,"
			   "\"hardcurve\":\"0,0;1,1;\",\"opacity\":1,\"opacitycurve\":\"0,"
			   "0;1,1;\",\"opacityp\":true,\"resmudge\":1,\"shape\":\"round-"
			   "soft\",\"size\":20,\"sizecurve\":\"0,0;0.25,0.0625;0.5,0.25;0."
			   "75,0.5625;1,1;\",\"sizep\":true,\"smoothing\":0,"
			   "\"smudgecurve\":\"0,0;1,1;\",\"spacing\":0.05000000074505806,"
			   "\"stabilizationmode\":0,\"stabilizer\":0},\"type\":\"dp-"
			   "classic\",\"version\":1}";
	default:
		qWarning("Unknown slot for default brush %d", i);
		return QByteArray();
	};
}

}

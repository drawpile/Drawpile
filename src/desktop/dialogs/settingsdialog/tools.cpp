// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/tools.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libclient/view/enums.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPen>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <QtColorWidgets/ColorWheel>
#include <limits>

namespace dialogs {
namespace settingsdialog {

enum Driver {
	WindowsInk,
	WinTab,
	WinTabRelativePenMode,
};

Tools::Tools(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Tools::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initKeyboardShortcuts(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initSlots(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initCursors(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initColorSpace(cfg, utils::addFormSection(layout));
}

void Tools::initColorSpace(config::Config *cfg, QFormLayout *form)
{
	QButtonGroup *space = utils::addRadioGroup(
		form, tr("Color space:"), false,
		{{tr("HSV (Hue–Saturation–Value)"),
		  int(color_widgets::ColorWheel::ColorSpaceEnum::ColorHSV)},
		 {tr("HSL (Hue–Saturation–Lightness)"),
		  int(color_widgets::ColorWheel::ColorSpaceEnum::ColorHSL)},
		 {tr("HCL (Hue–Chroma–Luminance)"),
		  int(color_widgets::ColorWheel::ColorSpaceEnum::ColorLCH)}});
	CFG_BIND_BUTTONGROUP(cfg, ColorWheelSpace, space);
}

void Tools::initCursors(config::Config *cfg, QFormLayout *form)
{
	QDoubleSpinBox *outlineSize = new QDoubleSpinBox;
	CFG_BIND_DOUBLESPINBOX(cfg, BrushOutlineWidth, outlineSize);
	outlineSize->setDecimals(1);
	outlineSize->setMaximum(25.0);
	outlineSize->setSingleStep(.5);
	outlineSize->setSuffix(tr("px"));
	utils::EncapsulatedLayout *outlineSizeLayout = utils::encapsulate(
		tr("Show a %1 outline around the brush"), outlineSize);
	QCheckBox *showOutline = utils::addCheckable(
		tr("Enable brush outline"), outlineSizeLayout, outlineSize);
	connect(
		showOutline, &QCheckBox::toggled, outlineSize,
		[cfg, outlineSize](bool enable) {
			if(enable &&
			   outlineSize->value() < std::numeric_limits<double>::epsilon()) {
				outlineSize->setValue(1.0);
			} else if(!enable) {
				cfg->setBrushOutlineWidth(0.0);
			}
		});
	CFG_BIND_SET_FN(
		cfg, BrushOutlineWidth, showOutline,
		([outlineSize, showOutline](double value) {
			bool enabled = (value >= std::numeric_limits<double>::epsilon());
			outlineSize->setEnabled(enabled);
			showOutline->setChecked(enabled);
		}));

	form->addRow(tr("Brush outline:"), outlineSizeLayout);

	QComboBox *brushCursor = new QComboBox;
	QComboBox *eraseCursor = new QComboBox;
	QComboBox *alphaLockCursor = new QComboBox;

	QPair<QString, int> cursorPairs[] = {
		{tr("Blank"), int(view::Cursor::Blank)},
		{tr("Dot"), int(view::Cursor::Dot)},
		{tr("Crosshair"), int(view::Cursor::Cross)},
		{tr("Arrow"), int(view::Cursor::Arrow)},
		{tr("Right-handed triangle"), int(view::Cursor::TriangleRight)},
		{tr("Left-handed triangle"), int(view::Cursor::TriangleLeft)},
		{tr("Eraser"), int(view::Cursor::Eraser)},
	};

	for(QComboBox *cursor : {brushCursor, eraseCursor, alphaLockCursor}) {
		// Always adjust in case of locale changes
		cursor->setSizeAdjustPolicy(QComboBox::AdjustToContents);

		if(cursor != brushCursor) {
			cursor->addItem(
				tr("Same as brush cursor"), int(view::Cursor::SameAsBrush));
		}

		for(const QPair<QString, int> &p : cursorPairs) {
			cursor->addItem(p.first, p.second);
		}
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, BrushCursor, brushCursor);
	CFG_BIND_COMBOBOX_USER_INT(cfg, EraseCursor, eraseCursor);
	CFG_BIND_COMBOBOX_USER_INT(cfg, AlphaLockCursor, alphaLockCursor);

	form->addRow(tr("Brush cursor:"), brushCursor);
	form->addRow(tr("Eraser cursor:"), eraseCursor);
	form->addRow(tr("Alpha lock cursor:"), alphaLockCursor);

	QCheckBox *samplingRing = new QCheckBox(tr("Show sampling ring"));
	samplingRing->setChecked(
		cfg->getSamplingRingVisibility() ==
		int(view::SamplingRingVisibility::Always));
	connect(
		samplingRing, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		[cfg](compat::CheckBoxState state) {
			cfg->setSamplingRingVisibility(
				state == Qt::Unchecked
					? int(view::SamplingRingVisibility::TouchOnly)
					: int(view::SamplingRingVisibility::Always));
		});
	form->addRow(tr("Color picker:"), samplingRing);
}

void Tools::initKeyboardShortcuts(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *toggleKeys =
		new QCheckBox(tr("Toggle between previous and current tool"));
	CFG_BIND_CHECKBOX(cfg, ToolToggle, toggleKeys);
	form->addRow(tr("Keyboard shortcuts:"), toggleKeys);

	QCheckBox *focusCanvas =
		new QCheckBox(tr("Double-tap Alt key to focus canvas"));
	CFG_BIND_CHECKBOX(cfg, DoubleTapAltToFocusCanvas, focusCanvas);
	form->addRow(nullptr, focusCanvas);

	QSpinBox *temporarySwitchMs = new QSpinBox;
	temporarySwitchMs->setRange(0, 4000);
	CFG_BIND_SPINBOX(cfg, TemporaryToolSwitchMs, temporarySwitchMs);
	//: This stands for millseconds.
	temporarySwitchMs->setSuffix(tr("ms"));
	utils::EncapsulatedLayout *temporarySwitchLayout = utils::encapsulate(
		tr("Switch tool temporarily by holding primary shortcut for %1"),
		temporarySwitchMs);
	QCheckBox *temporarySwitch = utils::addCheckable(
		tr("Enable brush outline"), temporarySwitchLayout, temporarySwitchMs);
	CFG_BIND_CHECKBOX(cfg, TemporaryToolSwitch, temporarySwitch);
	form->addRow(nullptr, temporarySwitchLayout);

	QCheckBox *cancelDeselects = new QCheckBox();
	CFG_BIND_CHECKBOX(cfg, CancelDeselects, cancelDeselects);
	form->addRow(nullptr, cancelDeselects);
	CFG_BIND_SET_FN(
		cfg, Shortcuts, this,
		([cancelDeselects](const QVariantMap &shortcutsConfig) {
			QString shortcut;
			QString name = QStringLiteral("cancelaction");
			if(shortcutsConfig.contains(name)) {
				QVariant v = shortcutsConfig.value(name);
				if(v.canConvert<QKeySequence>()) {
					shortcut = v.value<QKeySequence>().toString(
						QKeySequence::NativeText);
				} else {
					for(const QVariant &vv : v.toList()) {
						if(vv.canConvert<QKeySequence>()) {
							shortcut = vv.value<QKeySequence>().toString(
								QKeySequence::NativeText);
							if(!shortcut.isEmpty()) {
								break;
							}
						}
					}
				}
			} else {
				for(const QKeySequence &ks :
					CustomShortcutModel::getDefaultShortcuts(name)) {
					shortcut = ks.toString(QKeySequence::NativeText);
					if(!shortcut.isEmpty()) {
						break;
					}
				}
			}

			cancelDeselects->setText(
				shortcut.isEmpty() ? tr("Cancel action to deselect")
								   : tr("Press %1 to deselect").arg(shortcut));
		}));
}

void Tools::initSlots(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *presetsAttach = new QCheckBox(tr("Attach selected brushes"));
	presetsAttach->setToolTip(
		tr("When enabled, changes in a brush slot will be saved to the brush "
		   "itself automatically. When disabled, the brush itself only changes "
		   "when you save it explicitly."));
	CFG_BIND_CHECKBOX(cfg, BrushPresetsAttach, presetsAttach);
	form->addRow(tr("Brush slots:"), presetsAttach);

	QCheckBox *shareColor =
		new QCheckBox(tr("Share one color across all brush slots"));
	CFG_BIND_CHECKBOX(cfg, ShareBrushSlotColor, shareColor);
	form->addRow(nullptr, shareColor);

	KisSliderSpinBox *brushSlotCount = new KisSliderSpinBox;
	brushSlotCount->setRange(1, 9);
	CFG_BIND_SLIDERSPINBOX(cfg, BrushSlotCount, brushSlotCount);
	CFG_BIND_SET_FN(
		cfg, BrushSlotCount, brushSlotCount, ([brushSlotCount](int count) {
			QString sep = QStringLiteral("%1");
			QStringList parts =
				tr("Show %1 brush slot(s)", nullptr, count).split(sep);
			brushSlotCount->setPrefix(
				parts.isEmpty() ? QString() : parts.takeFirst());
			brushSlotCount->setSuffix(parts.join(sep));
		}));
	form->addRow(nullptr, brushSlotCount);
	disableKineticScrollingOnWidget(brushSlotCount);
}

}
}

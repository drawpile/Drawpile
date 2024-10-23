// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/tools.h"
#include "desktop/settings.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/view/cursor.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
#include <QComboBox>
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
#include <utility>

namespace dialogs {
namespace settingsdialog {

enum Driver {
	WindowsInk,
	WinTab,
	WinTabRelativePenMode,
};

Tools::Tools(desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
}

void Tools::setUp(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initKeyboardShortcuts(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initSlots(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initCursors(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initColorSpace(settings, utils::addFormSection(layout));
}

void Tools::initColorSpace(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *space = utils::addRadioGroup(
		form, tr("Color space:"), false,
		{{tr("HSV (Hue–Saturation–Value)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorHSV},
		 {tr("HSL (Hue–Saturation–Lightness)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorHSL},
		 {tr("HCL (Hue–Chroma–Luminance)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorLCH}});
	settings.bindColorWheelSpace(space);
}

void Tools::initCursors(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *outlineSize = new QDoubleSpinBox;
	settings.bindBrushOutlineWidth(outlineSize);
	outlineSize->setDecimals(1);
	outlineSize->setMaximum(25.0);
	outlineSize->setSingleStep(.5);
	outlineSize->setSuffix(tr("px"));
	auto *outlineSizeLayout = utils::encapsulate(
		tr("Show a %1 outline around the brush"), outlineSize);
	auto *showOutline = utils::addCheckable(
		tr("Enable brush outline"), outlineSizeLayout, outlineSize);
	connect(
		showOutline, &QCheckBox::toggled, outlineSize,
		[=, &settings](bool enable) {
			if(enable &&
			   outlineSize->value() < std::numeric_limits<double>::epsilon()) {
				outlineSize->setValue(1.0);
			} else if(!enable) {
				settings.setBrushOutlineWidth(0.0);
			}
		});
	settings.bindBrushOutlineWidth(showOutline, [=](double value) {
		const auto enabled = (value >= std::numeric_limits<double>::epsilon());
		showOutline->setChecked(enabled);
		outlineSize->setEnabled(enabled);
	});

	form->addRow(tr("Brush outline:"), outlineSizeLayout);

	auto *brushCursor = new QComboBox;
	auto *eraseCursor = new QComboBox;
	auto *alphaLockCursor = new QComboBox;

	for(QComboBox *cursor : {brushCursor, eraseCursor, alphaLockCursor}) {
		// Always adjust in case of locale changes
		cursor->setSizeAdjustPolicy(QComboBox::AdjustToContents);

		if(cursor != brushCursor) {
			cursor->addItem(
				tr("Same as brush cursor"), int(view::Cursor::SameAsBrush));
		}

		for(const auto &[name, value] :
			{std::pair{tr("Dot"), view::Cursor::Dot},
			 std::pair{tr("Crosshair"), view::Cursor::Cross},
			 std::pair{tr("Arrow"), view::Cursor::Arrow},
			 std::pair{
				 tr("Right-handed triangle"), view::Cursor::TriangleRight},
			 std::pair{tr("Left-handed triangle"), view::Cursor::TriangleLeft},
			 std::pair{tr("Eraser"), view::Cursor::Eraser}}) {
			cursor->addItem(name, int(value));
		}
	}

	settings.bindBrushCursor(brushCursor, Qt::UserRole);
	settings.bindEraseCursor(eraseCursor, Qt::UserRole);
	settings.bindAlphaLockCursor(alphaLockCursor, Qt::UserRole);

	form->addRow(tr("Brush cursor:"), brushCursor);
	form->addRow(tr("Eraser cursor:"), eraseCursor);
	form->addRow(tr("Alpha lock cursor:"), alphaLockCursor);

	QCheckBox *samplingRing = new QCheckBox(tr("Show sampling ring"));
	samplingRing->setChecked(
		settings.samplingRingVisibility() ==
		int(desktop::settings::SamplingRingVisibility::Always));
	connect(
		samplingRing, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		[&settings](compat::CheckBoxState state) {
			settings.setSamplingRingVisibility(
				state == Qt::Unchecked
					? int(desktop::settings::SamplingRingVisibility::TouchOnly)
					: int(desktop::settings::SamplingRingVisibility::Always));
		});
	form->addRow(tr("Color picker:"), samplingRing);
}

void Tools::initKeyboardShortcuts(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *toggleKeys =
		new QCheckBox(tr("Toggle between previous and current tool"));
	settings.bindToolToggle(toggleKeys);
	form->addRow(tr("Keyboard shortcuts:"), toggleKeys);

	auto *focusCanvas = new QCheckBox(tr("Double-tap Alt key to focus canvas"));
	settings.bindDoubleTapAltToFocusCanvas(focusCanvas);
	form->addRow(nullptr, focusCanvas);
}

void Tools::initSlots(desktop::settings::Settings &settings, QFormLayout *form)
{
	QCheckBox *presetsAttach = new QCheckBox(tr("Attach selected brushes"));
	presetsAttach->setToolTip(
		tr("When enabled, changes in a brush slot will be saved to the brush "
		   "itself automatically. When disabled, the brush itself only changes "
		   "when you save it explicitly."));
	settings.bindBrushPresetsAttach(presetsAttach);
	form->addRow(tr("Brush slots:"), presetsAttach);

	auto *shareColor =
		new QCheckBox(tr("Share one color across all brush slots"));
	settings.bindShareBrushSlotColor(shareColor);
	form->addRow(nullptr, shareColor);

	KisSliderSpinBox *brushSlotCount = new KisSliderSpinBox;
	brushSlotCount->setRange(1, 9);
	settings.bindBrushSlotCount(brushSlotCount);
	settings.bindBrushSlotCount(brushSlotCount, [brushSlotCount](int count) {
		QString sep = QStringLiteral("%1");
		QStringList parts =
			tr("Show %1 brush slot(s)", nullptr, count).split(sep);
		brushSlotCount->setPrefix(
			parts.isEmpty() ? QString() : parts.takeFirst());
		brushSlotCount->setSuffix(parts.join(sep));
	});
	form->addRow(nullptr, brushSlotCount);
}

} // namespace settingsdialog
} // namespace dialogs

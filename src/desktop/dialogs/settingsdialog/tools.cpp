// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/tools.h"
#include "desktop/scene/canvasview.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
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
	initGeneralTools(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initColorWheel(settings, layout);
}

void Tools::initColorWheel(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	QHBoxLayout *section = new QHBoxLayout;
	layout->addLayout(section);

	QFormLayout *form = utils::addFormSection(section);

	auto *shape = utils::addRadioGroup(
		form, tr("Shape:"), false,
		{{tr("Triangle"), color_widgets::ColorWheel::ShapeEnum::ShapeTriangle},
		 {tr("Square"), color_widgets::ColorWheel::ShapeEnum::ShapeSquare}});
	settings.bindColorWheelShape(shape);

	utils::addFormSpacer(form);

	auto *angle = utils::addRadioGroup(
		form, tr("Angle:"), false,
		{{tr("Fixed"), color_widgets::ColorWheel::AngleEnum::AngleFixed},
		 {tr("Rotating"),
		  color_widgets::ColorWheel::AngleEnum::AngleRotating}});
	settings.bindColorWheelAngle(angle);

	utils::addFormSpacer(form);

	auto *mirror = utils::addRadioGroup(
		form, tr("Direction:"), false,
		{{tr("Ascending"), true}, {tr("Descending"), false}});
	settings.bindColorWheelMirror(mirror);

	utils::addFormSpacer(form);

	auto *space = utils::addRadioGroup(
		form, tr("Color space:"), false,
		{{tr("HSV (Hue–Saturation–Value)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorHSV},
		 {tr("HSL (Hue–Saturation–Lightness)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorHSL},
		 {tr("HCL (Hue–Chroma–Luminance)"),
		  color_widgets::ColorWheel::ColorSpaceEnum::ColorLCH}});
	settings.bindColorWheelSpace(space);

	auto *preview = new color_widgets::ColorWheel;
	preview->setMinimumWidth(150);
	settings.bindColorWheelShape(
		preview, &color_widgets::ColorWheel::setSelectorShape);
	settings.bindColorWheelAngle(
		preview, &color_widgets::ColorWheel::setRotatingSelector);
	settings.bindColorWheelMirror(
		preview, &color_widgets::ColorWheel::setMirroredSelector);
	settings.bindColorWheelSpace(
		preview, &color_widgets::ColorWheel::setColorSpace);
	section->addWidget(preview);
}

void Tools::initGeneralTools(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *shareColor =
		new QCheckBox(tr("Share one color across all brush slots"));
	settings.bindShareBrushSlotColor(shareColor);
	form->addRow(tr("Brushes:"), shareColor);

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

	form->addRow(nullptr, outlineSizeLayout);

	auto *brushCursor = new QComboBox;
	// Always adjust in case of locale changes
	brushCursor->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	using BrushCursor = widgets::CanvasView::BrushCursor;
	for(const auto &[name, value] :
		{std::pair{tr("Dot"), BrushCursor::Dot},
		 std::pair{tr("Crosshair"), BrushCursor::Cross},
		 std::pair{tr("Arrow"), BrushCursor::Arrow},
		 std::pair{tr("Right-handed triangle"), BrushCursor::TriangleRight},
		 std::pair{tr("Left-handed triangle"), BrushCursor::TriangleLeft}}) {
		brushCursor->addItem(name, QVariant::fromValue(value));
	}
	settings.bindBrushCursor(brushCursor, Qt::UserRole);

	auto *brushCursorLayout =
		utils::encapsulate(tr("Draw the brush as a %1"), brushCursor);
	utils::setSpacingControlType(brushCursorLayout, QSizePolicy::ComboBox);
	form->addRow(nullptr, brushCursorLayout);
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

} // namespace settingsdialog
} // namespace dialogs

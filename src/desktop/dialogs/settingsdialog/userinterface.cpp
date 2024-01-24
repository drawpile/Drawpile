// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QPair>
#include <QPlainTextEdit>
#include <QtColorWidgets/ColorPreview>

namespace dialogs {
namespace settingsdialog {

UserInterface::UserInterface(
	desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
}

void UserInterface::setUp(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initScaling(settings, layout);
	utils::addFormSeparator(layout);
	initInterfaceMode(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initKineticScrolling(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initMiscellaneous(settings, utils::addFormSection(layout));
}

void UserInterface::initInterfaceMode(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QButtonGroup *interfaceMode = utils::addRadioGroup(
		form, tr("Interface mode:"), true,
		{{tr("Desktop"), int(desktop::settings::InterfaceMode::Desktop)},
		 {tr("Small screen (experimental)"),
		  int(desktop::settings::InterfaceMode::SmallScreen)}});
	settings.bindInterfaceMode(interfaceMode);
	form->addRow(
		nullptr, utils::formNote(tr("Changes to the interface mode apply after "
									"you restart Drawpile.")));
}


void UserInterface::initKineticScrolling(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QComboBox *kineticScrollGesture = new QComboBox;
	kineticScrollGesture->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	using desktop::settings::KineticScrollGesture;
	QPair<QString, int> gestures[] = {
		{tr("Disabled"), int(KineticScrollGesture::None)},
		{tr("On left-click drag"), int(KineticScrollGesture::LeftClick)},
		{tr("On middle-click drag"), int(KineticScrollGesture::MiddleClick)},
		{tr("On right-click drag"), int(KineticScrollGesture::RightClick)},
		{tr("On touch drag"), int(KineticScrollGesture::Touch)},
	};
	for(const auto &[name, value] : gestures) {
		kineticScrollGesture->addItem(name, QVariant::fromValue(value));
	}

	settings.bindKineticScrollGesture(kineticScrollGesture, Qt::UserRole);
	form->addRow(tr("Kinetic scrolling:"), kineticScrollGesture);

	KisSliderSpinBox *threshold = new KisSliderSpinBox;
	threshold->setRange(0, 100);
	threshold->setPrefix(tr("Threshold: "));
	settings.bindKineticScrollThreshold(threshold);
	form->addRow(nullptr, threshold);

	QCheckBox *hideBars = new QCheckBox(tr("Hide scroll bars"));
	settings.bindKineticScrollHideBars(hideBars);
	form->addRow(nullptr, hideBars);

	settings.bindKineticScrollGesture(this, [threshold, hideBars](int gesture) {
		bool enabled = gesture != int(KineticScrollGesture::None);
		threshold->setEnabled(enabled);
		hideBars->setEnabled(enabled);
	});

	form->addRow(
		nullptr,
		utils::formNote(tr(
			"Changes to kinetic scrolling apply after you restart Drawpile.")));
}

void UserInterface::initMiscellaneous(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	using color_widgets::ColorPreview;
	ColorPreview *colorPreview = new ColorPreview;
	colorPreview->setDisplayMode(ColorPreview::DisplayMode::NoAlpha);
	colorPreview->setToolTip(tr("Background color behind the canvas"));
	colorPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	colorPreview->setCursor(Qt::PointingHandCursor);
	form->addRow(tr("Color behind canvas:"), colorPreview);
	settings.bindCanvasViewBackgroundColor(
		this, [colorPreview](const QColor &color) {
			colorPreview->setColor(color);
		});

	connect(colorPreview, &ColorPreview::clicked, this, [this, &settings] {
		pickCanvasBackgroundColor(settings);
	});

	QCheckBox *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	settings.bindCanvasScrollBars(scrollBars);
	form->addRow(tr("Miscellaneous:"), scrollBars);

	QCheckBox *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	settings.bindConfirmLayerDelete(confirmDelete);
	form->addRow(nullptr, confirmDelete);
}

void UserInterface::initScaling(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCheckBox *highDpiScalingEnabled =
		new QCheckBox(tr("Enable high-DPI scaling (experimental)"));
	settings.bindHighDpiScalingEnabled(highDpiScalingEnabled);
	layout->addWidget(highDpiScalingEnabled);
#endif

	QCheckBox *overrideScaleFactor =
		new QCheckBox(tr("Override system scale factor (experimental)"));
	settings.bindHighDpiScalingOverride(overrideScaleFactor);
	layout->addWidget(overrideScaleFactor);

	KisSliderSpinBox *scaleFactor = new KisSliderSpinBox;
	scaleFactor->setRange(100, 400);
	scaleFactor->setSingleStep(25);
	scaleFactor->setPrefix(tr("Scale factor: "));
	scaleFactor->setSuffix(tr("%"));
	settings.bindHighDpiScalingFactor(scaleFactor);
	layout->addWidget(scaleFactor);

	QCheckBox *overrideFontSize =
		new QCheckBox(tr("Override system font size"));
	settings.bindOverrideFontSize(overrideFontSize);
	layout->addWidget(overrideFontSize);

	KisSliderSpinBox *fontSize = new KisSliderSpinBox;
	fontSize->setRange(6, 16);
	fontSize->setPrefix(tr("Font size: "));
	fontSize->setSuffix(tr("pt"));
	settings.bindFontSize(fontSize);
	layout->addWidget(fontSize);

	layout->addWidget(utils::formNote(tr(
		"Changes to scaling and font size apply after you restart Drawpile.")));

	settings.bindHighDpiScalingOverride(scaleFactor, &QWidget::setEnabled);
	settings.bindOverrideFontSize(fontSize, &QWidget::setEnabled);
}

void UserInterface::pickCanvasBackgroundColor(
	desktop::settings::Settings &settings)
{
	using color_widgets::ColorDialog;
	ColorDialog dlg;
	dlg.setColor(settings.canvasViewBackgroundColor());
	dlg.setAlphaEnabled(false);
	dialogs::applyColorDialogSettings(&dlg);
	dialogs::setColorDialogResetColor(
		&dlg, CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT);
	if(dlg.exec() == QDialog::Accepted) {
		settings.setCanvasViewBackgroundColor(dlg.color());
	}
}

}
}

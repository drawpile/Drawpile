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
#include <QSurfaceFormat>
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
	initRequiringRestart(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	QFormLayout *form = utils::addFormSection(layout);
	initInterfaceMode(settings, form);
	initKineticScrolling(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initMiscellaneous(settings, utils::addFormSection(layout));
}

void UserInterface::initInterfaceMode(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QButtonGroup *interfaceMode = utils::addRadioGroup(
		form, tr("Interface mode:"), true,
		{{tr("Dynamic"), int(desktop::settings::InterfaceMode::Dynamic)},
		 {tr("Desktop"), int(desktop::settings::InterfaceMode::Desktop)},
		 {tr("Small screen"),
		  int(desktop::settings::InterfaceMode::SmallScreen)}});
	settings.bindInterfaceMode(interfaceMode);
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
	threshold->setBlockUpdateSignalOnDrag(true);
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
}

void UserInterface::initMiscellaneous(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	using color_widgets::ColorPreview;

	ColorPreview *backgroundPreview = new ColorPreview;
	ColorPreview *checker1Preview = new ColorPreview;
	ColorPreview *checker2Preview = new ColorPreview;
	backgroundPreview->setDisplayMode(ColorPreview::DisplayMode::NoAlpha);
	checker1Preview->setDisplayMode(ColorPreview::DisplayMode::NoAlpha);
	checker2Preview->setDisplayMode(ColorPreview::DisplayMode::NoAlpha);
	backgroundPreview->setToolTip(tr("Background color behind the canvas"));
	checker1Preview->setToolTip(tr("First checker color"));
	checker2Preview->setToolTip(tr("Second checker color"));
	backgroundPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	checker1Preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	checker2Preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	backgroundPreview->setCursor(Qt::PointingHandCursor);
	checker1Preview->setCursor(Qt::PointingHandCursor);
	checker2Preview->setCursor(Qt::PointingHandCursor);
	QHBoxLayout *backgroundLayout = new QHBoxLayout;
	backgroundLayout->addWidget(backgroundPreview);
	backgroundLayout->addWidget(checker1Preview);
	backgroundLayout->addWidget(checker2Preview);
	backgroundLayout->addStretch();
	form->addRow(tr("Colors behind canvas:"), backgroundLayout);

	settings.bindCanvasViewBackgroundColor(
		this, [backgroundPreview](const QColor &color) {
			backgroundPreview->setColor(color);
		});
	settings.bindCheckerColor1(this, [checker1Preview](const QColor &color) {
		checker1Preview->setColor(color);
	});
	settings.bindCheckerColor2(this, [checker2Preview](const QColor &color) {
		checker2Preview->setColor(color);
	});

	connect(backgroundPreview, &ColorPreview::clicked, this, [this, &settings] {
		pickColor(
			settings, &desktop::settings::Settings::canvasViewBackgroundColor,
			&desktop::settings::Settings::setCanvasViewBackgroundColor,
			CANVAS_VIEW_BACKGROUND_COLOR_DEFAULT);
	});
	connect(checker1Preview, &ColorPreview::clicked, this, [this, &settings] {
		pickColor(
			settings, &desktop::settings::Settings::checkerColor1,
			&desktop::settings::Settings::setCheckerColor1,
			CHECKER_COLOR1_DEFAULT);
	});
	connect(checker2Preview, &ColorPreview::clicked, this, [this, &settings] {
		pickColor(
			settings, &desktop::settings::Settings::checkerColor2,
			&desktop::settings::Settings::setCheckerColor2,
			CHECKER_COLOR2_DEFAULT);
	});

	QCheckBox *showTransformNotices =
		new QCheckBox(tr("Zoom, rotate, mirror and flip notices"));
	settings.bindShowTransformNotices(showTransformNotices);
	form->addRow(tr("On-canvas notices:"), showTransformNotices);

	QCheckBox *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	settings.bindCanvasScrollBars(scrollBars);
	form->addRow(tr("Miscellaneous:"), scrollBars);

	QCheckBox *promptCreate = new QCheckBox(tr("Prompt when creating layers"));
	settings.bindPromptLayerCreate(promptCreate);
	form->addRow(nullptr, promptCreate);

	QCheckBox *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	settings.bindConfirmLayerDelete(confirmDelete);
	form->addRow(nullptr, confirmDelete);

	QCheckBox *automaticAlphaPreserve = new QCheckBox(
		tr("Automatically inherit and preserve alpha based on blend mode"));
	settings.bindAutomaticAlphaPreserve(automaticAlphaPreserve);
	form->addRow(nullptr, automaticAlphaPreserve);
}

void UserInterface::initRequiringRestart(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QSettings *scalingSettings = settings.scalingSettings();
	QString scalingLabel = tr("Scaling:");

	QCheckBox *overrideScaleFactor =
		new QCheckBox(tr("Override system scale factor"));
	overrideScaleFactor->setChecked(
		scalingSettings
			->value(
				QStringLiteral("scaling_override"), SCALING_OVERRIDE_DEFAULT)
			.toBool());
	connect(
		overrideScaleFactor, &QCheckBox::clicked, scalingSettings,
		[scalingSettings](bool checked) {
			scalingSettings->setValue(
				QStringLiteral("scaling_override"), checked);
		});
	form->addRow(scalingLabel, overrideScaleFactor);

	KisSliderSpinBox *scaleFactor = new KisSliderSpinBox;
	scaleFactor->setRange(100, 400);
	scaleFactor->setSingleStep(25);
	scaleFactor->setPrefix(tr("Scale factor: "));
	scaleFactor->setSuffix(tr("%"));
	scaleFactor->setValue(
		scalingSettings->value(QStringLiteral("scaling_factor"), 100).toInt());
	connect(
		scaleFactor, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		scalingSettings, [scalingSettings](int value) {
			scalingSettings->setValue(QStringLiteral("scaling_factor"), value);
		});
	form->addRow(nullptr, scaleFactor);

	connect(
		overrideScaleFactor, &QCheckBox::clicked, scaleFactor,
		&QWidget::setEnabled);
	scaleFactor->setEnabled(overrideScaleFactor->isChecked());

	QCheckBox *overrideFontSize =
		new QCheckBox(tr("Override system font size"));
	settings.bindOverrideFontSize(overrideFontSize);
	form->addRow(tr("Font size:"), overrideFontSize);

	KisSliderSpinBox *fontSize = new KisSliderSpinBox;
	fontSize->setRange(6, 16);
	fontSize->setPrefix(tr("Font size: "));
	fontSize->setSuffix(tr("pt"));
	settings.bindFontSize(fontSize);
	settings.bindOverrideFontSize(fontSize, &QWidget::setEnabled);
	form->addRow(nullptr, fontSize);

	QButtonGroup *vsyncButtons = utils::addRadioGroup(
		form, tr("Vertical sync:"), true,
		{{tr("Disabled"), 0}, {tr("Enabled"), 1}, {tr("System"), -1}});

	int vsync = scalingSettings->value(QStringLiteral("vsync")).toInt();
	QAbstractButton *vsyncButton = vsyncButtons->button(vsync);
	if(vsyncButton) {
		vsyncButton->click();
	}

	connect(
		vsyncButtons, &QButtonGroup::idClicked, scalingSettings,
		[scalingSettings](int id) {
			scalingSettings->setValue(QStringLiteral("vsync"), id);
		});

	int pointSize = font().pointSize();
	QString currentFontSize = pointSize == -1
								  ? tr("%1px").arg(font().pixelSize())
								  : tr("%1pt").arg(pointSize);
	form->addRow(
		nullptr,
		utils::formNote(tr("Changes apply after you restart Drawpile. Current "
						   "scale factor is %1%, font size is %2.")
							.arg(qRound(devicePixelRatioF() * 100.0))
							.arg(currentFontSize)));
}

void UserInterface::pickColor(
	desktop::settings::Settings &settings,
	QColor (desktop::settings::Settings::*getColor)() const,
	void (desktop::settings::Settings::*setColor)(QColor),
	const QColor &defaultColor)
{
	using color_widgets::ColorDialog;
	ColorDialog *dlg =
		dialogs::newDeleteOnCloseColorDialog((settings.*getColor)(), this);
	dlg->setAlphaEnabled(false);
	dialogs::setColorDialogResetColor(dlg, defaultColor);
	connect(dlg, &ColorDialog::accepted, this, [&settings, setColor, dlg] {
		(settings.*setColor)(dlg->color());
	});
	dlg->show();
}

}
}

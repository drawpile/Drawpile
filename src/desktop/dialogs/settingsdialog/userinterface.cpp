// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/view/enums.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFont>
#include <QFormLayout>
#include <QPair>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSurfaceFormat>
#include <QtColorWidgets/ColorPreview>

namespace dialogs {
namespace settingsdialog {

UserInterface::UserInterface(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void UserInterface::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initRequiringRestart(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	QFormLayout *form = utils::addFormSection(layout);
	initInterfaceMode(cfg, form);
	initKineticScrolling(cfg, form);
	utils::addFormSeparator(layout);
	initMiscellaneous(cfg, utils::addFormSection(layout));
}

void UserInterface::initInterfaceMode(config::Config *cfg, QFormLayout *form)
{
	QString interfaceModeLabel = tr("Interface mode:");
#if defined(Q_OS_ANDROID) && defined(KRITA_QT_SCREEN_DENSITY_ADJUSTMENT)
	Q_UNUSED(cfg);

	QHBoxLayout *scalingLayout = new QHBoxLayout;
	scalingLayout->setContentsMargins(0, 0, 0, 0);
	scalingLayout->setSpacing(0);

	QPushButton *scalingButton = new QPushButton(
		QIcon::fromTheme(QStringLiteral("monitor")),
		tr("Change mode and scale…"));
	scalingLayout->addWidget(scalingButton);
	connect(
		scalingButton, &QPushButton::clicked, this,
		&UserInterface::scalingChangeRequested);

	scalingLayout->addStretch(1);

	form->addRow(interfaceModeLabel, scalingLayout);
#else
	QButtonGroup *interfaceMode = utils::addRadioGroup(
		form, interfaceModeLabel, true,
		{{tr("Dynamic"), int(view::InterfaceMode::Dynamic)},
		 {tr("Desktop"), int(view::InterfaceMode::Desktop)},
		 {tr("Mobile"), int(view::InterfaceMode::SmallScreen)}});
	CFG_BIND_BUTTONGROUP(cfg, InterfaceMode, interfaceMode);
#endif
}

void UserInterface::initKineticScrolling(config::Config *cfg, QFormLayout *form)
{
	QComboBox *kineticScrollGesture = new QComboBox;
	kineticScrollGesture->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	QPair<QString, int> gestures[] = {
		{tr("Disabled"), int(view::KineticScrollGesture::None)},
		{tr("On left-click drag"), int(view::KineticScrollGesture::LeftClick)},
		{tr("On middle-click drag"),
		 int(view::KineticScrollGesture::MiddleClick)},
		{tr("On right-click drag"),
		 int(view::KineticScrollGesture::RightClick)},
		{tr("On touch drag"), int(view::KineticScrollGesture::Touch)},
	};
	for(const QPair<QString, int> &p : gestures) {
		kineticScrollGesture->addItem(p.first, QVariant::fromValue(p.second));
	}

	CFG_BIND_COMBOBOX_USER_INT(cfg, KineticScrollGesture, kineticScrollGesture);
	form->addRow(tr("Kinetic scrolling:"), kineticScrollGesture);

	KisSliderSpinBox *threshold = new KisSliderSpinBox;
	threshold->setRange(0, 100);
	threshold->setPrefix(tr("Threshold: "));
	threshold->setBlockUpdateSignalOnDrag(true);
	CFG_BIND_SLIDERSPINBOX(cfg, KineticScrollThreshold, threshold);
	form->addRow(nullptr, threshold);
	disableKineticScrollingOnWidget(threshold);

	QCheckBox *hideBars = new QCheckBox(tr("Hide scroll bars"));
	CFG_BIND_CHECKBOX(cfg, KineticScrollHideBars, hideBars);
	form->addRow(nullptr, hideBars);

	CFG_BIND_SET_FN(
		cfg, KineticScrollGesture, this, ([threshold, hideBars](int gesture) {
			bool enabled = gesture != int(view::KineticScrollGesture::None);
			threshold->setEnabled(enabled);
			threshold->setVisible(enabled);
			hideBars->setEnabled(enabled);
			hideBars->setVisible(enabled);
		}));
}

void UserInterface::initMiscellaneous(config::Config *cfg, QFormLayout *form)
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

	CFG_BIND_SET_FN(
		cfg, CanvasViewBackgroundColor, this,
		[backgroundPreview](const QColor &color) {
			backgroundPreview->setColor(color);
		});
	CFG_BIND_SET_FN(
		cfg, CheckerColor1, this, [checker1Preview](const QColor &color) {
			checker1Preview->setColor(color);
		});
	CFG_BIND_SET_FN(
		cfg, CheckerColor2, this, [checker2Preview](const QColor &color) {
			checker2Preview->setColor(color);
		});

	connect(backgroundPreview, &ColorPreview::clicked, this, [this, cfg] {
		pickColor(
			cfg, &config::Config::getCanvasViewBackgroundColor,
			&config::Config::setCanvasViewBackgroundColor,
			config::Config::defaultCanvasViewBackgroundColor());
	});
	connect(checker1Preview, &ColorPreview::clicked, this, [this, cfg] {
		pickColor(
			cfg, &config::Config::getCheckerColor1,
			&config::Config::setCheckerColor1,
			config::Config::defaultCheckerColor1());
	});
	connect(checker2Preview, &ColorPreview::clicked, this, [this, cfg] {
		pickColor(
			cfg, &config::Config::getCheckerColor2,
			&config::Config::setCheckerColor2,
			config::Config::defaultCheckerColor2());
	});

	QCheckBox *showTransformNotices =
		new QCheckBox(tr("Zoom, rotate, mirror and flip notices"));
	CFG_BIND_CHECKBOX(cfg, ShowTransformNotices, showTransformNotices);
	form->addRow(tr("On-canvas notices:"), showTransformNotices);

	QCheckBox *showViewModeNotices = new QCheckBox(tr("View mode notices"));
	CFG_BIND_CHECKBOX(cfg, ShowViewModeNotices, showViewModeNotices);
	form->addRow(nullptr, showViewModeNotices);

	QCheckBox *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	CFG_BIND_CHECKBOX(cfg, CanvasScrollBars, scrollBars);
	form->addRow(tr("Miscellaneous:"), scrollBars);

	QCheckBox *promptCreate = new QCheckBox(tr("Prompt when creating layers"));
	CFG_BIND_CHECKBOX(cfg, PromptLayerCreate, promptCreate);
	form->addRow(nullptr, promptCreate);

	QCheckBox *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	CFG_BIND_CHECKBOX(cfg, ConfirmLayerDelete, confirmDelete);
	form->addRow(nullptr, confirmDelete);

	QCheckBox *automaticAlphaPreserve = new QCheckBox(
		tr("Automatically inherit and preserve alpha based on blend mode"));
	CFG_BIND_CHECKBOX(cfg, AutomaticAlphaPreserve, automaticAlphaPreserve);
	form->addRow(nullptr, automaticAlphaPreserve);

	QCheckBox *longPress =
		new QCheckBox(tr("Long-press to open context menus"));
	CFG_BIND_CHECKBOX(cfg, LongPressEnabled, longPress);
	form->addRow(nullptr, longPress);

#ifdef Q_OS_MACOS
	QCheckBox *quitOnClose =
		new QCheckBox(tr("Quit when last window is closed"));
	CFG_BIND_CHECKBOX(cfg, QuitOnLastWindowClosed, quitOnClose);
	form->addRow(tr("macOS:"), quitOnClose);
#endif
}

void UserInterface::initRequiringRestart(config::Config *cfg, QFormLayout *form)
{
	QSettings *scalingSettings = dpApp().scalingSettings();

#if !defined(Q_OS_ANDROID) || !defined(KRITA_QT_SCREEN_DENSITY_ADJUSTMENT)
	QCheckBox *overrideScaleFactor =
		new QCheckBox(tr("Override system scale factor"));
	overrideScaleFactor->setChecked(
		scalingSettings->value(QStringLiteral("scaling_override"), false)
			.toBool());
	connect(
		overrideScaleFactor, &QCheckBox::clicked, scalingSettings,
		[scalingSettings](bool checked) {
			scalingSettings->setValue(
				QStringLiteral("scaling_override"), checked);
		});
	form->addRow(tr("Scaling:"), overrideScaleFactor);

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
	disableKineticScrollingOnWidget(scaleFactor);

	connect(
		overrideScaleFactor, &QCheckBox::clicked, scaleFactor,
		&QWidget::setEnabled);
	connect(
		overrideScaleFactor, &QCheckBox::clicked, scaleFactor,
		&QWidget::setVisible);
	scaleFactor->setEnabled(overrideScaleFactor->isChecked());
	scaleFactor->setVisible(overrideScaleFactor->isChecked());
#endif

	QCheckBox *overrideFontSize =
		new QCheckBox(tr("Override system font size"));
	CFG_BIND_CHECKBOX(cfg, OverrideFontSize, overrideFontSize);
	form->addRow(tr("Font size:"), overrideFontSize);

	int pointSize = font().pointSize();
	KisSliderSpinBox *fontSize = new KisSliderSpinBox;
	fontSize->setRange(6, 16);
	fontSize->setPrefix(tr("Font size: "));
	fontSize->setSuffix(pointSize == -1 ? tr("px") : tr("pt"));
	form->addRow(nullptr, fontSize);
	CFG_BIND_SLIDERSPINBOX(cfg, FontSize, fontSize);
	CFG_BIND_SET(cfg, OverrideFontSize, fontSize, KisSliderSpinBox::setEnabled);
	CFG_BIND_SET(cfg, OverrideFontSize, fontSize, KisSliderSpinBox::setVisible);
	disableKineticScrollingOnWidget(fontSize);

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
	config::Config *cfg, QColor (config::Config::*getColor)() const,
	void (config::Config::*setColor)(const QColor &),
	const QColor &defaultColor)
{
	using color_widgets::ColorDialog;
	ColorDialog *dlg =
		dialogs::newDeleteOnCloseColorDialog((cfg->*getColor)(), this);
	dlg->setAlphaEnabled(false);
	dialogs::setColorDialogResetColor(dlg, defaultColor);
	connect(dlg, &ColorDialog::accepted, this, [cfg, setColor, dlg] {
		(cfg->*setColor)(dlg->color());
	});
	dlg->show();
}

}
}

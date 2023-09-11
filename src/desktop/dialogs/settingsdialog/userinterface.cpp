// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QtColorWidgets/ColorPreview>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QPair>
#include <QPlainTextEdit>

namespace dialogs {
namespace settingsdialog {

UserInterface::UserInterface(
	desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	// TODO: make the form layout work with RTL or go back to a QFormLayout.
	setLayoutDirection(Qt::LeftToRight);
	utils::SanerFormLayout *form = new utils::SanerFormLayout(this);
	initFontSize(settings, form);
	form->addSeparator();
	initInterfaceMode(settings, form);
	form->addSeparator();
	initKineticScrolling(settings, form);
	form->addSeparator();
	initMiscellaneous(settings, form);
}

void UserInterface::initFontSize(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	QCheckBox *overrideFontSize =
		new QCheckBox(tr("Override system font size"));
	settings.bindOverrideFontSize(overrideFontSize);
	form->addSpanningRow(overrideFontSize);

	KisSliderSpinBox *fontSize = new KisSliderSpinBox;
	fontSize->setRange(6, 16);
	fontSize->setPrefix(tr("Font size: "));
	fontSize->setSuffix(tr("pt"));
	settings.bindFontSize(fontSize);
	form->addSpanningRow(fontSize);

	form->addSpanningRow(utils::note(
		tr("Changes the font size apply after you restart Drawpile."),
		QSizePolicy::Label));

	QPlainTextEdit *sampleText = new QPlainTextEdit;
	sampleText->setMaximumHeight(100);
	sampleText->setPlaceholderText(
		tr("Type text here to try out the effects of your chosen font size."));
	settings.bindFontSize(this, [sampleText](int size) {
		QFont font = QApplication::font();
		font.setPointSize(size);
		sampleText->setFont(font);
	});
	utils::initKineticScrolling(sampleText);
	form->addSpanningRow(sampleText);

	settings.bindOverrideFontSize(fontSize, &QWidget::setEnabled);
	settings.bindOverrideFontSize(sampleText, &QWidget::setEnabled);
}

void UserInterface::initInterfaceMode(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	QButtonGroup *interfaceMode = form->addRadioGroup(
		tr("Interface mode:"), true,
		{{tr("Desktop"), int(desktop::settings::InterfaceMode::Desktop)},
		 {tr("Small screen (experimental)"),
		  int(desktop::settings::InterfaceMode::SmallScreen)}});
	settings.bindInterfaceMode(interfaceMode);
	form->addRow(
		nullptr, utils::note(
					 tr("Changes to the interface mode apply after you restart "
						"Drawpile."),
					 QSizePolicy::Label));
}


void UserInterface::initKineticScrolling(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
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
		nullptr, utils::note(
					 tr("Changes to kinetic scrolling apply after you restart "
						"Drawpile."),
					 QSizePolicy::Label));
}

void UserInterface::initMiscellaneous(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	using color_widgets::ColorPreview;
	ColorPreview *colorPreview = new ColorPreview;
	colorPreview->setDisplayMode(ColorPreview::DisplayMode::NoAlpha);
	colorPreview->setToolTip(tr("Background color behind the canvas"));
	colorPreview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	colorPreview->setCursor(Qt::PointingHandCursor);
	form->addRow(tr("Color behind canvas:"), colorPreview);
	settings.bindCanvasViewBackgroundColor(this, [colorPreview](const QColor &color) {
		colorPreview->setColor(color);
	});

	connect(
		colorPreview, &ColorPreview::clicked, this,
		[this, &settings] { pickCanvasBackgroundColor(settings); });

	QCheckBox *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	settings.bindCanvasScrollBars(scrollBars);
	form->addRow(tr("Miscellaneous:"), scrollBars);

	QCheckBox *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	settings.bindConfirmLayerDelete(confirmDelete);
	form->addRow(nullptr, confirmDelete);
}

void UserInterface::pickCanvasBackgroundColor(desktop::settings::Settings &settings)
{
	using color_widgets::ColorDialog;
	ColorDialog dlg;
	dlg.setColor(settings.canvasViewBackgroundColor());
	dlg.setAlphaEnabled(false);
	dialogs::applyColorDialogSettings(&dlg);
	if(dlg.exec() == QDialog::Accepted) {
		settings.setCanvasViewBackgroundColor(dlg.color());
	}
}

}
}

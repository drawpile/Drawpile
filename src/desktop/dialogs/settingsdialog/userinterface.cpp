// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFont>
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
	initMiscellaneous(settings, form);
}

void UserInterface::initFontSize(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
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
	form->addSpanningRow(sampleText);
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

void UserInterface::initMiscellaneous(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	QCheckBox *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	settings.bindCanvasScrollBars(scrollBars);
	form->addRow(tr("Miscellaneous:"), scrollBars);

	QCheckBox *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	settings.bindConfirmLayerDelete(confirmDelete);
	form->addRow(nullptr, confirmDelete);
}

}
}

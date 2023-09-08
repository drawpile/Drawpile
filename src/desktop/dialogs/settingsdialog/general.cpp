// SPDX-License-Identifier: GPL-3.0-or-later

#include "cmake-config/config.h"
#include "desktop/dialogs/settingsdialog/general.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/kis_slider_spin_box.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QSpinBox>
#include <QString>
#include <QStyleFactory>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

General::General(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	// TODO: make the form layout work with RTL or go back to a QFormLayout.
	setLayoutDirection(Qt::LeftToRight);
	auto *form = new utils::SanerFormLayout(this);

	initTheme(settings, form);
	initLanguage(settings, form);
	form->addSeparator();
	initMiscUi(settings, form);
	form->addSeparator();
	initUndo(settings, form);
	form->addSpacer();
	initAutosave(settings, form);
	form->addSpacer();
	initSnapshots(settings, form);
}

void General::initAutosave(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *autosaveInterval = new QSpinBox;
	autosaveInterval->setRange(1, 999);
	settings.bindAutoSaveInterval(this, [=](int intervalMsec) {
		autosaveInterval->setValue(intervalMsec / 1000);
	});
	connect(autosaveInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [&](int intervalSec) {
		settings.setAutoSaveInterval(intervalSec * 1000);
	});

	auto *snapshotCountLayout = utils::encapsulate(tr("When enabled, save every %1 minutes"), autosaveInterval);
	snapshotCountLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Autosave:"), snapshotCountLayout);

	form->addRow(nullptr, utils::note(tr("Autosave can be enabled for the current file under <i>File ▸ Autosave</i>."), QSizePolicy::Label));
}

void General::initLanguage(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *language = new QComboBox;
	language->addItem(tr("System"), QString());

	const QLocale localeC = QLocale::c();
	for (const auto *localeName : cmake_config::locales()) {
		QLocale locale(localeName);
		if (locale != localeC) {
			language->addItem(formatLanguage(locale), localeName);
		}
	}

	settings.bindLanguage(language, Qt::UserRole);
	form->addRow(tr("Language:"), language);
	form->addRow(nullptr, utils::note(tr("Language changes apply after you restart Drawpile."), QSizePolicy::Label));
}

QString General::formatLanguage(const QLocale &locale)
{
	bool needsCountryDisambiguation = false;
	for (const char *localeName : cmake_config::locales()) {
		QLocale other(localeName);
		if(locale != other && locale.language() == other.language()) {
			needsCountryDisambiguation = true;
			break;
		}
	}

	if(needsCountryDisambiguation) {
		return tr("%1 (%2) / %3 (%4)")
			.arg(locale.nativeLanguageName())
			.arg(locale.nativeCountryName())
			.arg(QLocale::languageToString(locale.language()))
			.arg(QLocale::countryToString(locale.country()));
	} else {
		return tr("%1 / %2")
			.arg(locale.nativeLanguageName())
			.arg(QLocale::languageToString(locale.language()));
	}
}

void General::initMiscUi(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *scrollBars = new QCheckBox(tr("Show scroll bars on canvas"));
	settings.bindCanvasScrollBars(scrollBars);
	form->addRow(tr("User interface:"), scrollBars);

	auto *confirmDelete = new QCheckBox(tr("Ask before deleting layers"));
	settings.bindConfirmLayerDelete(confirmDelete);
	form->addRow(nullptr, confirmDelete);

	auto *interfaceMode = form->addRadioGroup(nullptr, true, {
		{ tr("Standard Mode"), int(desktop::settings::InterfaceMode::Standard) },
		{ tr("Small Screen Mode"), int(desktop::settings::InterfaceMode::SmallScreen) }
	});
	settings.bindInterfaceMode(interfaceMode);

	auto *fontSize = new KisSliderSpinBox;
	fontSize->setRange(6, 16);
	fontSize->setPrefix(tr("Font size: "));
	fontSize->setSuffix(tr("pt"));
	settings.bindFontSize(fontSize);
	form->addRow(nullptr, fontSize);

	form->addRow(nullptr, utils::note(
		tr("Changes to mode and font size apply after you restart Drawpile."),
		QSizePolicy::Label));

	form->addSeparator();

	auto *enableLogging = new QCheckBox(tr("Write debugging log to file"));
	settings.bindWriteLogFile(enableLogging);
	form->addRow(tr("Logging:"), enableLogging);
}

void General::initSnapshots(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *snapshotCount = new QSpinBox;
	snapshotCount->setRange(0, 99);
	settings.bindEngineSnapshotCount(snapshotCount);
	auto *snapshotCountLayout = utils::encapsulate(tr("Keep %1 canvas snapshots"), snapshotCount);
	snapshotCountLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Canvas snapshots:"), snapshotCountLayout);

	auto *snapshotInterval = new QSpinBox;
	snapshotInterval->setRange(1, 600);
	snapshotInterval->setSingleStep(5);
	settings.bindEngineSnapshotInterval(snapshotInterval);
	settings.bindEngineSnapshotCount(snapshotInterval, &QSpinBox::setEnabled);
	auto *snapshotIntervalLayout = utils::encapsulate(tr("Take one snapshot every %1 seconds"), snapshotInterval);
	snapshotIntervalLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(nullptr, snapshotIntervalLayout);

	form->addRow(nullptr, utils::note(tr("Snapshots can be restored from the <i>Session ▸ Reset…</i> menu."), QSizePolicy::Label));
}

void General::initTheme(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *style = new QComboBox;
	style->addItem(tr("System"), QString());
	const auto styleNames = QStyleFactory::keys();
	for (const auto &styleName : styleNames) {
		// Qt5 does not give a proper name for the macOS style
		if (styleName == "macintosh") {
			style->addItem("macOS", styleName);
		} else {
			style->addItem(styleName, styleName);
		}
	}
	settings.bindThemeStyle(style, Qt::UserRole);
	form->addRow(tr("Style:"), style);

	auto *theme = new QComboBox;
	theme->addItems({
		tr("System"),
		tr("Light"),
		tr("Dark"),
		tr("Krita Bright"),
		tr("Krita Dark"),
		tr("Krita Darker"),
		tr("Qt Fusion"),
		tr("Hotdog Stand")
	});
	settings.bindThemePalette(theme, std::nullopt);
	form->addRow(tr("Color scheme:"), theme);
}

void General::initUndo(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *undoLimit = new QSpinBox;
	undoLimit->setRange(1, 255);
	settings.bindEngineUndoDepth(undoLimit);
	auto *undoLimitLayout = utils::encapsulate(tr("Use %1 undo levels by default"), undoLimit);
	undoLimitLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Session history:"), undoLimitLayout);
}

} // namespace settingsdialog
} // namespace dialogs

// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/files.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Files::Files(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Files::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initFormats(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
#ifndef __EMSCRIPTEN__
	initAutosave(cfg, utils::addFormSection(layout));
#endif
	initLogging(cfg, utils::addFormSection(layout));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	utils::addFormSeparator(layout);
	initDialogs(cfg, utils::addFormSection(layout));
#endif
}

void Files::initAutosave(config::Config *cfg, QFormLayout *form)
{
	QSpinBox *autosaveInterval = new QSpinBox;
	autosaveInterval->setRange(1, 999);
	CFG_BIND_SPINBOX(cfg, AutoSaveIntervalMinutes, autosaveInterval);

	utils::EncapsulatedLayout *snapshotCountLayout = utils::encapsulate(
		tr("When enabled, save every %1 minutes"), autosaveInterval);
	snapshotCountLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Autosave:"), snapshotCountLayout);

	QString autosaveNote = tr(
		"Autosave can be enabled for the current file under File ▸ Autosave.");
#ifdef Q_OS_ANDROID
	// The Android font can't deal with this character.
	autosaveNote.replace(QStringLiteral("▸"), QStringLiteral(">"));
#endif
	form->addRow(nullptr, utils::formNote(autosaveNote));
}

void Files::initDialogs(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *nativeDialogs =
		new QCheckBox(tr("Use system file picker dialogs"));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	CFG_BIND_CHECKBOX(cfg, NativeDialogs, nativeDialogs);
#else
	Q_UNUSED(cfg);
#endif
	form->addRow(tr("Interface:"), nativeDialogs);
}

void Files::initFormats(config::Config *cfg, QFormLayout *form)
{
	//: %1 is a file extension, like ".ora" or ".png"
	QString defaultTemplate = tr("Default (%1)");

	QComboBox *preferredSaveFormat = new QComboBox;
	preferredSaveFormat->addItem(
		defaultTemplate.arg(QStringLiteral(".ora")), QString());
	preferredSaveFormat->addItem(
		tr("OpenRaster (.ora)"), QStringLiteral("ora"));
	preferredSaveFormat->addItem(
		tr("Drawpile Canvas (.dpcs)"), QStringLiteral("dpcs"));
	CFG_BIND_COMBOBOX_USER_STRING(
		cfg, PreferredSaveFormat, preferredSaveFormat);
	form->addRow(tr("Preferred save format:"), preferredSaveFormat);

	QComboBox *preferredExportFormat = new QComboBox;
	preferredExportFormat->addItem(
		defaultTemplate.arg(QStringLiteral(".png")), QString());
	preferredExportFormat->addItem(tr("PNG (.png)"), QStringLiteral("png"));
	preferredExportFormat->addItem(tr("JPEG (.jpg)"), QStringLiteral("jpg"));
	preferredExportFormat->addItem(tr("QOI (.qoi)"), QStringLiteral("qoi"));
	preferredExportFormat->addItem(tr("WEBP (.webp)"), QStringLiteral("webp"));
	preferredExportFormat->addItem(
		tr("OpenRaster (.ora)"), QStringLiteral("ora"));
	preferredExportFormat->addItem(
		tr("Drawpile Canvas (.dpcs)"), QStringLiteral("dpcs"));
	preferredExportFormat->addItem(
		tr("Photoshop Document (.psd)"), QStringLiteral("psd"));
	CFG_BIND_COMBOBOX_USER_STRING(
		cfg, PreferredExportFormat, preferredExportFormat);
	form->addRow(tr("Preferred export format:"), preferredExportFormat);
}

void Files::initLogging(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *enableLogging = new QCheckBox(tr("Write debugging log to file"));
	CFG_BIND_CHECKBOX(cfg, WriteLogFile, enableLogging);
	form->addRow(tr("Logging:"), enableLogging);
}

}
}

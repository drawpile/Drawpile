// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/files.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Files::Files(desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
}

void Files::setUp(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initFormats(settings, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
#ifndef __EMSCRIPTEN__
	initAutosave(settings, utils::addFormSection(layout));
#endif
	initLogging(settings, utils::addFormSection(layout));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	utils::addFormSeparator(layout);
	initDialogs(settings, utils::addFormSection(layout));
#endif
}

void Files::initAutosave(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QSpinBox *autosaveInterval = new QSpinBox;
	autosaveInterval->setRange(1, 999);
	settings.bindAutoSaveIntervalMinutes(autosaveInterval);

	utils::EncapsulatedLayout *snapshotCountLayout = utils::encapsulate(
		tr("When enabled, save every %1 minutes"), autosaveInterval);
	snapshotCountLayout->setControlTypes(QSizePolicy::CheckBox);
	form->addRow(tr("Autosave:"), snapshotCountLayout);

	form->addRow(
		nullptr, utils::formNote(
					 tr("Autosave can be enabled for the current file under "
						"File ▸ Autosave.")));
}

void Files::initDialogs(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QCheckBox *nativeDialogs =
		new QCheckBox(tr("Use system file picker dialogs"));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	settings.bindNativeDialogs(nativeDialogs);
#else
	Q_UNUSED(settings);
#endif
	form->addRow(tr("Interface:"), nativeDialogs);
}

void Files::initFormats(
	desktop::settings::Settings &settings, QFormLayout *form)
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
	settings.bindPreferredSaveFormat(preferredSaveFormat, Qt::UserRole);
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
	settings.bindPreferredExportFormat(preferredExportFormat, Qt::UserRole);
	form->addRow(tr("Preferred export format:"), preferredExportFormat);
}

void Files::initLogging(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	QCheckBox *enableLogging = new QCheckBox(tr("Write debugging log to file"));
	settings.bindWriteLogFile(enableLogging);
	form->addRow(tr("Logging:"), enableLogging);
}

}
}

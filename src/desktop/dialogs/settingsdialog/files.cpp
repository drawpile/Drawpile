// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/files.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace dialogs {
namespace settingsdialog {

Files::Files(config::Config *cfg, QAction *autorecordAction, QWidget *parent)
	: Page(parent)
	, m_autorecordAction(autorecordAction)
{
	init(cfg);
}

void Files::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initFormats(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initAutorecord(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initLogging(cfg, utils::addFormSection(layout));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	utils::addFormSeparator(layout);
	initDialogs(cfg, utils::addFormSection(layout));
#endif
}

void Files::initAutorecord(config::Config *cfg, QFormLayout *form)
{
	m_autoRecordCurrent = new QCheckBox(tr("Enable for the current session"));
	if(m_autorecordAction) {
		m_autoRecordCurrent->setEnabled(m_autorecordAction);
		m_autoRecordCurrent->setChecked(m_autorecordAction->isChecked());
		connect(
			m_autoRecordCurrent, &QCheckBox::clicked, m_autorecordAction,
			&QAction::trigger);
		connect(
			m_autorecordAction, &QAction::changed, this,
			&Files::updateAutoRecordCurrent);
	} else {
		m_autoRecordCurrent->setEnabled(false);
	}
	form->addRow(tr("Autosave:"), m_autoRecordCurrent);

	QCheckBox *autoRecordHost =
		new QCheckBox(tr("When offline or hosting sessions"));
	CFG_BIND_CHECKBOX(cfg, AutoRecordHost, autoRecordHost);
	form->addRow(nullptr, autoRecordHost);

	QCheckBox *autoRecordJoin = new QCheckBox(tr("When joining sessions"));
	CFG_BIND_CHECKBOX(cfg, AutoRecordJoin, autoRecordJoin);
	form->addRow(nullptr, autoRecordJoin);

	KisSliderSpinBox *snapshotInterval = new KisSliderSpinBox;
	snapshotInterval->setRange(1, 60);
	CFG_BIND_SLIDERSPINBOX(
		cfg, AutoRecordSnapshotIntervalMinutes, snapshotInterval);
	form->addRow(nullptr, snapshotInterval);

	auto updateSnapshotIntervalText = [snapshotInterval](int value) {
		utils::encapsulateSpinBoxPrefixSuffix(
			snapshotInterval,
			tr("Snapshot every %1 minute(s)", nullptr, value));
	};
	connect(
		snapshotInterval, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		snapshotInterval, updateSnapshotIntervalText);
	updateSnapshotIntervalText(snapshotInterval->value());

	QString autosaveNote =
		tr("You can control autosaving during a session via the File menu.");
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
	preferredSaveFormat->addItem(
		tr("Drawpile Project (.dppr)"), QStringLiteral("dppr"));
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
		tr("Drawpile Project (.dppr)"), QStringLiteral("dppr"));
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

void Files::updateAutoRecordCurrent()
{
	m_autoRecordCurrent->setEnabled(m_autorecordAction->isEnabled());
	m_autoRecordCurrent->setChecked(m_autorecordAction->isChecked());
}

}
}

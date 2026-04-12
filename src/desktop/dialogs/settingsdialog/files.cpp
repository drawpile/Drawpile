// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/files.h"
#include "desktop/dialogs/projectrecordingsettingsdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/utils/logging.h"
#include "libshared/util/paths.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
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
	initAutorecord(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initFormats(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
	initLogging(cfg, utils::addFormSection(layout));
#ifdef NATIVE_DIALOGS_SETTING_AVAILABLE
	utils::addFormSeparator(layout);
	initDialogs(cfg, utils::addFormSection(layout));
#endif
}

void Files::initAutorecord(config::Config *cfg, QFormLayout *form)
{
	QFrame *currentFrame = new QFrame;
	currentFrame->setFrameShape(QFrame::StyledPanel);
	currentFrame->setFrameShadow(QFrame::Sunken);
	form->addRow(currentFrame);

	QHBoxLayout *currentLayout = new QHBoxLayout(currentFrame);

	currentLayout->addWidget(
		utils::makeIconLabel(
			QIcon::fromTheme(QStringLiteral("backup")), currentFrame));

	QLabel *currentLabel = new QLabel(
		utils::toHtmlWithLink(
			//: The stuff in [] will turn into a link. Don't remove the [] or
			//: replace them with different symbols!
			tr("Changing autorecovery preferences will not affect the status "
			   "or limits of any running sessions. [Click here to manage "
			   "autorecovery on your current session.]"),
			QStringLiteral("#")));
	currentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	currentLabel->setTextFormat(Qt::RichText);
	currentLabel->setWordWrap(true);
	currentLayout->addWidget(currentLabel, 1);
	connect(
		currentLabel, &QLabel::linkActivated, this,
		&Files::projectRecordingSettingsRequested);

	utils::addFormSpacer(form);

	QCheckBox *autoRecordHost =
		new QCheckBox(tr("When offline or hosting sessions"));
	CFG_BIND_CHECKBOX(cfg, AutoRecordHost, autoRecordHost);
	form->addRow(tr("Autorecovery:"), autoRecordHost);

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

	KisDoubleSliderSpinBox *sizeLimit = new KisDoubleSliderSpinBox;
	dialogs::ProjectRecordingSettingsDialog::initSizeLimitSlider(
		sizeLimit, 0.5);
	connect(
		sizeLimit, QOverload<double>::of(&KisDoubleSliderSpinBox::valueChanged),
		sizeLimit, [cfg, sizeLimit](double gib) {
			if(gib < sizeLimit->maximum()) {
				cfg->setAutoRecordSizeLimitGiB(gib);
			} else {
				cfg->setAutoRecordSizeLimitGiB(0.0);
			}
		});
	CFG_BIND_SET_FN(cfg, AutoRecordSizeLimitGiB, this, [sizeLimit](double gib) {
		if(gib < sizeLimit->minimum()) {
			sizeLimit->setValue(sizeLimit->maximum());
		} else {
			sizeLimit->setValue(gib);
		}
	});
	form->addRow(nullptr, sizeLimit);

	form->addRow(
		nullptr,
		utils::formNote(
			dialogs::ProjectRecordingSettingsDialog::getAutorecordNoteText()));
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

	QPushButton *clearButton = new QPushButton(
		QIcon::fromTheme(QStringLiteral("trash-empty")),
		tr("Clear log files…"));
	clearButton->setAutoDefault(false);
	form->addRow(nullptr, clearButton);
	connect(clearButton, &QPushButton::clicked, this, &Files::clearLogFiles);
}

void Files::clearLogFiles()
{
	QFileInfoList infos = utils::allLogFilesExceptCurrent();
	int count = infos.size();
	if(count == 0) {
		utils::showInformation(
			this, tr("Clear Log Files"), tr("No log files to clear found."));
	} else {
		qint64 totalSize = 0;
		for(const QFileInfo &info : infos) {
			totalSize += info.size();
		}
		QMessageBox *box = utils::showQuestion(
			this, tr("Clear Log Files"),
			tr("Do you want to delete %n log file(s)?", nullptr, count),
			tr("It/They take(s) up %1 of space.", nullptr, count)
				.arg(utils::paths::formatFileSize(totalSize)));
		connect(box, &QMessageBox::accepted, this, [infos] {
			for(const QFileInfo &info : infos) {
				QFile file(info.filePath());
				if(!file.remove()) {
					qWarning(
						"Failed to remove log file '%s': %s",
						qUtf8Printable(file.fileName()),
						qUtf8Printable(file.errorString()));
				}
			}
		});
	}
}

}
}

// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/videoexportdialog.h"
#include "desktop/filewrangler.h"
#include "libclient/export/imageseriesexporter.h"
#include "libclient/export/ffmpegexporter.h"
#include "libshared/util/qtcompat.h"

#include "ui_videoexport.h"

#include <QDebug>
#include <QImageWriter>
#include <QFileDialog>
#include <QSettings>
#include <QStandardItemModel>

namespace dialogs {

static QStandardItem *sizeItem(const QString &title, const QVariant &userdata) {
	QStandardItem *item = new QStandardItem(title);
	item->setData(userdata, Qt::UserRole);
	return item;
}

VideoExportDialog::VideoExportDialog(QWidget *parent) :
	QDialog(parent), m_ui(new Ui_VideoExport)
{
	m_ui->setupUi(this);

	m_ui->exportFormatChoice->addItem(tr("Image Series"), VideoExporter::IMAGE_SERIES);
	m_ui->exportFormatChoice->addItem(tr("MP4 Video"), VideoExporter::FFMPEG_MP4);
	m_ui->exportFormatChoice->addItem(tr("WebM Video"), VideoExporter::FFMPEG_WEBM);
	m_ui->exportFormatChoice->addItem(tr("Custom FFmpeg Command"), VideoExporter::FFMPEG_CUSTOM);

	QStandardItemModel *sizes = new QStandardItemModel(this);
	sizes->appendRow(sizeItem(tr("Original"), QVariant(false)));
	sizes->appendRow(sizeItem(tr("Custom:"), QVariant(true)));
	sizes->appendRow(sizeItem("360p", QSize(480, 360)));
	sizes->appendRow(sizeItem("480p", QSize(640, 480)));
	sizes->appendRow(sizeItem("720p", QSize(1280, 720)));
	sizes->appendRow(sizeItem("1080p", QSize(1920, 1080)));
	m_ui->sizeChoice->setModel(sizes);

	// make sure currentIndexChanged gets called if saved setting was something other than Custom
	m_ui->sizeChoice->setCurrentIndex(1);

	connect(m_ui->sizeChoice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		const QVariant isCustom = m_ui->sizeChoice->currentData(Qt::UserRole);
		const bool e = compat::metaTypeFromVariant(isCustom) == QMetaType::Bool && isCustom.toBool();

		m_ui->framewidth->setVisible(e);
		m_ui->frameheight->setVisible(e);
		m_ui->sizeXlabel->setVisible(e);
	});

	// Fill file format box
	const auto formats = QImageWriter::supportedImageFormats();
	for(const QByteArray &fmt : formats)
		m_ui->imageFormatChoice->addItem(fmt);
	m_ui->imageFormatChoice->setCurrentText("png");

	// Load settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	m_ui->exportFormatChoice->setCurrentIndex(cfg.value("formatchoice", 0).toInt());
	m_ui->fps->setValue(cfg.value("fps", 30).toInt());
	m_ui->framewidth->setValue(cfg.value("framewidth", 1280).toInt());
	m_ui->frameheight->setValue(cfg.value("frameheight", 720).toInt());
	m_ui->sizeChoice->setCurrentIndex(cfg.value("sizeChoice", 0).toInt());
	m_ui->ffmpegCustom->setPlainText(cfg.value("customffmpeg").toString());

	// Check for ffmpeg
	m_ui->ffmpegNotFoundWarning->setHidden(FfmpegExporter::checkIsFfmpegAvailable());

	connect(m_ui->exportFormatChoice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VideoExportDialog::updateUi);
	connect(m_ui->fps, QOverload<int>::of(&QSpinBox::valueChanged), this, &VideoExportDialog::updateUi);
	updateUi();
}

VideoExportDialog::~VideoExportDialog()
{
	// Remember settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	cfg.setValue("formatchoice", m_ui->exportFormatChoice->currentIndex());
	cfg.setValue("fps", m_ui->fps->value());
	cfg.setValue("framewidth", m_ui->framewidth->value());
	cfg.setValue("frameheight", m_ui->frameheight->value());
	cfg.setValue("sizeChoice", m_ui->sizeChoice->currentIndex());
	cfg.setValue("customffmpeg", m_ui->ffmpegCustom->toPlainText());

	delete m_ui;
}

void VideoExportDialog::updateUi()
{
	VideoExporter::Format format =
		VideoExporter::Format(m_ui->exportFormatChoice->currentData().toInt());
	if(format == VideoExporter::IMAGE_SERIES) {
		m_ui->optionStack->setCurrentIndex(0);
	} else {
		m_ui->optionStack->setCurrentIndex(1);
		QStringList args = FfmpegExporter::getCommonArguments(m_ui->fps->value());
		if(format == VideoExporter::FFMPEG_MP4) {
			args.append(FfmpegExporter::getDefaultMp4Arguments());
		} else if(format == VideoExporter::FFMPEG_WEBM) {
			args.append(FfmpegExporter::getDefaultWebmArguments());
		} else {
			args.append("<CUSTOM ARGS>");
		}
		args.append({"-y", "<FILENAME>"});

		m_ui->ffmpegBasics->setText("ffmpeg " + args.join(QChar(' ')));
		m_ui->ffmpegCustomLabel->setVisible(format == VideoExporter::FFMPEG_CUSTOM);
		m_ui->ffmpegCustom->setVisible(format == VideoExporter::FFMPEG_CUSTOM);
	}
}

VideoExporter *VideoExportDialog::getExporter()
{
	if(result() != QDialog::Accepted)
		return nullptr;

	// Return appropriate exporter based on exporter format box selection
	VideoExporter::Format format =
		VideoExporter::Format(m_ui->exportFormatChoice->currentData().toInt());
	VideoExporter *ve = format == VideoExporter::IMAGE_SERIES
							? getImageSeriesExporter()
							: getFfmpegExporter(format);

	// Set common settings
	ve->setFps(m_ui->fps->value());

	QVariant sizechoice = m_ui->sizeChoice->currentData(Qt::UserRole);
	if(compat::metaTypeFromVariant(sizechoice) == QMetaType::Bool) {
		if(sizechoice.toBool()) {
			// custom (fixed) size
			ve->setFrameSize(QSize(m_ui->framewidth->value(), m_ui->frameheight->value()));
		} else {
			// keep original size
			ve->setVariableSize(true);
		}
	} else {
		// size preset
		ve->setFrameSize(sizechoice.toSize());
	}

	return ve;
}

VideoExporter *VideoExportDialog::getImageSeriesExporter()
{
	QString dir = FileWrangler{this}.getSaveAnimationFramesPath();
	if(dir.isEmpty()) {
		return nullptr;
	} else {
		ImageSeriesExporter *exporter = new ImageSeriesExporter;
		exporter->setFilePattern(m_ui->filenamePattern->text());
		exporter->setOutputPath(dir);
		exporter->setFormat(m_ui->imageFormatChoice->currentText());
		return exporter;
	}
}

VideoExporter *VideoExportDialog::getFfmpegExporter(VideoExporter::Format format)
{
	FileWrangler fw{this};
	QString filename;
	if(format == VideoExporter::FFMPEG_MP4) {
		filename = FileWrangler{this}.getSaveFfmpegMp4Path();
	} else if(format == VideoExporter::FFMPEG_WEBM) {
		filename = FileWrangler{this}.getSaveFfmpegWebmPath();
	} else {
		filename = FileWrangler{this}.getSaveFfmpegCustomPath();
	}

	if(filename.isEmpty()) {
		return nullptr;
	} else {
		return new FfmpegExporter{
			format, filename, m_ui->ffmpegCustom->toPlainText()};
	}
}

}

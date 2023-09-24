// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/videoexportdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "libclient/export/imageseriesexporter.h"
#include "libclient/export/ffmpegexporter.h"
#include "libshared/util/qtcompat.h"

#include "ui_videoexport.h"

#include <QDebug>
#include <QImageWriter>
#include <QFileDialog>
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

	auto &settings = dpApp().settings();
	settings.bindVideoExportFormat(m_ui->exportFormatChoice, std::nullopt);
	settings.bindVideoExportFormat(this, &VideoExportDialog::updateUi);
	settings.bindVideoExportFrameRate(m_ui->fps);
	settings.bindVideoExportFrameRate(this, &VideoExportDialog::updateUi);
	settings.bindVideoExportFrameWidth(m_ui->framewidth);
	settings.bindVideoExportFrameHeight(m_ui->frameheight);
	settings.bindVideoExportSizeChoice(m_ui->sizeChoice, std::nullopt);
	settings.bindVideoExportFfmpegPath(m_ui->ffmpegPathEdit);
	settings.bindVideoExportFfmpegPath(this, &VideoExportDialog::updateUi);
	settings.bindVideoExportCustomFfmpeg(m_ui->ffmpegCustom);
	connect(
		m_ui->ffmpegPathButton, &QAbstractButton::clicked, this,
		&VideoExportDialog::chooseFfmpegPath);

	connect(
		m_ui->buttonBox, &QDialogButtonBox::accepted, this,
		&VideoExportDialog::chooseExportPath);

	updateUi();
}

VideoExportDialog::~VideoExportDialog()
{
	delete m_ui;
}

void VideoExportDialog::chooseFfmpegPath()
{
#ifdef Q_OS_WINDOWS
	QString executableFilter =
		//: Used for picking a kind of file, used like "Executables (*.exe)".
		QStringLiteral("%1 (*.exe)").arg(tr("Executables"));
#else
	QString executableFilter;
#endif
	QString ffmpegPath = QFileDialog::getOpenFileName(
		this, tr("Choose ffmpeg path"), QString(), executableFilter);
	if(!ffmpegPath.isEmpty()) {
		m_ui->ffmpegPathEdit->setText(ffmpegPath);
	}
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

		m_ui->ffmpegBasics->setText(QStringLiteral("%1 %2").arg(
			getFfmpegPath(), args.join(QChar(' '))));
		m_ui->ffmpegCustomLabel->setVisible(format == VideoExporter::FFMPEG_CUSTOM);
		m_ui->ffmpegCustom->setVisible(format == VideoExporter::FFMPEG_CUSTOM);
	}
}

void VideoExportDialog::chooseExportPath()
{
	m_format =
		VideoExporter::Format(m_ui->exportFormatChoice->currentData().toInt());

	switch(m_format) {
	case VideoExporter::IMAGE_SERIES:
		m_exportPath = FileWrangler{this}.getSaveAnimationFramesPath();
		break;
	case VideoExporter::FFMPEG_MP4:
		m_exportPath = FileWrangler{this}.getSaveFfmpegMp4Path();
		break;
	case VideoExporter::FFMPEG_WEBM:
		m_exportPath = FileWrangler{this}.getSaveFfmpegWebmPath();
		break;
	case VideoExporter::FFMPEG_CUSTOM:
		m_exportPath = FileWrangler{this}.getSaveFfmpegCustomPath();
		break;
	default:
		qWarning("Unknown video exporter format %d", int(m_format));
		m_exportPath.clear();
		break;
	}

	if(!m_exportPath.isEmpty()) {
		accept();
	}
}

VideoExporter *VideoExportDialog::getExporter()
{
	if(result() != QDialog::Accepted || m_exportPath.isEmpty()) {
		return nullptr;
	}

	// Return appropriate exporter based on exporter format box selection
	VideoExporter *ve = m_format == VideoExporter::IMAGE_SERIES
							? getImageSeriesExporter()
							: getFfmpegExporter();

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

QString VideoExportDialog::getFfmpegPath()
{
	QString path = m_ui->ffmpegPathEdit->text().trimmed();
	return path.isEmpty() ? QStringLiteral("ffmpeg") : path;
}

VideoExporter *VideoExportDialog::getImageSeriesExporter()
{
	ImageSeriesExporter *exporter = new ImageSeriesExporter;
	exporter->setFilePattern(m_ui->filenamePattern->text());
	exporter->setOutputPath(m_exportPath);
	exporter->setFormat(m_ui->imageFormatChoice->currentText());
	return exporter;
}

VideoExporter *VideoExportDialog::getFfmpegExporter()
{
	return new FfmpegExporter{
		m_format, getFfmpegPath(), m_exportPath,
		m_ui->ffmpegCustom->toPlainText()};
}

}

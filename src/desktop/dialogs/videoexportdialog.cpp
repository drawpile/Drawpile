/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include "desktop/dialogs/videoexportdialog.h"
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

#ifdef Q_OS_MAC
	// Flat style doesn't look good on Mac
	for(QGroupBox *box : findChildren<QGroupBox*>()) {
		box->setFlat(false);
	}
#endif

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
	m_ui->fps->setValue(cfg.value("fps", 25).toInt());
	m_ui->framewidth->setValue(cfg.value("framewidth", 1280).toInt());
	m_ui->frameheight->setValue(cfg.value("frameheight", 720).toInt());
	m_ui->sizeChoice->setCurrentIndex(cfg.value("sizeChoice", 0).toInt());

	m_ui->ffmpegUseCustom->setChecked(cfg.value("usecustomffmpeg").toBool());
	m_ui->ffmpegCustom->setPlainText(cfg.value("customffmpeg").toString());

	// Check for ffmpeg
	m_ui->ffmpegNotFoundWarning->setHidden(FfmpegExporter::checkIsFfmpegAvailable());

	connect(m_ui->fps, QOverload<int>::of(&QSpinBox::valueChanged), this, &VideoExportDialog::updateFfmpegArgumentPreview);
	connect(m_ui->ffmpegUseCustom, &QCheckBox::toggled, this, &VideoExportDialog::updateFfmpegArgumentPreview);
	updateFfmpegArgumentPreview();
}

VideoExportDialog::~VideoExportDialog()
{
	// Remember settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	cfg.setValue("fps", m_ui->fps->value());
	cfg.setValue("framewidth", m_ui->framewidth->value());
	cfg.setValue("frameheight", m_ui->frameheight->value());
	cfg.setValue("sizeChoice", m_ui->sizeChoice->currentIndex());
	cfg.setValue("usecustomffmpeg", m_ui->ffmpegUseCustom->isChecked());
	cfg.setValue("customffmpeg", m_ui->ffmpegCustom->toPlainText());

	delete m_ui;
}

void VideoExportDialog::updateFfmpegArgumentPreview()
{
	QStringList args = FfmpegExporter::getCommonArguments(m_ui->fps->value());
	if(!m_ui->ffmpegUseCustom->isChecked())
		args << FfmpegExporter::getDefaultArguments();
	else
		args << "<CUSTOM ARGS>";

	args << "-y" << "<FILENAME>";

	m_ui->ffmpegBasics->setText("ffmpeg " + args.join(QChar(' ')));
	m_ui->ffmpegCustom->setEnabled(m_ui->ffmpegUseCustom->isChecked());
}

VideoExporter *VideoExportDialog::getExporter()
{
	if(result() != QDialog::Accepted)
		return nullptr;

	// Return appropriate exporter based on exporter format box selection
	VideoExporter *ve = nullptr;
	switch(m_ui->exportFormatChoice->currentIndex()) {
	case 0: ve = getImageSeriesExporter(); break;
	case 1: ve = getFfmpegExporter(); break;
	}

	if(!ve)
		return nullptr;

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

	const QString dir = QFileDialog::getExistingDirectory(
		this,
		tr("Select output directory"),
		QSettings().value("window/lastpath").toString()
	);

	if(dir.isEmpty())
		return nullptr;

	QSettings().setValue("window/lastpath", dir);

	ImageSeriesExporter *exporter = new ImageSeriesExporter;

	exporter->setFilePattern(m_ui->filenamePattern->text());
	exporter->setOutputPath(dir);
	exporter->setFormat(m_ui->imageFormatChoice->currentText());

	return exporter;
}

VideoExporter *VideoExportDialog::getFfmpegExporter()
{
	const QString outfile = QFileDialog::getSaveFileName(
		this,
		tr("Export video"),
		QSettings().value("window/lastpath").toString(),
		"MKV (*.mkv);;WebM (*.webm);;AVI (*.avi);;All files (*)"
	);
	if(outfile.isEmpty())
		return nullptr;

	QSettings().setValue("window/lastpath", QFileInfo(outfile).dir().absolutePath());

	// Set exporter settings
	FfmpegExporter *exporter = new FfmpegExporter;
	exporter->setFilename(outfile);
	if(m_ui->ffmpegUseCustom)
		exporter->setCustomArguments(m_ui->ffmpegCustom->toPlainText());

	return exporter;
}

}

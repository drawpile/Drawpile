/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QDebug>
#include <QImageWriter>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>

#include "config.h"

#include "videoexportdialog.h"
#include "export/imageseriesexporter.h"
#include "export/ffmpegexporter.h"

#include "ui_videoexport.h"

namespace dialogs {

VideoExportDialog::VideoExportDialog(QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_VideoExport;
	_ui->setupUi(this);

	QMenu *sizepresets = new QMenu(_ui->sizePreset);
	sizepresets->addAction("360p")->setProperty("fs", QSize(480, 360));
	sizepresets->addAction("480p")->setProperty("fs", QSize(640, 480));
	sizepresets->addAction("720p")->setProperty("fs", QSize(1280, 720));
	sizepresets->addAction("1080p")->setProperty("fs", QSize(1920, 1080));
	_ui->sizePreset->setMenu(sizepresets);
	connect(sizepresets, &QMenu::triggered, [this](const QAction *a) {
		const QSize s = a->property("fs").toSize();
		_ui->framewidth->setValue(s.width());
		_ui->frameheight->setValue(s.height());
	});

	connect(_ui->exportFormatChoice, SIGNAL(activated(int)), this, SLOT(selectExportFormat(int)));
	connect(_ui->videoFormat, SIGNAL(activated(QString)), this, SLOT(selectContainerFormat(QString)));

	connect(_ui->pickSoundtrack, &QToolButton::clicked, [this]() {
		QString file = QFileDialog::getOpenFileName(this, tr("Select soundtrack"), _lastpath,
			tr("Sound files (%1)").arg("*.wav *.mp3 *.aac *.ogg *.flac") + ";;" +
			tr("All files (*)")
		);
		if(!file.isEmpty())
			_ui->soundtrack->setText(file);
	});

	// Fill file format box
	foreach(const QByteArray &fmt, QImageWriter::supportedImageFormats())
		_ui->imageFormatChoice->addItem(fmt);
	_ui->imageFormatChoice->setCurrentText("png");

	if(FfmpegExporter::isFfmpegAvailable())
		_ui->noFfmpegWarning->setHidden(true);

	// Load settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	_ui->fps->setValue(cfg.value("fps", 25).toInt());
	_ui->framewidth->setValue(cfg.value("framewidth", 1280).toInt());
	_ui->frameheight->setValue(cfg.value("frameheight", 720).toInt());
	_ui->keepOriginalSize->setChecked(cfg.value("keepOriginalSize", false).toBool());
	_lastpath = cfg.value("lastpath", "").toString();

}

VideoExportDialog::~VideoExportDialog()
{
	// Remember settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	cfg.setValue("fps", _ui->fps->value());
	cfg.setValue("framewidth", _ui->framewidth->value());
	cfg.setValue("frameheight", _ui->frameheight->value());
	cfg.setValue("keepOriginalSize", _ui->keepOriginalSize->isChecked());
	cfg.setValue("lastpath", _lastpath);

	delete _ui;
}

void VideoExportDialog::selectExportFormat(int idx)
{
	bool allow_variable_size = (idx == 0);

	if(!allow_variable_size)
		_ui->keepOriginalSize->setChecked(false);
	_ui->keepOriginalSize->setEnabled(allow_variable_size);
}

void VideoExportDialog::selectContainerFormat(const QString &fmt)
{
	if(fmt == "WebM") {
		_ui->videoCodec->setCurrentText("VP8");
		_ui->audioCodec->setCurrentText("Vorbis");
		_ui->videoCodec->setEnabled(false);
		_ui->audioCodec->setEnabled(false);
	} else {
		_ui->videoCodec->setEnabled(true);
		_ui->audioCodec->setEnabled(true);
	}
}

VideoExporter *VideoExportDialog::getExporter()
{
	if(result() != QDialog::Accepted)
		return 0;

	// Return appropriate exporter based on exporter format box selection
	VideoExporter *ve = 0;
	switch(_ui->exportFormatChoice->currentIndex()) {
	case 0: ve = getImageSeriesExporter(); break;
	case 1: ve = getFfmpegExporter(); break;
	}

	if(!ve)
		return 0;

	// Set common settings
	ve->setFps(_ui->fps->value());

	if(_ui->keepOriginalSize->isChecked())
		ve->setVariableSize(true);
	else
		ve->setFrameSize(QSize(_ui->framewidth->value(), _ui->frameheight->value()));

	return ve;
}

VideoExporter *VideoExportDialog::getImageSeriesExporter()
{

	const QString dir = QFileDialog::getExistingDirectory(this, tr("Select output directory"), _lastpath);
	if(dir.isEmpty())
		return 0;

	_lastpath = dir;

	ImageSeriesExporter *exporter = new ImageSeriesExporter;

	exporter->setFilePattern(_ui->filenamePattern->text());
	exporter->setOutputPath(dir);
	exporter->setFormat(_ui->imageFormatChoice->currentText());

	return exporter;
}

VideoExporter *VideoExportDialog::getFfmpegExporter()
{
	// Select output file name. Constrain file extension based on selected type
	const QString format = _ui->videoFormat->currentText();
	QString formatext;
	if(format == "AVI")
		formatext = ".avi";
	else if(format == "Matroska")
		formatext = ".mkv";
	else if(format == "WebM")
		formatext = ".webm";
	else {
		qWarning() << "Unhandled video format:" << format;
		Q_ASSERT(false);
		formatext = ".*";
	}

	QString outfile = QFileDialog::getSaveFileName(this, tr("Export video"), _lastpath, tr("%1 files (*%2)").arg(format).arg(formatext));
	if(outfile.isEmpty())
		return 0;

	if(!outfile.endsWith(formatext, Qt::CaseInsensitive))
		outfile.append(formatext);

	_lastpath = QFileInfo(outfile).dir().absolutePath();

	// Set exporter settings
	FfmpegExporter *exporter = new FfmpegExporter;
	qDebug() << "EXPORTING VIDEO" << outfile;
	exporter->setFilename(outfile);
	exporter->setSoundtrack(_ui->soundtrack->text());
	exporter->setFormat(_ui->videoFormat->currentText());
	exporter->setVideoCodec(_ui->videoCodec->currentText());
	exporter->setAudioCodec(_ui->audioCodec->currentIndex() ? _ui->audioCodec->currentText() : QString());
	exporter->setQuality(_ui->videoquality->currentIndex());

	return exporter;
}

}

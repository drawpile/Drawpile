/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QImageWriter>
#include <QFileDialog>
#include <QSettings>

#include "videoexportdialog.h"
#include "export/imageseriesexporter.h"

#include "ui_videoexport.h"

namespace dialogs {

VideoExportDialog::VideoExportDialog(QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_VideoExport;
	_ui->setupUi(this);

	connect(_ui->pickOutputDirectory, SIGNAL(clicked()), this, SLOT(selectOutputDirectory()));

	// Fill file format box
	foreach(const QByteArray &fmt, QImageWriter::supportedImageFormats())
		_ui->imageFormatChoice->addItem(fmt);
	_ui->imageFormatChoice->setCurrentText("png");

	// Load settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	_ui->fps->setValue(cfg.value("fps", 25).toInt());
	_ui->keepOriginalSize->setChecked(cfg.value("keepOriginalSize", false).toBool());
	_ui->outputDirectory->setText(cfg.value("outputdirectory", "").toString());

}

VideoExportDialog::~VideoExportDialog()
{
	// Remember settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	cfg.setValue("fps", _ui->fps->value());
	cfg.setValue("keepOriginalSize", _ui->keepOriginalSize->isChecked());
	cfg.setValue("outputdirectory", _ui->outputDirectory->text());

	delete _ui;
}

void VideoExportDialog::selectOutputDirectory()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Select output directory"), _ui->outputDirectory->text());
	if(!dir.isEmpty())
		_ui->outputDirectory->setText(dir);
}

VideoExporter *VideoExportDialog::getExporter()
{
	if(result() != QDialog::Accepted)
		return 0;

	// Return appropriate exporter based on exporter format box selection
	VideoExporter *ve = getImageSeriesExporter();

	if(!ve)
		return 0;

	// Set common settings
	ve->setFps(_ui->fps->value());
	ve->setScaleToOriginal(_ui->keepOriginalSize->isChecked());

	return ve;
}

VideoExporter *VideoExportDialog::getImageSeriesExporter()
{
	ImageSeriesExporter *exporter = new ImageSeriesExporter;

	exporter->setFilePattern(_ui->filenamePattern->text());
	exporter->setOutputPath(_ui->outputDirectory->text());
	exporter->setFormat(_ui->imageFormatChoice->currentText());

	return exporter;
}
}

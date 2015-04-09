/*
   Drawpile - a collaborative drawing program.

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

#include "config.h"

#include "videoexportdialog.h"
#include "export/imageseriesexporter.h"
#include "export/ffmpegexporter.h"

#include "widgets/colorbutton.h"
using widgets::ColorButton;

#include "ui_videoexport.h"

#include <QDebug>
#include <QImageWriter>
#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <QStandardItemModel>

namespace dialogs {

static QStandardItem *sizeItem(const QString &title, const QVariant &userdata) {
	QStandardItem *item = new QStandardItem(title);
	item->setData(userdata, Qt::UserRole);
	return item;
}

VideoExportDialog::VideoExportDialog(QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_VideoExport;
	_ui->setupUi(this);

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
	_ui->sizeChoice->setModel(sizes);

	// make sure currentIndexChanged gets called if saved setting was something other than Custom
	_ui->sizeChoice->setCurrentIndex(1);

	connect(_ui->sizeChoice, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this]() {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
		QVariant isCustom = _ui->sizeChoice->currentData(Qt::UserRole);
#else
		QVariant isCustom = _ui->sizeChoice->itemData(_ui->sizeChoice->currentIndex(), Qt::UserRole);
#endif
		bool e = isCustom.type() == QVariant::Bool && isCustom.toBool();

		_ui->framewidth->setVisible(e);
		_ui->frameheight->setVisible(e);
		_ui->sizeXlabel->setVisible(e);
	});

	connect(_ui->exportFormatChoice, SIGNAL(activated(int)), this, SLOT(selectExportFormat(int)));
	connect(_ui->videoFormat, SIGNAL(activated(QString)), this, SLOT(selectContainerFormat(QString)));

	connect(_ui->pickSoundtrack, &QToolButton::clicked, [this]() {
		QString file = QFileDialog::getOpenFileName(this, tr("Select soundtrack"), _lastpath,
			tr("Sound files (%1)").arg("*.wav *.mp3 *.aac *.ogg *.flac") + ";;" +
			QApplication::tr("All files (*)")
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

	// Animation settings
	connect(_ui->animBg, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int idx) {
		_ui->animBgColor->setVisible(idx==2);
		if(idx==1 && _ui->animFirstLayer->value()==1)
			_ui->animFirstLayer->setValue(2);
	});

	// Load settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	_ui->fps->setValue(cfg.value("fps", 25).toInt());
	_ui->framewidth->setValue(cfg.value("framewidth", 1280).toInt());
	_ui->frameheight->setValue(cfg.value("frameheight", 720).toInt());
	_ui->sizeChoice->setCurrentIndex(cfg.value("sizeChoice", 0).toInt());
	//_ui->keepOriginalSize->setChecked(cfg.value("keepOriginalSize", false).toBool());
	_ui->animBgColor->setColor(cfg.value("bgcolor", QColor(255,255,255)).value<QColor>());
	_ui->animBg->setCurrentIndex(cfg.value("animbg", 1).toInt());

	_ui->animBgColor->setVisible(_ui->animBg->currentIndex()==2);

	_lastpath = cfg.value("lastpath", "").toString();

	// Animation settings are shown only when we ask for them
	_ui->animOpts->setVisible(false);

}

VideoExportDialog::~VideoExportDialog()
{
	// Remember settings
	QSettings cfg;
	cfg.beginGroup("videoexport");
	cfg.setValue("fps", _ui->fps->value());
	cfg.setValue("framewidth", _ui->framewidth->value());
	cfg.setValue("frameheight", _ui->frameheight->value());
	cfg.setValue("sizeChoice", _ui->sizeChoice->currentIndex());
	cfg.setValue("lastpath", _lastpath);
	cfg.setValue("bgcolor", _ui->animBgColor->color());
	cfg.setValue("animbg", _ui->animBg->currentIndex());
	delete _ui;
}

void VideoExportDialog::showAnimationSettings(int layercount)
{
	_ui->animFirstLayer->setMaximum(layercount);
	_ui->animLastLayer->setMaximum(layercount);
	_ui->animFirstLayer->setValue(1);
	_ui->animLastLayer->setValue(layercount);
	_ui->animOpts->setVisible(true);
	if(_ui->animBg->currentIndex()==1 && _ui->animFirstLayer->value()==1)
		_ui->animFirstLayer->setValue(2);
}

void VideoExportDialog::selectExportFormat(int idx)
{
	bool allow_variable_size = (idx == 0);

	static_cast<QStandardItemModel*>(_ui->sizeChoice->model())->item(0)->setEnabled(allow_variable_size);
	if(_ui->sizeChoice->currentIndex() == 0)
		_ui->sizeChoice->setCurrentIndex(1);
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

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	QVariant sizechoice = _ui->sizeChoice->currentData(Qt::UserRole);
#else
	QVariant sizechoice = _ui->sizeChoice->itemData(_ui->sizeChoice->currentIndex(), Qt::UserRole);
#endif
	if(sizechoice.type() == QVariant::Bool) {
		if(sizechoice.toBool()) {
			// custom (fixed) size
			ve->setFrameSize(QSize(_ui->framewidth->value(), _ui->frameheight->value()));
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

int VideoExportDialog::getFirstLayer() const {
	return qMin(_ui->animFirstLayer->value(), _ui->animLastLayer->value());
}

int VideoExportDialog::getLastLayer() const {
	return qMax(_ui->animFirstLayer->value(), _ui->animLastLayer->value());
}

bool VideoExportDialog::useBackgroundLayer() const
{
	return _ui->animBg->currentIndex() == 1;
}

QColor VideoExportDialog::animationBackground() const
{
	if(_ui->animBg->currentIndex()==0)
		return QColor(0, 0, 0, 0);
	return _ui->animBgColor->color();
}

}

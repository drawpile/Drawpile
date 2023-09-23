// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFileInfo>
#include <QImageWriter>
#include <QDir>

#include "libclient/export/imageseriesexporter.h"

ImageSeriesExporter::ImageSeriesExporter(QObject *parent)
	: VideoExporter(parent)
{
}

void ImageSeriesExporter::writeFrame(const QImage &image, int repeat)
{
	for(int f=1;f<=repeat;++f) {
		QString filename = _filepattern;
		filename.replace(QStringLiteral("{F}"), QString("%1").arg(frame() + f, 5, 10, QChar('0')));
		filename.replace(QStringLiteral("{E}"), _format);

		QString fullpath = QFileInfo(QDir(_path), filename).absoluteFilePath();

		QImageWriter writer(fullpath, _format);
		if(!writer.write(image)) {
			emit exporterError(writer.errorString());
			return;
		}
	}
	emit exporterReady();
}

void ImageSeriesExporter::initExporter()
{
	emit exporterReady();
}

void ImageSeriesExporter::shutdownExporter()
{
	emit exporterFinished(false);
}

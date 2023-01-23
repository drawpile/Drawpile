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
	emit exporterFinished();
}

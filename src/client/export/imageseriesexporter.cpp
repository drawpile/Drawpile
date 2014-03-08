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

#include <QFileInfo>
#include <QImageWriter>
#include <QDir>

#include "imageseriesexporter.h"

ImageSeriesExporter::ImageSeriesExporter(QObject *parent)
	: VideoExporter(parent)
{
}

void ImageSeriesExporter::writeFrame(const QImage &image, int repeat)
{
	for(int f=1;f<=repeat;++f) {
		QString filename = _filepattern;
		filename.replace(QLatin1Literal("{F}"), QString("%1").arg(frame() + f, 5, 10, QLatin1Char('0')));
		filename.replace(QLatin1Literal("{E}"), _format);

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

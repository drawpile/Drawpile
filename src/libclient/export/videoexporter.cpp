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

#include <QImage>
#include <QPainter>

#include "libclient/export/videoexporter.h"

VideoExporter::VideoExporter(QObject *parent)
	: QObject(parent), _fps(25), _variablesize(true), _frame(0), _targetsize(0, 0)
{
}

void VideoExporter::setFrameSize(const QSize &size)
{
	_targetsize = size;
	setVariableSize(false);
}

void VideoExporter::finish()
{
	shutdownExporter();
}

void VideoExporter::saveFrame(const QImage &image, int count)
{
	Q_ASSERT(count>0);
	Q_ASSERT(!image.isNull());

	if(count<=0 || image.isNull())
		return;

	QImage frameImage = image;

	if(isVariableSize() && !variableSizeSupported()) {
		// If exporter does not support variable size, fix frame
		// size to the size of the first image.
		setFrameSize(image.size());
	}

	if(!isVariableSize() && image.size() != _targetsize) {
		QImage newframe = QImage(_targetsize, QImage::Format_RGB32);
		newframe.fill(Qt::black);

		QSize newsize = image.size().scaled(_targetsize, Qt::KeepAspectRatio);

		QRect rect(
					QPoint(
						_targetsize.width()/2 - newsize.width()/2,
						_targetsize.height()/2 - newsize.height()/2
					),
					newsize
		);

		QPainter painter(&newframe);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawImage(rect, image, QRect(QPoint(), image.size()));
		painter.end();

		frameImage = newframe;
	}

	writeFrame(frameImage, count);
	_frame += count;
}

void VideoExporter::start()
{
	initExporter();
}

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

#include <QImage>
#include <QPainter>

#include "videoexporter.h"

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

void VideoExporter::saveFrame(const QImage &image)
{
	++_frame;

	if(!isVariableSize() && image.size() != _targetsize) {
		QImage newframe = QImage(_targetsize, QImage::Format_ARGB32);
		newframe.fill(Qt::black);

		QSize newsize = image.size().scaled(_targetsize, Qt::KeepAspectRatio);

		QRect rect(
					QPoint(
						_targetsize.width()/2 - newsize.width()/2,
						_targetsize.height()/2 - newsize.height()/2
					),
					newsize
		);

		{
			QPainter painter(&newframe);
			painter.drawImage(rect, image, QRect(QPoint(), image.size()));
		}

		writeFrame(newframe);

	} else {
		writeFrame(image);
	}
}

void VideoExporter::start()
{
	initExporter();
}

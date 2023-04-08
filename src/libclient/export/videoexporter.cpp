// SPDX-License-Identifier: GPL-3.0-or-later

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

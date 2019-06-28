/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "webmexporter.h"
#include "webmencoder.h"

#include <QThread>

struct WebmExporter::Private {
	QThread *encoderThread = nullptr;
	WebmEncoder *encoder = nullptr;

	QString filename;
};

WebmExporter::WebmExporter(QObject *parent)
	: VideoExporter(parent), d(new Private)
{
}

WebmExporter::~WebmExporter()
{
	if(d->encoderThread) {
		d->encoderThread->quit();
		d->encoderThread->wait();
	}
	delete d->encoder;
	delete d;
}

void WebmExporter::setFilename(const QString &filename)
{
	d->filename = filename;
}

void WebmExporter::initExporter()
{
	d->encoderThread = new QThread(this);

	d->encoder = new WebmEncoder(d->filename);
	d->encoder->moveToThread(d->encoderThread);

	connect(d->encoderThread, &QThread::started, d->encoder, &WebmEncoder::open);

	connect(d->encoder, &WebmEncoder::encoderReady, this, &WebmExporter::exporterReady);
	connect(d->encoder, &WebmEncoder::encoderError, this, &WebmExporter::exporterError);
	connect(d->encoder, &WebmEncoder::encoderFinished, this, &WebmExporter::exporterFinished);

	d->encoderThread->start();
}

void WebmExporter::startExporter()
{
	Q_ASSERT(d->encoder);
	Q_ASSERT(framesize().width() > 0 && framesize().height()>0);

	QMetaObject::invokeMethod(d->encoder, "start", Qt::AutoConnection,
		Q_ARG(int, framesize().width()),
		Q_ARG(int, framesize().height()),
		Q_ARG(int, fps())
	);
}

void WebmExporter::writeFrame(const QImage &image, int repeat)
{
	Q_ASSERT(d->encoder);

	QMetaObject::invokeMethod(d->encoder, "writeFrame", Qt::AutoConnection,
		Q_ARG(QImage, image),
		Q_ARG(int, repeat)
	);
}

void WebmExporter::shutdownExporter()
{
	Q_ASSERT(d->encoder);

	QMetaObject::invokeMethod(d->encoder, "finish", Qt::AutoConnection);
}


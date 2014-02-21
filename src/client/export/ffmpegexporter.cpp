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

#include <QDebug>
#include <QImage>
#include <QBuffer>

#define FFMPEG_EXECUTABLE "ffmpeg"

#include "ffmpegexporter.h"

FfmpegExporter::FfmpegExporter(QObject *parent)
	: VideoExporter(parent), _encoder(0), _quality(1)
{
}

void FfmpegExporter::initExporter()
{
	Q_ASSERT(!_encoder);

	QStringList args;

	// Image input
	args << "-f" << "image2pipe" << "-c:v" << "bmp" << "-i" << "-";

	// Soundtrack input
	if(!_soundtrack.isEmpty()) {
		args << "-i" << _soundtrack;
	}

	// Framerate
	args << "-r" << QString::number(fps());

	// Output format is deduced automatically from the file name,
	// but we need to set the codec options
	// Possible quality values are: [0] low, [1] normal, [2] good, [3] very good
	args << "-c:v";
	if(_videoCodec == "H.264") {
		args << "libx264";

		switch(_quality) {
		case 0: args << "-crf" << "32" << "-preset" << "medium"; break;
		case 2: args << "-crf" << "18" << "-preset" << "slow"; break;
		case 3: args << "-crf" << "16" << "-preset" << "veryslow"; break;
		case 1:
		default: args << "-crf" << "23" << "-preset" << "slow"; break;
		}

	} else if(_videoCodec == "VP8") {
		args << "libvpx";

		switch(_quality) {
		case 0: args << "-crf" << "32" << "-b:v" << "0.5M"; break;
		case 2: args << "-crf" << "10" << "-b:v" << "1M"; break;
		case 3: args << "-crf" << "5" << "-b:v" << "2M"; break;
		case 1:
		default: args << "-crf" << "15" << "-b:v" << "1M"; break;
		}

	} else if(_videoCodec == "Theora") {
		args << "libtheora";

		switch(_quality) {
		case 0: args << "-qscale:v" << "3"; break;
		case 2: args << "-qscale:v" << "7"; break;
		case 3: args << "-qscale:v" << "10"; break;
		case 1:
		default: args << "-qscale:v" << "6"; break;
		}

	} else {
		qWarning() << "unhandled video codec:" << _videoCodec;
		// no quality adjustment for unknown codecs!
		args << _videoCodec.toLower();
	}

	if(!_soundtrack.isEmpty()) {
		args << "-c:a";
		if(_audioCodec.isEmpty())
			args << "copy";
		else if(_audioCodec == "MP3")
			args << "libmp3lame";
		else if(_audioCodec == "Vorbis")
			args << "libvorbis";
		else
			args << _audioCodec.toLower();

		// TODO: this should cut the audio down to the length of the video
		// but using it will trigger a QProcess write error.
		//args << "-shortest";
	}

	// Output file (overwrite)
	args << "-y" << _filename;

	_encoder = new QProcess(this);
	_encoder->setProcessChannelMode(QProcess::ForwardedChannels);

	connect(_encoder, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
	connect(_encoder, SIGNAL(bytesWritten(qint64)), this, SLOT(bytesWritten(qint64)));
	connect(_encoder, SIGNAL(started()), this, SIGNAL(exporterReady()));
	connect(_encoder, SIGNAL(finished(int)), this, SIGNAL(exporterFinished()));

	qDebug() << "Encoding: ffmpeg" << args;

	_encoder->start(FFMPEG_EXECUTABLE, args);
}

void FfmpegExporter::processError(QProcess::ProcessError error)
{
	qWarning() << "Ffmpeg error:" << error;
	switch(error) {
	case QProcess::FailedToStart:
		emit exporterError(tr("Couldn't start ffmpeg!"));
		break;
	case QProcess::Crashed:
		emit exporterError(tr("Ffmpeg crashed!"));
		break;
	default:
		emit exporterError(tr("Ffmpeg process error"));
		break;
	}
}

void FfmpegExporter::writeFrame(const QImage &image)
{
	Q_ASSERT(_writebuffer.isEmpty());
	//qDebug() << "WRITING FRAME" << frame();
	{
		QBuffer buf(&_writebuffer);
		buf.open(QIODevice::ReadWrite);
		image.save(&buf, "BMP");
		_written = 0;
		_chunk = 0;
	}
	bytesWritten(0);
}

void FfmpegExporter::bytesWritten(qint64 bytes)
{
	const qint64 bufsize = _writebuffer.size();
	_written += bytes;
	_chunk -= bytes;
	Q_ASSERT(_written <= bufsize);
	Q_ASSERT(_chunk >= 0);

	//qDebug() << "wrote" << bytes << "bytes:" << _written << "of" << bufsize << QString("(%1%)").arg(_written/qreal(bufsize)*100, 0, 'f', 1);

	if(_written == bufsize) {
		_writebuffer.clear();
		emit exporterReady();

	} else if(_written < bufsize && _chunk==0) {
		_chunk = qMin(bufsize - _written, qint64(1024 * 1024));
		_encoder->write(_writebuffer.constData() + _written, _chunk);
	}
}

void FfmpegExporter::shutdownExporter()
{
	_encoder->closeWriteChannel();
}

enum FfmpegAvailable {
	NOT_TESTED,
	FFMPEG_FOUND,
	FFMPEG_NOT_FOUND
};

static FfmpegAvailable _ffmpegAvailable;
bool FfmpegExporter::isFfmpegAvailable()
{
	if(_ffmpegAvailable == NOT_TESTED) {
		QProcess p;
		p.start(FFMPEG_EXECUTABLE);
		_ffmpegAvailable = p.waitForStarted() ? FFMPEG_FOUND : FFMPEG_NOT_FOUND;
		p.waitForFinished();
	}

	return _ffmpegAvailable == FFMPEG_FOUND;
}

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
#ifndef FFMPEGEXPORTER_H
#define FFMPEGEXPORTER_H

#include <QProcess>
#include <QByteArray>

#include "videoexporter.h"

class FfmpegExporter : public VideoExporter
{
	Q_OBJECT
public:
	FfmpegExporter(QObject *parent=0);

	void setFilename(const QString &filename) { _filename = filename; }
	void setSoundtrack(const QString &filename) { _soundtrack = filename; }
	void setFormat(const QString &format) { _format = format; }
	void setVideoCodec(const QString &codec) { _videoCodec = codec; }
	void setAudioCodec(const QString &codec) { _audioCodec = codec; }

	/**
	 * @brief Set video quality
	 *
	 * Quality range is 0 (low) to 3 (very high)
	 * @param quality
	 */
	void setQuality(int quality) { _quality = quality; }

	static bool isFfmpegAvailable();

private slots:
	void processError(QProcess::ProcessError error);
	void bytesWritten(qint64 bytes);

protected:
	void initExporter();
	void writeFrame(const QImage &image);
	void shutdownExporter();

private:
	QProcess *_encoder;

	QString _filename;
	QString _soundtrack;
	QString _format;
	QString _videoCodec;
	QString _audioCodec;
	int _quality;

	QByteArray _writebuffer;
	qint64 _written;
	qint64 _chunk;
};

#endif // FFMPEGEXPORTER_H

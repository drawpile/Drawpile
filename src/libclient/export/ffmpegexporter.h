/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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
#ifndef FFMPEGEXPORTER_H
#define FFMPEGEXPORTER_H

#include <QProcess>
#include <QByteArray>

#include "libclient/export/videoexporter.h"

class FfmpegExporter final : public VideoExporter
{
	Q_OBJECT
public:
	explicit FfmpegExporter(QObject *parent=nullptr);

	//! Set the output filename
	void setFilename(const QString &filename) { m_filename = filename; }

	//! Set custom arguments to use
	//! If not set, the default arguments will be used
	void setCustomArguments(const QString &args) { m_customArguments = args; }

	//! Get the arguments that are always given to the encoder process
	static QStringList getCommonArguments(int fps);

	//! Get the default encoding arguments (which can be replaced by custom user inputted arguments)
	static QStringList getDefaultArguments();

	//! Try to see if ffmpeg is found and executable
	static bool checkIsFfmpegAvailable();

private slots:
	void processError(QProcess::ProcessError error);
	void bytesWritten(qint64 bytes);

protected:
	void initExporter() override;
	void writeFrame(const QImage &image, int repeat) override;
	void shutdownExporter() override;

private:
	QProcess *m_encoder;

	QString m_filename;
	QString m_customArguments;

	QByteArray m_writebuffer;
	qint64 m_written;
	int m_repeats;
};

#endif // FFMPEGEXPORTER_H

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

#ifndef WEBMENCODER_H
#define WEBMENCODER_H

#include <QObject>
#include <QFile>

#include "mkvmuxer/mkvmuxer.h"
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

class QFileMkvWriter : public mkvmuxer::IMkvWriter {
public:
	void setFilename(const QString &filename)
	{
		Q_ASSERT(!m_file.isOpen());
		m_file.setFileName(filename);
	}

	bool open() { return m_file.open(QFile::WriteOnly); }
	void close() { m_file.close(); }
	QString errorString() const { return m_file.errorString(); }

	  // IMkvWriter interface
	mkvmuxer::int64 Position() const override { return m_file.pos(); }

	mkvmuxer::int32 Position(mkvmuxer::int64 position) override {
		// C style convention: return 0 on success
		return m_file.seek(position) ? 0 : -1;
	}

	bool Seekable() const override { return !m_file.isSequential(); }
	mkvmuxer::int32 Write(const void* buffer, mkvmuxer::uint32 length) override;
	void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) override {
		Q_UNUSED(element_id);
		Q_UNUSED(position);
	}

private:
	QFile m_file;
};

class WebmEncoder : public QObject
{
	Q_OBJECT
public:
	explicit WebmEncoder(const QString &filename, QObject *parent = nullptr);
	~WebmEncoder();

signals:
	void encoderError(const QString &message);
	void encoderReady();
	void encoderFinished();

public slots:
	//! Open the exporter. Emits exporterError or exporterReady
	void open();

	//! Initialize the encoder (after it has been opened)
	void start(int width, int height, int fps);

	//! Encode a new frame
	void writeFrame(const QImage &image, int repeat);

	//! Write out any buffered frames and clean up
	void finish();

private:
	bool writeFrames();

	QFileMkvWriter m_writer;
	mkvmuxer::Segment m_segment;
	unsigned long m_videoTrack;

	// VPX Encoder
	vpx_codec_ctx_t m_codec;
	vpx_image_t m_rawFrame;
	int64_t m_timecode;
	int m_fps;

	bool m_initialized;
};

#endif // WEBMENCODER_H

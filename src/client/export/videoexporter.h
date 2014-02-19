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
#ifndef VIDEOEXPORTER_H
#define VIDEOEXPORTER_H

#include <QThread>
#include <QString>
#include <QSize>

class QImage;

class VideoExporter : public QObject
{
	Q_OBJECT
public:
	VideoExporter(QObject *parent);

	/**
	 * @brief Set framerate
	 * @param fps frames per second
	 */
	void setFps(int fps) { Q_ASSERT(fps>0); _fps = fps; }
	int fps() const { return _fps; }

	/**
	 * @brief Can each frame keep their original size
	 *
	 * If variable size is false (as is required by some exporters, such as ffmpeg,)
	 * frames are scaled to framesize() before they are passed to the exporter.
	 * @param vs
	 */
	void setVariableSize(bool vs) { _variablesize = vs; }
	bool isVariableSize() const { return _variablesize; }

	/**
	 * @brief Force output to specific size
	 * @param size
	 */
	void setFrameSize(const QSize &size);

	/**
	 * @brief Get the output frame size
	 *
	 * Note. This is only meaningful if isVariableSize() == false
	 * @return
	 */
	const QSize &framesize() const { return _targetsize; }

	/**
	 * @brief Get current frame
	 * @return framen number
	 */
	int frame() const { return _frame; }

	/**
	 * @brief Get current time
	 * @return time in seconds
	 */
	float time() const { return frame() / float(_fps); }

	void start();

	/**
	 * @brief Add a new frame to the video
	 *
	 * @param image frame content
	 */
	void saveFrame(const QImage &image);

	/**
	 * @brief Stop exporter
	 */
	void finish();

signals:
	//! This signal is emitted when the exporter becomes ready for a new frame
	void exporterReady();

	//! This signal is emitted when an error occurs. The exporter will terminate
	void exporterError(const QString &message);

	//! This signal is emitted after the exporter has shut down normally
	void exporterFinished();

protected:
	void run();
	virtual void initExporter() = 0;
	virtual void writeFrame(const QImage &image) = 0;
	virtual void shutdownExporter() = 0;

private:
	int _fps;
	bool _variablesize;
	int _frame;
	QSize _targetsize;
};

#endif // VIDEOEXPORTER_H

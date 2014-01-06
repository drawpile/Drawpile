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

#include <QString>

class QImage;

class VideoExporter
{
public:
	VideoExporter();
	virtual ~VideoExporter() = default;

	/**
	 * @brief Set framerate
	 * @param fps frames per second
	 */
	void setFps(int fps) { Q_ASSERT(fps>0); _fps = fps; }
	int fps() const { return _fps; }

	/**
	 * @brief Set whether frames should be scaled to the original size
	 * @param scale
	 */
	void setScaleToOriginal(bool scale) { _scaleToOriginal = scale; }
	bool scaleToOriginal() const { return _scaleToOriginal; }

	/**
	 * @brief Add a new frame to the video
	 *
	 * If an error occurs, get the error message with errorString()
	 *
	 * @param frame frame content
	 * @return false on error
	 */
	bool saveFrame(const QImage &image);

	/**
	 * @brief Get the last error message
	 * @return
	 */
	const QString &errorString() const { return _error; }

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

	/**
	 * @brief Finalize video
	 */
	virtual void finish() = 0;

protected:
	void setErrorString(const QString &error) { _error = error; }
	virtual bool writeFrame(const QImage &image) = 0;

private:
	int _fps;
	bool _scaleToOriginal;
	int _frame;
	QSize _originalSize;
	QString _error;
};

#endif // VIDEOEXPORTER_H

// SPDX-License-Identifier: GPL-3.0-or-later

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
	 * If variable size is false, frames are scaled to framesize() before
	 * they are passed to the exporter.
	 *
	 * If the exporter does not support variable size, the size will be
	 * automatically fixed to the size of the first frame.
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
	 * The frame counter is incremented after the frame(s) have been written.
	 *
	 * @param image frame content
	 * @param count number of times to write the frame
	 */
	void saveFrame(const QImage &image, int count);

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
	/**
	 * @brief Initialize the exporter before any images have been fed to it
	 *
	 * exporterReady should be emitted when the exporter is ready to receive images.
	 */
	virtual void initExporter() = 0;

	/**
	 * @brief Export a frame, possible repeated more than once
	 *
	 * Emit exporterReady when the exporter is ready for more images
	 */
	virtual void writeFrame(const QImage &image, int repeat) = 0;

	//! Last frame has been written, shut down the exporter
	virtual void shutdownExporter() = 0;

	/**
	 * @brief Does this exporter support differently sized frames
	 * @return true if exporter can accept images of different sizes
	 */
	virtual bool variableSizeSupported() { return false; }

private:
	int _fps;
	bool _variablesize;
	int _frame;
	QSize _targetsize;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
	class [[maybe_unused]] AbstractVideoExporterMarker : VideoExporter {
		inline bool variableSizeSupported() override { return false; }
	};
}

#endif // VIDEOEXPORTER_H

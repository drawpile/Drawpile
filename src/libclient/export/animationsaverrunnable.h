// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_ANIMATIONSAVERRUNNABLE_H
#define LIBCLIENT_EXPORT_ANIMATIONSAVERRUNNABLE_H
extern "C" {
#include <dpengine/save_enums.h>
}
#include "libclient/drawdance/canvasstate.h"
#include "libclient/export/videoexporthandle.h"
#include <QObject>
#include <QRunnable>
#include <QVector>

/**
 * @brief A runnable for saving the canvas content as an animation in a
 * background thread
 */
class AnimationSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(AnimationSaverRunnable)
public:
	AnimationSaverRunnable(
#ifndef __EMSCRIPTEN__
		const QString &path,
#endif
		int format, int width, int height, int loops,
		const QVector<int> &frameIndexes, double framerate, const QRect &crop,
		bool scaleSmooth, const drawdance::CanvasState &canvasState,
		const QString &ffmpegPath, const QString &encoderKey,
		QObject *parent = nullptr);

	void run() override;

public slots:
	void cancelExport();

signals:
	void progress(int progress);
	void saveComplete(const QString &error, qint64 elapsedMsec);
#ifdef __EMSCRIPTEN__
	void downloadReady(const QString &defaultName, const QByteArray &bytes);
#endif

private:
#ifdef __EMSCRIPTEN__
	QString getFormatExtension() const;
#endif

#ifdef DP_LIBAV
	int formatToSaveVideoFormat() const;
#endif

	QString saveResultToErrorString(DP_SaveResult result) const;

	static bool onProgress(void *user, double progress);

#ifndef __EMSCRIPTEN__
	const QString m_path;
#endif
	const int m_format;
	const int m_width;
	const int m_height;
	const int m_loops;
	const double m_framerate;
	const QRect m_crop;
	const QVector<int> m_frameIndexes;
	const drawdance::CanvasState m_canvasState;
	const QString m_ffmpegPath;
	const QString m_encoderKey;
	const bool m_scaleSmooth;
	bool m_cancelled;
	DP_DECLARE_VIDEO_EXPORT_HANDLE(m_videoExportHandle)
};

#endif

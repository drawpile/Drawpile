// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_ANIMATIONSAVERRUNNABLE_H
#define LIBCLIENT_EXPORT_ANIMATIONSAVERRUNNABLE_H
extern "C" {
#include <dpengine/save_enums.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QObject>
#include <QRunnable>

/**
 * @brief A runnable for saving the canvas content as an animation in a
 * background thread
 */
class AnimationSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	AnimationSaverRunnable(
#ifndef __EMSCRIPTEN__
		const QString &path,
#endif
		int format, int width, int height, int loops, int start, int end,
		int framerate, const QRect &crop, bool scaleSmooth,
		const drawdance::CanvasState &canvasState, QObject *parent = nullptr);

	void run() override;

public slots:
	void cancelExport();

signals:
	void progress(int progress);
	void saveComplete(const QString &error);
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

	static bool onProgress(void *user, double progress);

#ifndef __EMSCRIPTEN__
	const QString m_path;
#endif
	const int m_format;
	const int m_width;
	const int m_height;
	const int m_loops;
	const int m_start;
	const int m_end;
	const int m_framerate;
	const QRect m_crop;
	const drawdance::CanvasState m_canvasState;
	const bool m_scaleSmooth;
	bool m_cancelled;
};

#endif

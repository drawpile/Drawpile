// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANIMATIONSAVERRUNNABLE_H
#define ANIMATIONSAVERRUNNABLE_H

extern "C" {
#include <dpengine/save.h>
}
#include "libclient/drawdance/canvasstate.h"
#include <QObject>
#include <QRunnable>
#include <functional>

/**
 * @brief A runnable for saving the canvas content as an animation in a
 * background thread
 */
class AnimationSaverRunnable final : public QObject, public QRunnable {
	Q_OBJECT
public:
	using SaveFn = std::function<DP_SaveResult(
		DP_CanvasState *, const char *, DP_SaveAnimationProgressFn, void *)>;

	AnimationSaverRunnable(
		const drawdance::CanvasState &canvasState, SaveFn saveFn,
		const QString &filename, QObject *parent = nullptr);

	void run() override;

public slots:
	void cancelExport();

signals:
	void progress(int progress);
	void saveComplete(const QString &error);

private:
	const drawdance::CanvasState m_canvasState;
	QString m_filename;
	SaveFn m_saveFn;
	bool m_cancelled;

	static bool onProgress(void *user, double progress);
};

#endif
